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

  if (!StringCursorAdvanceAfterCRLF(&cursor)) {
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
    StringCursorAdvanceAfter(&cursor, &STRING_FROM_ZERO_TERMINATED(": "));
    struct string headerValue = StringCursorConsumeUntilCRLF(&cursor);
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
