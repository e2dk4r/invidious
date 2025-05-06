/*
 * HTTP 1.1 Parser
 *
 * This was implemented in assumption that you use continuous buffer that has
 * HTTP response. As you fill the buffer you continuously call HttpParse() and
 * see the HTTP tokens. If any error happened you can early out.
 *
 * Notes:
 *   - Http headers starts at index 3
 *   - If parser has chunked encoded body and content length body, ignore
 *   content length
 *
 * @code
 *   buf = allocate(length)
 *   position = 0;
 *   while (1) {
 *     readBytes = ssl_read(buf, length - position);
 *     packet = StringFromBuffer(buf + position, readBytes);
 *     position += readBytes;
 *     ok = HttpParse(parser, packet);
 *     if (ok)
 *       break;
 *     if (parser->error != PARTIAL)
 *       print("http error")
 *       exit(1)
 *   }
 * @endcode
 */

#include "assert.h"
#include "string_cursor.h"
#include "text.h"
#include "type.h"

enum http_token_type {
  HTTP_TOKEN_NONE,

  /* https://www.rfc-editor.org/rfc/rfc2616#section-6.1 "Status-Line" */

  HTTP_TOKEN_HTTP_VERSION,
  HTTP_TOKEN_STATUS_CODE,
  HTTP_TOKEN_REASON_PHRASE,

  /* https://www.rfc-editor.org/rfc/rfc2616#section-4.5 "General Header Fields" */

  HTTP_TOKEN_HEADER_CACHE_CONTROL,
  HTTP_TOKEN_HEADER_CONNECTION,
  HTTP_TOKEN_HEADER_DATE,
  HTTP_TOKEN_HEADER_PRAGMA,
  HTTP_TOKEN_HEADER_TRAILER,
  HTTP_TOKEN_HEADER_TRANSFER_ENCODING,
  HTTP_TOKEN_HEADER_UPGRADE,
  HTTP_TOKEN_HEADER_VIA,
  HTTP_TOKEN_HEADER_WARNING,

  /* https://www.rfc-editor.org/rfc/rfc2616#section-6.2 "Response Header Fields" */

  HTTP_TOKEN_HEADER_ACCEPT_RANGES,
  HTTP_TOKEN_HEADER_AGE,
  HTTP_TOKEN_HEADER_ETAG,
  HTTP_TOKEN_HEADER_LOCATION,
  HTTP_TOKEN_HEADER_PROXY_AUTHENTICATE,

  /* https://www.rfc-editor.org/rfc/rfc2616#section-7.1 "Entity Header Fields" */

  HTTP_TOKEN_HEADER_ALLOW,
  HTTP_TOKEN_HEADER_CONTENT_ENCODING,
  HTTP_TOKEN_HEADER_CONTENT_LANGUAGE,
  HTTP_TOKEN_HEADER_CONTENT_LENGTH,
  HTTP_TOKEN_HEADER_CONTENT_LOCATION,
  HTTP_TOKEN_HEADER_CONTENT_MD5,
  HTTP_TOKEN_HEADER_CONTENT_RANGE,
  HTTP_TOKEN_HEADER_CONTENT_TYPE,
  HTTP_TOKEN_HEADER_EXPIRES,
  HTTP_TOKEN_HEADER_LAST_MODIFIED,

  /* Commonly used headers */
  HTTP_TOKEN_HEADER_SERVER,

  HTTP_TOKEN_CONTENT,

  /* https://www.rfc-editor.org/rfc/rfc2616#section-3.6.1 "Chunked Transfer Coding" */
  HTTP_TOKEN_CHUNK_SIZE,
  HTTP_TOKEN_CHUNK_DATA,
};

struct http_token {
  enum http_token_type type;
  u64 start;
  u64 end;
};

enum http_parser_error {
  HTTP_PARSER_ERROR_NONE,
  HTTP_PARSER_ERROR_OUT_OF_MEMORY,
  HTTP_PARSER_ERROR_HTTP_VERSION_INVALID,
  HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1,
  HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
  HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER,
  HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_BETWEEN_100_AND_999,
  HTTP_PARSER_ERROR_REASON_PHRASE_INVALID,
  HTTP_PARSER_ERROR_HEADER_FIELD_NAME_REQUIRED,
  HTTP_PARSER_ERROR_HEADER_FIELD_VALUE_REQUIRED,
  HTTP_PARSER_ERROR_CONTENT_LENGTH_EXPECTED_POSITIVE_NUMBER,
  HTTP_PARSER_ERROR_UNSUPPORTED_TRANSFER_ENCODING,
  HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID,
  HTTP_PARSER_ERROR_CHUNK_DATA_MALFORMED,
  HTTP_PARSER_ERROR_CONTENT_INVALID_LENGTH,
  HTTP_PARSER_ERROR_PARTIAL,
};

