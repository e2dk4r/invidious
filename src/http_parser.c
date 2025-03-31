/*
 * HTTP 1.1 Parser
 * USAGE:
 * <code>
 *   http_parser parser;
 *   HttpParserInit(&parser);
 *   HttpParserMustHaveStatusCode(&parser, 200);
 *   jsmntok_t jsonTokens[128];
 *   u16 jsonTokenCount = 0;
 *   HttpParserMustBeJson(&parser, jsonTokens, ARRAY_COUNT(jsonTokens), &jsonTokenCount)
 *   if HttpParserParse(&parser, response)
 *     print(HttpParserGetError())
 *     exit
 * </code>
 */

#include "assert.h"
#include "json_parser.c"
#include "string_cursor.h"
#include "text.h"
#include "type.h"

enum http_token_type {
  HTTP_TOKEN_NONE,

  /* https://www.rfc-editor.org/rfc/rfc2616#section-6.1 "Status-Line" */

  HTTP_TOKEN_HTTP_VERSION,
  HTTP_TOKEN_STATUS_CODE,
  HTTP_TOKEN_REASON_PHRASE,

  // NOTE: Add headers after
  HTTP_TOKEN_HEADER_FIRSTONE_INTERNAL,

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

  // NOTE: Add headers before
  HTTP_TOKEN_HEADER_LASTONE_INTERNAL,

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
  HTTP_PARSER_ERROR_HTTP_VERSION_INVALID,
  HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1,
  HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
  HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER,
  HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_BETWEEN_100_AND_999,
  HTTP_PARSER_ERROR_REASON_PHRASE_INVALID,
  HTTP_PARSER_ERROR_HEADER_FIELD_NAME_REQUIRED,
  HTTP_PARSER_ERROR_HEADER_FIELD_VALUE_REQUIRED,
  HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID,
  HTTP_PARSER_ERROR_CHUNK_DATA_MALFORMED,
  HTTP_PARSER_ERROR_PARTIAL,
};

enum http_parser_state {
  HTTP_PARSER_STATE_STATUS_LINE_PARSED = (1 << 0),
  HTTP_PARSER_STATE_HEADERS_PARSED = (1 << 1),
};

struct http_parser {
  u16 statusCode;

  enum http_parser_state state;
  enum http_parser_error error;
  u32 tokenCount;
  u32 tokenMax;
  struct http_token *tokens;
};

internalfn void
HttpParserInit(struct http_parser *parser, struct http_token *tokens, u32 tokenCount)
{
  parser->state = 0;
  parser->tokenCount = 0;
  parser->tokenMax = tokenCount;
  parser->tokens = tokens;
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
  struct http_token *tokens = MemoryArenaPushUnaligned(arena, sizeof(*tokens) * tokenCount);
  struct http_parser *parser = MemoryArenaPushUnaligned(arena, sizeof(*parser));
  HttpParserInit(parser, tokens, tokenCount);
  return parser;
}

internalfn b8
IsHttpTokenHeader(struct http_token *token)
{
  return token->type > HTTP_TOKEN_HEADER_FIRSTONE_INTERNAL && token->type < HTTP_TOKEN_HEADER_LASTONE_INTERNAL;
}

internalfn struct string
HttpTokenExtractString(struct http_token *token, struct string *httpResponse)
{
  debug_assert(token->end > token->start);
  return StringFromBuffer(httpResponse->value + token->start, token->end - token->start);
}

internalfn u32
HttpParse(struct http_parser *parser, struct string *httpResponse)
{
  parser->error = HTTP_PARSER_ERROR_NONE;

  struct string *SP = &STRING_FROM_ZERO_TERMINATED(" ");
  struct string *CRLF = &STRING_FROM_ZERO_TERMINATED("\r\n");

  u32 writtenTokenCount = 0;
  struct string_cursor cursor = StringCursorFromString(httpResponse);

  /*****************************************************************
   * Parsing Status-Line
   *****************************************************************/
  b8 isStatusLineParsed = (parser->state & HTTP_PARSER_STATE_STATUS_LINE_PARSED) != 0;
  if (!isStatusLineParsed) {
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
      return 0;
    } else if (!IsStringEqual(&httpVersion, &STRING_FROM_ZERO_TERMINATED("HTTP/1.1"))) {
      parser->error = HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1;
      return 0;
    } else {
      struct http_token *token = parser->tokens + parser->tokenCount + writtenTokenCount;
      token->type = HTTP_TOKEN_HTTP_VERSION;
      token->start = cursor.position;
      token->end = token->start + httpVersion.length;
      writtenTokenCount++;

      // Advance after SP
      cursor.position += httpVersion.length + SP->length;
    }

    struct string statusCodeText = StringCursorExtractUntil(&cursor, SP);
    if (statusCodeText.length != 3) {
      parser->error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID;
      return 0;
    }

    u64 statusCode;
    if (!ParseU64(&statusCodeText, &statusCode)) {
      parser->error = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER;
      return 0;
    } else if (statusCode < 100) {
      parser->error = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_BETWEEN_100_AND_999;
      return 0;
    } else {
      struct http_token *token = parser->tokens + parser->tokenCount + writtenTokenCount;
      token->type = HTTP_TOKEN_STATUS_CODE;
      token->start = cursor.position;
      token->end = token->start + statusCodeText.length;
      writtenTokenCount++;

      // Advance after SP
      cursor.position += statusCodeText.length + SP->length;

      // cache status code
      parser->statusCode = (u16)statusCode;
    }

    struct string reasonPhrase = StringCursorExtractUntil(&cursor, CRLF);
    if (reasonPhrase.length == 0) {
      parser->error = HTTP_PARSER_ERROR_REASON_PHRASE_INVALID;
      return 0;
    }

    // Advance after CRLF
    cursor.position += reasonPhrase.length + CRLF->length;

    parser->state |= HTTP_PARSER_STATE_STATUS_LINE_PARSED;
  }

  if (IsStringCursorAtEnd(&cursor)) {
    parser->error = HTTP_PARSER_ERROR_PARTIAL;
    return writtenTokenCount;
  }

  /*****************************************************************
   * Parsing Headers
   *****************************************************************/
  b8 isHeadersParsed = (parser->state & HTTP_PARSER_STATE_HEADERS_PARSED) != 0;
  if (!isHeadersParsed) {
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
      struct string *colon = &STRING_FROM_ZERO_TERMINATED(":");
      struct string fieldName = StringCursorExtractUntil(&cursor, colon);
      if (fieldName.length == 0) {
        parser->error = HTTP_PARSER_ERROR_HEADER_FIELD_NAME_REQUIRED;
        return 0;
      }

      enum http_token_type tokenType = HTTP_TOKEN_NONE;
      if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("cache-control")))
        tokenType = HTTP_TOKEN_HEADER_CACHE_CONTROL;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("connection")))
        tokenType = HTTP_TOKEN_HEADER_CONNECTION;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("date")))
        tokenType = HTTP_TOKEN_HEADER_DATE;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("pragma")))
        tokenType = HTTP_TOKEN_HEADER_PRAGMA;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("trailer")))
        tokenType = HTTP_TOKEN_HEADER_TRAILER;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("transfer-encoding")))
        tokenType = HTTP_TOKEN_HEADER_TRANSFER_ENCODING;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("upgrade")))
        tokenType = HTTP_TOKEN_HEADER_UPGRADE;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("via")))
        tokenType = HTTP_TOKEN_HEADER_VIA;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("warning")))
        tokenType = HTTP_TOKEN_HEADER_WARNING;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("accept-ranges")))
        tokenType = HTTP_TOKEN_HEADER_ACCEPT_RANGES;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("age")))
        tokenType = HTTP_TOKEN_HEADER_AGE;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("etag")))
        tokenType = HTTP_TOKEN_HEADER_ETAG;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("location")))
        tokenType = HTTP_TOKEN_HEADER_LOCATION;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("proxy-authenticate")))
        tokenType = HTTP_TOKEN_HEADER_PROXY_AUTHENTICATE;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("allow")))
        tokenType = HTTP_TOKEN_HEADER_ALLOW;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("content-encoding")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_ENCODING;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("content-language")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_LANGUAGE;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("content-length")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_LENGTH;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("content-location")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_LOCATION;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("content-md5")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_MD5;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("content-range")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_RANGE;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("content-type")))
        tokenType = HTTP_TOKEN_HEADER_CONTENT_TYPE;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("expires")))
        tokenType = HTTP_TOKEN_HEADER_EXPIRES;
      else if (IsStringEqualIgnoreCase(&fieldName, &STRING_FROM_ZERO_TERMINATED("last-modified")))
        tokenType = HTTP_TOKEN_HEADER_LAST_MODIFIED;

      if (!(tokenType > HTTP_TOKEN_HEADER_FIRSTONE_INTERNAL && tokenType < HTTP_TOKEN_HEADER_LASTONE_INTERNAL)) {
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
        return 0;
      }

      struct http_token *token = parser->tokens + parser->tokenCount + writtenTokenCount;
      token->type = tokenType;
      token->start = cursor.position + ((u64)trimmedFieldValue.value - (u64)fieldValue.value);
      token->end = token->start + trimmedFieldValue.length;
      writtenTokenCount++;

      // Advance after CRLF
      cursor.position += fieldValue.length + CRLF->length;
    }
  }

  /*****************************************************************
   * Parsing Message-Body
   *****************************************************************/
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

    struct string chunkSizeText = StringCursorExtractUntil(&cursor, CRLF);
    struct string *lastChunkSizeText = &STRING_FROM_ZERO_TERMINATED("0");
    if (IsStringEqual(&chunkSizeText, lastChunkSizeText))
      break;

    u64 chunkSize;
    if (!ParseHex(&chunkSizeText, &chunkSize)) {
      parser->error = HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID;
      return 0;
    } else {
      struct http_token *token = parser->tokens + parser->tokenCount + writtenTokenCount;
      token->type = HTTP_TOKEN_CHUNK_SIZE;
      token->start = cursor.position;
      token->end = token->start + chunkSizeText.length;
      writtenTokenCount++;
    }

    // Advance after chunk size
    cursor.position += chunkSizeText.length + CRLF->length;

    struct string chunkData = StringCursorExtractUntil(&cursor, CRLF);
    if (chunkSize != chunkData.length) {
      parser->error = HTTP_PARSER_ERROR_CHUNK_DATA_MALFORMED;
      return 0;
    } else {
      struct http_token *token = parser->tokens + writtenTokenCount;
      token->type = HTTP_TOKEN_CHUNK_DATA;
      token->start = cursor.position;
      token->end = token->start + chunkData.length;
      writtenTokenCount++;
    }

    // Advance after chunk data
    cursor.position += chunkData.length + CRLF->length;
  }

  parser->tokenCount += writtenTokenCount;
  return writtenTokenCount;
}