enum http_parser_state {
  HTTP_PARSER_STATE_STATUS_LINE_PARSED = (1 << 0),
  HTTP_PARSER_STATE_HEADERS_PARSED = (1 << 1),
  HTTP_PARSER_STATE_HAS_CONTENT_LENGTH_BODY = (1 << 2),
  HTTP_PARSER_STATE_HAS_CHUNKED_ENCODED_BODY = (1 << 3),
};

struct http_parser {
  u16 statusCode;
  u64 contentLength;

  // TODO: Can be replaced by looping tokens.
  enum http_parser_state state;
  enum http_parser_error error;

  u32 tokenCount;
  u32 tokenMax;
  struct http_token *tokens;

  // last read position from buffer
  u64 position;
};

internalfn void
HttpParserInit(struct http_parser *parser, struct http_token *tokens, u32 tokenCount)
{
  parser->state = 0;
  parser->tokenCount = 0;
  parser->tokenMax = tokenCount;
  parser->tokens = tokens;
  parser->position = 0;
}

internalfn struct http_parser
HttpParser(struct http_token *tokens, u32 tokenCount)
{
  struct http_parser parser;
  HttpParserInit(&parser, tokens, tokenCount);
  return parser;
}

internalfn struct http_parser *
MakeHttpParser(memory_arena *arena, u32 tokenCount)
{
  struct http_token *tokens = MemoryArenaPush(arena, sizeof(*tokens) * tokenCount);
  struct http_parser *parser = MemoryArenaPush(arena, sizeof(*parser));
  HttpParserInit(parser, tokens, tokenCount);
  return parser;
}

internalfn struct string
HttpTokenExtractString(struct http_token *token, struct string *httpResponse)
{
  debug_assert(token->end > token->start);
  return StringFromBuffer(httpResponse->value + token->start, token->end - token->start);
}