#if 0
enum http_parser_flag {
  HTTP_PARSER_CHECK_STATUS_CODE_FLAG = (1 << 0),
  HTTP_PARSER_CHECK_CONTENT_TYPE_FLAG = (1 << 1),
};

enum http_parser_error {
  HTTP_RESPONSE_SUCCESS,
  HTTP_RESPONSE_IS_NOT_HTTP_1_1,
  HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER,
  HTTP_RESPONSE_STATUS_CODE_NOT_EXPECTED,
  HTTP_RESPONSE_PARTIAL,
  HTTP_RESPONSE_NOT_CHUNKED_TRANSFER_ENCODED,
  HTTP_RESPONSE_CONTENT_TYPE_NOT_EXPECTED,
  HTTP_RESPONSE_CHUNK_TRANSFER_ENCODING_CHUNK_SIZE_IS_NOT_HEX,
  HTTP_RESPONSE_CHUNK_TRANSFER_ENCODING_CHUNK_DATA_MALFORMED,
};

struct http_parser {
  enum http_parser_flag flags;
  enum http_parser_error error;
  u16 statusCode;
  struct string contentType;
  void *contentParser;
};

internalfn void
HttpParserInit(struct http_parser *parser)
{
  parser->flags = 0;
  parser->error = HTTP_RESPONSE_SUCCESS;
}

internalfn b8
IsHttpParserFlagSet(struct http_parser *parser, enum http_parser_flag flag)
{
  return (parser->flags & flag) != 0;
}

internalfn void
HttpParserFlagSet(struct http_parser *parser, enum http_parser_flag flag)
{
  parser->flags |= flag;
}

internalfn void
HttpParserMustHaveStatusCode(struct http_parser *parser, u16 statusCode)
{
  HttpParserFlagSet(parser, HTTP_PARSER_CHECK_STATUS_CODE_FLAG);
  parser->statusCode = statusCode;
}

internalfn void
HttpParserMustBeJson(struct http_parser *parser, struct json_parser *jsonParser)
{
  HttpParserFlagSet(parser, HTTP_PARSER_CHECK_CONTENT_TYPE_FLAG);
  parser->contentType = STRING_FROM_ZERO_TERMINATED("application/json");
  parser->contentParser = jsonParser;
}

internalfn b8
StringCursorAdvanceAfterCRLF(struct string_cursor *cursor)
{
  return StringCursorAdvanceAfter(cursor, &STRING_FROM_ZERO_TERMINATED("\r\n"));
}

internalfn b8
IsStringCursorRemainingEqualToCRLF(struct string_cursor *cursor)
{
  return IsStringCursorRemainingEqual(cursor, &STRING_FROM_ZERO_TERMINATED("\r\n"));
}

internalfn struct string
StringCursorConsumeUntilCRLF(struct string_cursor *cursor)
{
  return StringCursorConsumeUntil(cursor, &STRING_FROM_ZERO_TERMINATED("\r\n"));
}

internalfn struct string
StringCursorExtractUntilCRLF(struct string_cursor *cursor)
{
  return StringCursorExtractUntil(cursor, &STRING_FROM_ZERO_TERMINATED("\r\n"));
}

internalfn struct string
StringCursorConsumeAfterSpace(struct string_cursor *cursor)
{
  return StringCursorConsumeUntil(cursor, &STRING_FROM_ZERO_TERMINATED(" "));
}

internalfn b8
HttpParserParse(struct http_parser *parser, struct string *httpResponse)
{
  struct string_cursor cursor = StringCursorFromString(httpResponse);

  parser->error = HTTP_RESPONSE_SUCCESS;

  if (IsStringCursorAtEnd(&cursor)) {
    parser->error = HTTP_RESPONSE_PARTIAL;
    return 0;
  }

  /* https://www.rfc-editor.org/rfc/rfc2616#section-6
   * 6 Response
   *
   *    After receiving and interpreting a request message, a server responds
   *    with an HTTP response message.
   *
   *        Response      = Status-Line               ; Section 6.1
   *                        *(( general-header        ; Section 4.5
   *                         | response-header        ; Section 6.2
   *                         | entity-header ) CRLF)  ; Section 7.1
   *                        CRLF
   *                        [ message-body ]          ; Section 7.2
   *
   * https://www.rfc-editor.org/rfc/rfc2616#section-6.1
   * 6.1 Status-Line
   *
   *    The first line of a Response message is the Status-Line, consisting
   *    of the protocol version followed by a numeric status code and its
   *    associated textual phrase, with each element separated by SP
   *    characters. No CR or LF is allowed except in the final CRLF sequence.
   *
   *        Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
   *
   * https://www.rfc-editor.org/rfc/rfc2616#section-3.1
   * 3.1 HTTP Version
   *
   *    The version of an HTTP message is indicated by an HTTP-Version field
   *    in the first line of the message.
   *
   *        HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
   */

  if (!IsStringCursorStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 "))) {
    parser->error = HTTP_RESPONSE_IS_NOT_HTTP_1_1;
    return 0;
  }

  struct string statusCodeText = StringCursorConsumeUntil(&cursor, &STRING_FROM_ZERO_TERMINATED(" "));
  if (statusCodeText.length != 3) {
    parser->error = HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER;
    return 0;
  }
  u64 statusCode;
  if (!ParseU64(&statusCodeText, &statusCode)) {
    parser->error = HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER;
    return 0;
  }
  if (IsHttpParserFlagSet(parser, HTTP_PARSER_CHECK_STATUS_CODE_FLAG) && statusCode != parser->statusCode) {
    parser->error = HTTP_RESPONSE_STATUS_CODE_NOT_EXPECTED;
    return 0;
  }

  if (!StringCursorAdvanceAfter(&cursor, &CRLF)) {
    parser->error = HTTP_RESPONSE_PARTIAL;
    return 0;
  }

  /*
   * https://www.rfc-editor.org/rfc/rfc2616#section-4.5
   * 4.5 General Header Fields
   *        general-header = Cache-Control            ; Section 14.9
   *                       | Connection               ; Section 14.10
   *                       | Date                     ; Section 14.18
   *                       | Pragma                   ; Section 14.32
   *                       | Trailer                  ; Section 14.40
   *                       | Transfer-Encoding        ; Section 14.41
   *                       | Upgrade                  ; Section 14.42
   *                       | Via                      ; Section 14.45
   *                       | Warning                  ; Section 14.46
   *
   * https://www.rfc-editor.org/rfc/rfc2616#section-6.2
   * 6.2 Response Header Fields
   *        response-header = Accept-Ranges           ; Section 14.5
   *                        | Age                     ; Section 14.6
   *                        | ETag                    ; Section 14.19
   *                        | Location                ; Section 14.30
   *                        | Proxy-Authenticate      ; Section 14.33
   *
   * https://www.rfc-editor.org/rfc/rfc2616#section-7.1
   * 7.1 Entity Header Fields
   *        entity-header  = Allow                    ; Section 14.7
   *                       | Content-Encoding         ; Section 14.11
   *                       | Content-Language         ; Section 14.12
   *                       | Content-Length           ; Section 14.13
   *                       | Content-Location         ; Section 14.14
   *                       | Content-MD5              ; Section 14.15
   *                       | Content-Range            ; Section 14.16
   *                       | Content-Type             ; Section 14.17
   *                       | Expires                  ; Section 14.21
   *                       | Last-Modified            ; Section 14.29
   */

  while (1) {
    if (IsStringCursorAtEnd(&cursor)) {
      parser->error = HTTP_RESPONSE_PARTIAL;
      return 0;
    }

    struct string headerField = StringCursorConsumeUntil(&cursor, &STRING_FROM_ZERO_TERMINATED(": "));
    if (headerField.length == 0) {
      parser->error = HTTP_RESPONSE_PARTIAL;
      return 0;
    }

    // StringCursorAdvanceAfter(&cursor, &STRING_FROM_ZERO_TERMINATED(": "));
    struct string headerValue = StringCursorExtractUntil(&cursor, &CRLF);
    StringCursorAdvanceAfterCRLF(&cursor);

    // https://www.rfc-editor.org/rfc/rfc2616#section-14.17
    if (IsStringEqualIgnoreCase(&headerField, &STRING_FROM_ZERO_TERMINATED("Content-Type"))) {
      struct string contentType = headerValue;
      if (IsHttpParserFlagSet(parser, HTTP_PARSER_CHECK_CONTENT_TYPE_FLAG) &&
          !IsStringEqualIgnoreCase(&contentType, &parser->contentType)) {
        parser->error = HTTP_RESPONSE_CONTENT_TYPE_NOT_EXPECTED;
        return 0;
      }
    }

    // https://www.rfc-editor.org/rfc/rfc2616#section-14.41
    if (IsStringEqualIgnoreCase(&headerField, &STRING_FROM_ZERO_TERMINATED("Transfer-Encoding"))) {
      struct string transferEncoding = headerValue;
      if (!IsStringEqualIgnoreCase(&transferEncoding, &STRING_FROM_ZERO_TERMINATED("chunked"))) {
        parser->error = HTTP_RESPONSE_NOT_CHUNKED_TRANSFER_ENCODED;
        return 0;
      }
      break;
    }
  }

  // TODO: check next 2 bytes if it is CRLF.
  // see: https://www.rfc-editor.org/rfc/rfc2616#section-6 "6. Response"

  if (IsStringCursorAtEnd(&cursor) || !StringCursorAdvanceAfterCRLF(&cursor)) {
    parser->error = HTTP_RESPONSE_PARTIAL;
    return 0;
  }

  // message-body
  /* https://www.rfc-editor.org/rfc/rfc2616#section-3.6.1
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
  while (!IsStringCursorAtEnd(&cursor)) {
    struct string chunkSizeText = StringCursorConsumeUntilCRLF(&cursor);
    u64 chunkSize;
    if (!ParseHex(&chunkSizeText, &chunkSize)) {
      parser->error = HTTP_RESPONSE_CHUNK_TRANSFER_ENCODING_CHUNK_SIZE_IS_NOT_HEX;
      return 0;
    }

    if (chunkSize == 0)
      break;

    StringCursorAdvanceAfterCRLF(&cursor);
    struct string chunkData = StringCursorExtractUntilCRLF(&cursor);
    if (chunkData.length != chunkSize) {
      parser->error = HTTP_RESPONSE_CHUNK_TRANSFER_ENCODING_CHUNK_DATA_MALFORMED;
      return 0;
    }

    // chunkData
    debug_assert(IsStringEqual(&parser->contentType, &STRING_FROM_ZERO_TERMINATED("application/json")));
    struct json_parser *jsonParser = parser->contentParser;
    u32 beforeTokenCount = jsonParser->tokenCount;
    u32 parsedTokenCount = JsonParse(jsonParser, &chunkData);

    // TODO: Handle json parse errors
    debug_assert(jsonParser->error == JSON_PARSER_ERROR_NONE || jsonParser->error == JSON_PARSER_ERROR_PARTIAL);

    // Adjust json token positions to where in http response
    // This is because json parser thinks we store json as sequential in memory.
    for (u32 parsedTokenIndex = 0; parsedTokenIndex < parsedTokenCount; parsedTokenIndex++) {
      struct json_token *token = jsonParser->tokens + beforeTokenCount + parsedTokenIndex;
      token->start += cursor.position;
      token->end += cursor.position;
    }

    StringCursorAdvanceAfterCRLF(&cursor);
  }

  return 1;
}

#endif