internalfn b8
HttpParse(struct http_parser *parser, struct string *httpResponse)
{
  parser->error = HTTP_PARSER_ERROR_NONE;

  struct string *SP = &StringFromLiteral(" ");
  struct string *CRLF = &StringFromLiteral("\r\n");

  u32 writtenTokenCount = 0;
  struct string_cursor cursor = StringCursorFromString(httpResponse);

  /*****************************************************************
   * Parsing Status-Line
   *****************************************************************/
  b8 isStatusLineNotParsed = (parser->state & HTTP_PARSER_STATE_STATUS_LINE_PARSED) == 0;
  if (isStatusLineNotParsed) {
    /* https://www.rfc-editor.org/rfc/rfc2616#section-6.1 "Status-Line"
     *   The first line of a Response message is the Status-Line, consisting
     *   of the protocol version followed by a numeric status code and its
     *   associated textual phrase, with each element separated by SP
     *   characters. No CR or LF is allowed except in the final CRLF sequence.
     *
     *     Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
     *
     * https://www.rfc-editor.org/rfc/rfc2616#section-3.1 "HTTP Version"
     *
     *   The version of an HTTP message is indicated by an HTTP-Version field
     *   in the first line of the message.
     *
     *     HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
     */

    struct string httpVersion = StringCursorExtractUntil(&cursor, SP);
    if (httpVersion.length != 8) {
      parser->error = HTTP_PARSER_ERROR_HTTP_VERSION_INVALID;
      goto end;
    } else if (!IsStringEqual(&httpVersion, &StringFromLiteral("HTTP/1.1"))) {
      parser->error = HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1;
      goto end;
    } else {
      u32 tokenIndex = parser->tokenCount + writtenTokenCount;
      if (tokenIndex == parser->tokenMax) {
        parser->error = HTTP_PARSER_ERROR_OUT_OF_MEMORY;
        goto end;
      }
      struct http_token *token = parser->tokens + tokenIndex;

      token->type = HTTP_TOKEN_HTTP_VERSION;
      token->start = cursor.position;
      token->end = token->start + httpVersion.length;
      token->start += parser->position;
      token->end += parser->position;
      writtenTokenCount++;

      // Advance after SP
      cursor.position += httpVersion.length + SP->length;
    }

    struct string statusCodeText = StringCursorExtractUntil(&cursor, SP);
    if (statusCodeText.length != 3) {
      parser->error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID;
      goto end;
    }

    u64 statusCode;
    if (!ParseU64(&statusCodeText, &statusCode)) {
      parser->error = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER;
      goto end;
    } else if (statusCode < 100) {
      parser->error = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_BETWEEN_100_AND_999;
      goto end;
    } else {
      u32 tokenIndex = parser->tokenCount + writtenTokenCount;
      if (tokenIndex == parser->tokenMax) {
        parser->error = HTTP_PARSER_ERROR_OUT_OF_MEMORY;
        goto end;
      }
      struct http_token *token = parser->tokens + tokenIndex;

      token->type = HTTP_TOKEN_STATUS_CODE;
      token->start = cursor.position;
      token->end = token->start + statusCodeText.length;
      token->start += parser->position;
      token->end += parser->position;
      writtenTokenCount++;

      // Advance after SP
      cursor.position += statusCodeText.length + SP->length;

      // cache status code
      parser->statusCode = (u16)statusCode;
    }

    struct string reasonPhrase = StringCursorExtractUntil(&cursor, CRLF);
    if (reasonPhrase.length == 0) {
      parser->error = HTTP_PARSER_ERROR_REASON_PHRASE_INVALID;
      goto end;
    }

    // Advance after CRLF
    cursor.position += reasonPhrase.length + CRLF->length;

    parser->state |= HTTP_PARSER_STATE_STATUS_LINE_PARSED;
  }

  if (IsStringCursorAtEnd(&cursor)) {
    parser->error = HTTP_PARSER_ERROR_PARTIAL;
    goto end;
  }

  /*****************************************************************
   * Parsing Headers
   *****************************************************************/
  b8 isHeadersNotParsed = (parser->state & HTTP_PARSER_STATE_HEADERS_PARSED) == 0;
  if (isHeadersNotParsed) {
    while (!IsStringCursorAtEnd(&cursor)) {
      if (StringCursorPeekStartsWith(&cursor, CRLF)) {
        parser->state |= HTTP_PARSER_STATE_HEADERS_PARSED;
        cursor.position += CRLF->length;
        break;
      }

      /* https://www.rfc-editor.org/rfc/rfc2616#section-4.2 "Message Headers"
       *   message-header = field-name ":" [ field-value ]
       *   field-name     = token
       *   field-value    = *( field-content | LWS )
       *   field-content  = <the OCTETs making up the field-value
       *                    and consisting of either *TEXT or combinations
       *                    of token, separators, and quoted-string>
       */
      struct string *colon = &StringFromLiteral(":");
      struct string fieldName = StringCursorExtractUntil(&cursor, colon);
      if (fieldName.length == 0) {
        parser->error = HTTP_PARSER_ERROR_HEADER_FIELD_NAME_REQUIRED;
        goto end;
      }

      enum http_token_type tokenType = HTTP_TOKEN_NONE;
      if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("cache-control")))
        tokenType = HTTP_TOKEN_HEADER_CACHE_CONTROL;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("connection")))
        tokenType = HTTP_TOKEN_HEADER_CONNECTION;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("date")))
        tokenType = HTTP_TOKEN_HEADER_DATE;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("pragma")))
        tokenType = HTTP_TOKEN_HEADER_PRAGMA;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("trailer")))
        tokenType = HTTP_TOKEN_HEADER_TRAILER;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("transfer-encoding")))
        tokenType = HTTP_TOKEN_HEADER_TRANSFER_ENCODING;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("upgrade")))
        tokenType = HTTP_TOKEN_HEADER_UPGRADE;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("via")))
        tokenType = HTTP_TOKEN_HEADER_VIA;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("warning")))
        tokenType = HTTP_TOKEN_HEADER_WARNING;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("accept-ranges")))
        tokenType = HTTP_TOKEN_HEADER_ACCEPT_RANGES;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("age")))
        tokenType = HTTP_TOKEN_HEADER_AGE;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("etag")))
        tokenType = HTTP_TOKEN_HEADER_ETAG;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("location")))
        tokenType = HTTP_TOKEN_HEADER_LOCATION;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("proxy-authenticate")))
        tokenType = HTTP_TOKEN_HEADER_PROXY_AUTHENTICATE;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("allow")))
        tokenType = HTTP_TOKEN_HEADER_ALLOW;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("content-encoding")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_ENCODING;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("content-language")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_LANGUAGE;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("content-length")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_LENGTH;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("content-location")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_LOCATION;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("content-md5")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_MD5;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("content-range")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_RANGE;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("content-type")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_TYPE;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("expires")))
        tokenType = HTTP_TOKEN_HEADER_EXPIRES;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("last-modified")))
        tokenType = HTTP_TOKEN_HEADER_LAST_MODIFIED;
      else if (IsStringEqualIgnoreCase(&fieldName, &StringFromLiteral("server")))
        tokenType = HTTP_TOKEN_HEADER_SERVER;

      if (tokenType == HTTP_TOKEN_NONE) {
        // Pass unrecognized http header fields
        StringCursorAdvanceAfter(&cursor, CRLF);
        continue;
      }

      // Advance after colon (":")
      cursor.position += fieldName.length + colon->length;

      struct string fieldValue = StringCursorExtractUntil(&cursor, CRLF);
      struct string trimmedFieldValue = StringStripWhitespace(&fieldValue);
      if (trimmedFieldValue.length == 0) {
        parser->error = HTTP_PARSER_ERROR_HEADER_FIELD_VALUE_REQUIRED;
        goto end;
      }

      if (tokenType == HTTP_TOKEN_HEADER_TRANSFER_ENCODING &&
          !IsStringEqual(&trimmedFieldValue, &StringFromLiteral("chunked"))) {
        parser->error = HTTP_PARSER_ERROR_UNSUPPORTED_TRANSFER_ENCODING;
        goto end;
      }

      if (tokenType == HTTP_TOKEN_HEADER_TRANSFER_ENCODING) {
        parser->state |= HTTP_PARSER_STATE_HAS_CHUNKED_ENCODED_BODY;
      }

      if (tokenType == HTTP_TOKEN_HEADER_CONTENT_LENGTH) {
        parser->state |= HTTP_PARSER_STATE_HAS_CONTENT_LENGTH_BODY;
        u64 contentLength;
        if (!ParseU64(&trimmedFieldValue, &contentLength)) {
          parser->error = HTTP_PARSER_ERROR_CONTENT_LENGTH_EXPECTED_POSITIVE_NUMBER;
          goto end;
        }

        parser->contentLength = contentLength;
      }

      u32 tokenIndex = parser->tokenCount + writtenTokenCount;
      if (tokenIndex == parser->tokenMax) {
        parser->error = HTTP_PARSER_ERROR_OUT_OF_MEMORY;
        goto end;
      }
      struct http_token *token = parser->tokens + tokenIndex;

      token->type = tokenType;
      token->start = cursor.position + ((u64)trimmedFieldValue.value - (u64)fieldValue.value);
      token->end = token->start + trimmedFieldValue.length;
      token->start += parser->position;
      token->end += parser->position;
      writtenTokenCount++;

      // Advance after CRLF
      cursor.position += fieldValue.length + CRLF->length;
    }
  }

  if (IsStringCursorAtEnd(&cursor)) {
    parser->error = HTTP_PARSER_ERROR_PARTIAL;
    goto end;
  }

  /*****************************************************************
   * Parsing Message-Body
   *****************************************************************/
  /* https://www.rfc-editor.org/rfc/rfc2616#section-4.3 "Message Body"
   * ...
   * The presence of a message-body in a request is signaled by the
   * inclusion of a Content-Length or Transfer-Encoding header field in
   * the request's message-headers.
   * ...
   * All 1xx (informational), 204 (no content), and 304 (not modified)
   * responses MUST NOT include a message-body. All other responses do include
   * a message-body, although it MAY be of zero length.
   */
  if ((parser->statusCode >= 100 && parser->statusCode <= 199) && parser->statusCode == 204 &&
      parser->statusCode == 304 &&
      (parser->state & (HTTP_PARSER_STATE_HAS_CHUNKED_ENCODED_BODY | HTTP_PARSER_STATE_HAS_CONTENT_LENGTH_BODY)) == 0)
    goto end;

  /* https://www.rfc-editor.org/rfc/rfc2616#section-4.4 "Message Length"
   * ...
   * Messages MUST NOT include both a Content-Length header field and a
   * non-identity transfer-coding. If the message does include a non-
   * identity transfer-coding, the Content-Length MUST be ignored.
   */

  // Parse chunked encoded body
  if (parser->state & HTTP_PARSER_STATE_HAS_CHUNKED_ENCODED_BODY) {
    struct http_token *partialToken = 0;
    {
      struct http_token *lastToken = parser->tokens + parser->tokenCount - 1;
      if (lastToken->type == HTTP_TOKEN_CHUNK_DATA && lastToken->end == 0)
        partialToken = lastToken;
    }

    while (!IsStringCursorAtEnd(&cursor)) {
      /* https://www.rfc-editor.org/rfc/rfc2616#section-3.6.1 "Chunked Transfer Coding"
       *   Chunked-Body   = *chunk
       *                    last-chunk
       *                    trailer
       *                    CRLF
       *
       *   chunk          = chunk-size [ chunk-extension ] CRLF
       *                    chunk-data CRLF
       *   chunk-size     = 1*HEX
       *   last-chunk     = 1*("0") [ chunk-extension ] CRLF
       *
       *   chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
       *   chunk-ext-name = token
       *   chunk-ext-val  = token | quoted-string
       *   chunk-data     = chunk-size(OCTET)
       *   trailer        = *(entity-header CRLF)
       */

      if (!partialToken) {
        // Extract chunk size
        struct string chunkSizeText = StringCursorExtractUntil(&cursor, CRLF);
        struct string *lastChunkSizeText = &StringFromLiteral("0");
        if (IsStringEqual(&chunkSizeText, lastChunkSizeText) &&
            // and CRLF must be found
            chunkSizeText.length != StringCursorRemainingLength(&cursor)) {
          // finished
          goto end;
        }

        u64 chunkSize;
        if (!ParseHex(&chunkSizeText, &chunkSize)) {
          parser->error = HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID;
          goto end;
        } else {
          u32 tokenIndex = parser->tokenCount + writtenTokenCount;
          if (tokenIndex == parser->tokenMax) {
            parser->error = HTTP_PARSER_ERROR_OUT_OF_MEMORY;
            goto end;
          }
          struct http_token *token = parser->tokens + tokenIndex;

          token->type = HTTP_TOKEN_CHUNK_SIZE;
          token->start = cursor.position;
          token->end = token->start + chunkSizeText.length;
          token->start += parser->position;
          token->end += parser->position;
          writtenTokenCount++;
        }

        // Advance after chunk size
        cursor.position += chunkSizeText.length + CRLF->length;
      }

      // Extract chunk data
      struct string chunkData = StringCursorExtractUntil(&cursor, CRLF);
      struct http_token *token = partialToken;
      if (!token) {
        // If it is not partial chunk data, create new
        u32 tokenIndex = parser->tokenCount + writtenTokenCount;
        if (tokenIndex == parser->tokenMax) {
          parser->error = HTTP_PARSER_ERROR_OUT_OF_MEMORY;
          goto end;
        }
        token = parser->tokens + tokenIndex;

        token->type = HTTP_TOKEN_CHUNK_DATA;
        token->start = cursor.position;
        token->start += parser->position;

        writtenTokenCount++;
      }
      if (chunkData.length == StringCursorRemainingLength(&cursor)) {
        token->end = 0;
        parser->error = HTTP_PARSER_ERROR_PARTIAL;
        cursor.position += chunkData.length;
        goto end;
      }
      partialToken = 0;

      token->end = cursor.position + chunkData.length;
      token->end += parser->position;

      // Advance after chunk data
      cursor.position += chunkData.length + CRLF->length;
    }
  }
  // Parse content-length limited body
  else if (parser->state & HTTP_PARSER_STATE_HAS_CONTENT_LENGTH_BODY) {
    u64 contentLength = parser->contentLength;

    struct http_token *partialToken = 0;
    {
      struct http_token *lastToken = parser->tokens + parser->tokenCount - 1;
      if (lastToken->type == HTTP_TOKEN_CONTENT && lastToken->end == 0)
        partialToken = lastToken;
    }

    while (!IsStringCursorAtEnd(&cursor)) {
      // Extract content
      struct string content = StringCursorExtractRemaining(&cursor);

      struct http_token *token = partialToken;
      if (!token) {
        // If it is not partial chunk data, create new
        u32 tokenIndex = parser->tokenCount + writtenTokenCount;
        if (tokenIndex == parser->tokenMax) {
          parser->error = HTTP_PARSER_ERROR_OUT_OF_MEMORY;
          goto end;
        }
        token = parser->tokens + tokenIndex;

        token->type = HTTP_TOKEN_CONTENT;
        token->start = cursor.position;
        token->start += parser->position;

        writtenTokenCount++;
      }
      u64 end = cursor.position + content.length;
      end += parser->position;
      if (end - token->start != contentLength) {
        token->end = 0;
        parser->error = HTTP_PARSER_ERROR_PARTIAL;
        cursor.position += content.length;
        goto end;
      }

      partialToken = 0;

      token->end = end;

      // Advance after content
      cursor.position += content.length;
      goto end;
    }
  }

  parser->error = HTTP_PARSER_ERROR_PARTIAL;

end:
  parser->tokenCount += writtenTokenCount;
  parser->position += cursor.position;
  return parser->error == HTTP_PARSER_ERROR_NONE;
}
