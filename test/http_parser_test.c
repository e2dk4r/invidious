#include "http_parser.c"
#include "print.h"
#include "string_builder.h"

#define TEST_ERROR_LIST(XX)                                                                                            \
  XX(PARSE_EXPECTED_TRUE, "HTTP response must be parsed successfully")                                                 \
  XX(PARSE_EXPECTED_FALSE, "HTTP parser must fail to parse HTTP response")

enum http_parser_test_error {
  HTTP_PARSER_TEST_ERROR_NONE = 0,
#define XX(tag, message) HTTP_PARSER_TEST_ERROR_##tag,
  TEST_ERROR_LIST(XX)
#undef XX

  // src: https://mesonbuild.com/Unit-tests.html#skipped-tests-and-hard-errors
  // For the default exitcode testing protocol, the GNU standard approach in
  // this case is to exit the program with error code 77. Meson will detect this
  // and report these tests as skipped rather than failed. This behavior was
  // added in version 0.37.0.
  MESON_TEST_SKIP = 77,
  // In addition, sometimes a test fails set up so that it should fail even if
  // it is marked as an expected failure. The GNU standard approach in this case
  // is to exit the program with error code 99. Again, Meson will detect this
  // and report these tests as ERROR, ignoring the setting of should_fail. This
  // behavior was added in version 0.50.0.
  MESON_TEST_FAILED_TO_SET_UP = 99,
};

comptime struct http_parser_test_error_info {
  enum http_parser_test_error code;
  struct string message;
} TEXT_TEST_ERRORS[] = {
#define XX(tag, msg) {.code = HTTP_PARSER_TEST_ERROR_##tag, .message = StringFromLiteral(msg)},
    TEST_ERROR_LIST(XX)
#undef XX
};

internalfn string *
GetHttpParserTestErrorMessage(enum http_parser_test_error errorCode)
{
  for (u32 index = 0; index < ARRAY_COUNT(TEXT_TEST_ERRORS); index++) {
    const struct http_parser_test_error_info *info = TEXT_TEST_ERRORS + index;
    if (info->code == errorCode)
      return (struct string *)&info->message;
  }
  return 0;
}

internalfn void
StringBuilderAppendHttpParserError(string_builder *sb, enum http_parser_error error)
{
  struct string httpParserErrorTexts[] = {
      [HTTP_PARSER_ERROR_NONE] = StringFromLiteral("None"),
      [HTTP_PARSER_ERROR_OUT_OF_MEMORY] = StringFromLiteral("Out of memory"),
      [HTTP_PARSER_ERROR_HTTP_VERSION_INVALID] = StringFromLiteral("HTTP version is invalid"),
      [HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1] = StringFromLiteral("HTTP version 1.1 is expected"),
      [HTTP_PARSER_ERROR_STATUS_CODE_INVALID] = StringFromLiteral("Status code is invalid"),
      [HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER] =
          StringFromLiteral("Status code must be 3-digit integer"),
      [HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_BETWEEN_100_AND_999] =
          StringFromLiteral("Status code must be [100, 999]"),
      [HTTP_PARSER_ERROR_REASON_PHRASE_INVALID] = StringFromLiteral("Reason phrase is invalid"),
      [HTTP_PARSER_ERROR_HEADER_FIELD_NAME_REQUIRED] = StringFromLiteral("Header field name is required"),
      [HTTP_PARSER_ERROR_HEADER_FIELD_VALUE_REQUIRED] = StringFromLiteral("Header field value is required"),
      [HTTP_PARSER_ERROR_CONTENT_LENGTH_EXPECTED_POSITIVE_NUMBER] =
          StringFromLiteral("Content length must be positive number"),
      [HTTP_PARSER_ERROR_UNSUPPORTED_TRANSFER_ENCODING] = StringFromLiteral("Unsupported transfer encoding"),
      [HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID] = StringFromLiteral("Chunk size is invalid"),
      [HTTP_PARSER_ERROR_CHUNK_DATA_MALFORMED] = StringFromLiteral("Chunk data is malformed"),
      [HTTP_PARSER_ERROR_CONTENT_INVALID_LENGTH] = StringFromLiteral("Content has invalid length"),
      [HTTP_PARSER_ERROR_PARTIAL] = StringFromLiteral("Partial response"),
  };
  struct string *httpParserErrorText = httpParserErrorTexts + (u32)error;
  StringBuilderAppendString(sb, httpParserErrorText);
}

internalfn void
StringBuilderAppendHttpTokenType(string_builder *sb, enum http_token_type type)
{
  struct string table[] = {
      [HTTP_TOKEN_NONE] = StringFromLiteral("None"),

      [HTTP_TOKEN_HTTP_VERSION] = StringFromLiteral("HTTP version"),
      [HTTP_TOKEN_STATUS_CODE] = StringFromLiteral("Status code"),
      [HTTP_TOKEN_REASON_PHRASE] = StringFromLiteral("Reason Phrase"),

      [HTTP_TOKEN_HEADER_CACHE_CONTROL] = StringFromLiteral("Cache-Control"),
      [HTTP_TOKEN_HEADER_CONNECTION] = StringFromLiteral("Connection"),
      [HTTP_TOKEN_HEADER_DATE] = StringFromLiteral("Date"),
      [HTTP_TOKEN_HEADER_PRAGMA] = StringFromLiteral("Pragma"),
      [HTTP_TOKEN_HEADER_TRAILER] = StringFromLiteral("Trailer"),
      [HTTP_TOKEN_HEADER_TRANSFER_ENCODING] = StringFromLiteral("Transfer-Encoding"),
      [HTTP_TOKEN_HEADER_UPGRADE] = StringFromLiteral("Upgrade"),
      [HTTP_TOKEN_HEADER_VIA] = StringFromLiteral("Via"),
      [HTTP_TOKEN_HEADER_WARNING] = StringFromLiteral("Warning"),

      [HTTP_TOKEN_HEADER_ACCEPT_RANGES] = StringFromLiteral("Accept-Ranges"),
      [HTTP_TOKEN_HEADER_AGE] = StringFromLiteral("Age"),
      [HTTP_TOKEN_HEADER_ETAG] = StringFromLiteral("Etag"),
      [HTTP_TOKEN_HEADER_LOCATION] = StringFromLiteral("Location"),
      [HTTP_TOKEN_HEADER_PROXY_AUTHENTICATE] = StringFromLiteral("Proxy-Authenticate"),

      [HTTP_TOKEN_HEADER_ALLOW] = StringFromLiteral("Allow"),
      [HTTP_TOKEN_HEADER_CONTENT_ENCODING] = StringFromLiteral("Content-Encoding"),
      [HTTP_TOKEN_HEADER_CONTENT_LANGUAGE] = StringFromLiteral("Content-Language"),
      [HTTP_TOKEN_HEADER_CONTENT_LENGTH] = StringFromLiteral("Content-Length"),
      [HTTP_TOKEN_HEADER_CONTENT_LOCATION] = StringFromLiteral("Content-Location"),
      [HTTP_TOKEN_HEADER_CONTENT_MD5] = StringFromLiteral("Content-Md5"),
      [HTTP_TOKEN_HEADER_CONTENT_RANGE] = StringFromLiteral("Content-Range"),
      [HTTP_TOKEN_HEADER_CONTENT_TYPE] = StringFromLiteral("Content-Type"),
      [HTTP_TOKEN_HEADER_EXPIRES] = StringFromLiteral("Expires"),
      [HTTP_TOKEN_HEADER_LAST_MODIFIED] = StringFromLiteral("Last-Modified"),

      [HTTP_TOKEN_HEADER_SERVER] = StringFromLiteral("Server"),

      [HTTP_TOKEN_CHUNK_SIZE] = StringFromLiteral("Chunk size"),
      [HTTP_TOKEN_CHUNK_DATA] = StringFromLiteral("Chunk data"),
  };
  struct string *string = table + (u32)type;
  StringBuilderAppendString(sb, string);
}

internalfn void
StringBuilderAppendBool(string_builder *sb, b8 value)
{
  struct string *boolString = value ? &StringFromLiteral("true") : &StringFromLiteral("false");
  StringBuilderAppendString(sb, boolString);
}

internalfn void
StringBuilderAppendPrintableString(string_builder *sb, struct string *string)
{
  if (string->value == 0)
    StringBuilderAppendStringLiteral(sb, "(NULL)");
  else if (string->value != 0 && string->length == 0)
    StringBuilderAppendStringLiteral(sb, "(EMPTY)");
  else if (string->length == 1 && string->value[0] == ' ')
    StringBuilderAppendStringLiteral(sb, "(SPACE)");
  else
    StringBuilderAppendString(sb, string);
}

internalfn void
StringBuilderAppendPrintableHexDump(string_builder *sb, struct string *string)
{
  if (string->value == 0) {
    StringBuilderAppendStringLiteral(sb, "(NULL)");
    return;
  } else if (string->value != 0 && string->length == 0) {
    StringBuilderAppendStringLiteral(sb, "(EMPTY)");
    return;
  }
  StringBuilderAppendHexDump(sb, string);
}

int
main(void)
{
  enum http_parser_test_error errorCode = HTTP_PARSER_TEST_ERROR_NONE;

  // setup
  u32 KILOBYTES = 1 << 10;
  u32 MEGABYTES = 1 << 20;
  u8 stackBuffer[1 * MEGABYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 128 * KILOBYTES, 32);

  // b8 HttpParserParse(struct http_parser *parser, struct string *httpResponse)
  {
    u32 maxHttpTokenCount = 128;

    struct test_case {
      struct string *httpResponse;
      struct {
        enum http_parser_error error;
        u32 httpTokenCount;
        struct http_token *httpTokens;
        struct string *httpTokenStrings;
        struct string *content;
      } expected;
    } testCases[] = {
#define CRLF "\r\n"
        {
            .httpResponse = &StringFromLiteral(""),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_HTTP_VERSION_INVALID,
                },
        },
        {
            .httpResponse = &StringFromLiteral("HTTP/1.0 "),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1,
                },
        },
        {
            .httpResponse = &StringFromLiteral("HTTP/2.0 "),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1,
                },
        },
        {
            .httpResponse = &StringFromLiteral("HTTP/1.1 ABC "),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER,
                },
        },
        {
            .httpResponse = &StringFromLiteral("HTTP/1.1 1 "),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
                },
        },
        {
            .httpResponse = &StringFromLiteral("HTTP/1.1 10 "),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
                },
        },
        {
            .httpResponse = &StringFromLiteral("HTTP/1.1 1000 Custom"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
                },
        },
        {
            .httpResponse = &StringFromLiteral("HTTP/1.1 200 " CRLF),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_REASON_PHRASE_INVALID,
                },
        },
        {
            .httpResponse = &StringFromLiteral(
                /*** --- Status-Line -------------------------------- ***/
                "HTTP/1.1 200 OK" CRLF
                /*** --- Header Fields ------------------------------ ***/
                /**/ "Content-Type: application/json" CRLF
                /**/ "Transfer-Encoding: chunked" CRLF
                /**/ CRLF
                /*** --- Message Body ------------------------------- ***/
                //// first chunk size (format: hex digits)
                /**/ "THIS IS NOT HEX" CRLF
                //// first chunk data
                /**/ "[ 1, 2, 3 ]" CRLF
                //// Last chunk
                /**/ "0" CRLF CRLF),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID,
                },
        },
    // TODO: Do http parser checks chunk data length
#if 0
        {
            .httpResponse = &StringFromLiteral(
                /*** --- Status-Line -------------------------------- ***/
                "HTTP/1.1 200 OK" CRLF
                /*** --- Header Fields ------------------------------ ***/
                /**/ "Content-Type: application/json" CRLF
                /**/ "Transfer-Encoding: chunked" CRLF
                /**/ CRLF
                /*** --- Message Body ------------------------------- ***/
                //// first chunk size (format: hex digits)
                /**/ "9" CRLF
                //// first chunk data
                /**/ "[ 1, 2, 3 ]" CRLF
                //// Last chunk
                /**/ "0" CRLF CRLF),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_CHUNK_DATA_MALFORMED,
                },
        },
#endif
        {
            .httpResponse = &StringFromLiteral(
                /*** --- Status-Line -------------------------------- ***/
                "HTTP/1.1 200 OK" CRLF
                /*** --- Header Fields ------------------------------ ***/
                /**/ "Content-Type: application/json" CRLF
                /**/ "Content-Length: 90" CRLF
                /**/ CRLF
                /*** --- Message Body ------------------------------- ***/
                /**/ "[ 1, 2, 3 ]"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_PARTIAL,
                },
        },
        {
            .httpResponse = &StringFromLiteral(
                /*** --- Status-Line -------------------------------- ***/
                "HTTP/1.1 200 OK" CRLF
                /*** --- Header Fields ------------------------------ ***/
                /**/ "Content-Type: application/json" CRLF
                /**/ "Content-Length: 11" CRLF
                /**/ CRLF
                /*** --- Message Body ------------------------------- ***/
                /**/ "[ 1, 2, 3 ]"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_NONE,
                    .httpTokenCount = 5,
                    .httpTokens =
                        // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                        // fields
                    (struct http_token[]){
                        {.type = HTTP_TOKEN_HTTP_VERSION, .start = 0x0, .end = 0x8},
                        {.type = HTTP_TOKEN_STATUS_CODE, .start = 0x9, .end = 0xc},
                        {.type = HTTP_TOKEN_HEADER_CONTENT_TYPE, .start = 0x1f, .end = 0x2f},
                        {.type = HTTP_TOKEN_HEADER_CONTENT_LENGTH, .start = 0x41, .end = 0x43},
                        {.type = HTTP_TOKEN_CONTENT, .start = 0x47, .end = 0x52},
                        // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                        // fields
                    },
                    .httpTokenStrings =
                        (struct string[]){
                            StringFromLiteral("HTTP/1.1"),
                            StringFromLiteral("200"),
                            StringFromLiteral("application/json"),
                            StringFromLiteral("11"),
                            StringFromLiteral("[ 1, 2, 3 ]"),
                        },
                    .content = &StringFromLiteral("[ 1, 2, 3 ]"),
                },
        },
        {
            .httpResponse = &StringFromLiteral(
                /*** --- Status-Line -------------------------------- ***/
                "HTTP/1.1 200 OK" CRLF
                /*** --- Header Fields ------------------------------ ***/
                /**/ "Content-Type: application/json" CRLF
                /**/ "Transfer-Encoding: chunked" CRLF
                /**/ CRLF
                /*** --- Message Body ------------------------------- ***/
                //// first chunk size (format: hex digits)
                /**/ "b" CRLF
                //// first chunk data
                /**/ "[ 1, 2, 3 ]" CRLF
                //// Last chunk
                /**/ "0" CRLF CRLF),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_NONE,
                    .httpTokenCount = 6,
                    .httpTokens =
                        // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                        // fields
                    (struct http_token[]){
                        {.type = HTTP_TOKEN_HTTP_VERSION, .start = 0x0, .end = 0x8},
                        {.type = HTTP_TOKEN_STATUS_CODE, .start = 0x9, .end = 0xc},
                        {.type = HTTP_TOKEN_HEADER_CONTENT_TYPE, .start = 0x1f, .end = 0x2f},
                        {.type = HTTP_TOKEN_HEADER_TRANSFER_ENCODING, .start = 0x44, .end = 0x4b},
                        {.type = HTTP_TOKEN_CHUNK_SIZE, .start = 0x4f, .end = 0x50},
                        {.type = HTTP_TOKEN_CHUNK_DATA, .start = 0x52, .end = 0x5d},
                        // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                        // fields
                    },
                    .httpTokenStrings =
                        (struct string[]){
                            StringFromLiteral("HTTP/1.1"),
                            StringFromLiteral("200"),
                            StringFromLiteral("application/json"),
                            StringFromLiteral("chunked"),
                            StringFromLiteral("b"),
                            StringFromLiteral("[ 1, 2, 3 ]"),
                        },
                    .content = &StringFromLiteral("[ 1, 2, 3 ]"),
                },
        },
        {
            .httpResponse = &StringFromLiteral(
                /*** --- Status-Line -------------------------------- ***/
                "HTTP/1.1 200 OK" CRLF
                /*** --- Header Fields ------------------------------ ***/
                /**/ "Content-Type: application/json" CRLF
                /**/ "Transfer-Encoding: chunked" CRLF
                /**/ CRLF
                /*** --- Message Body ------------------------------- ***/
                //// first chunk size (format: hex digits)
                /**/ "c" CRLF
                //// first chunk data
                /**/ "[ 4029, 2104" CRLF
                //// second chunk size (format: hex digits)
                /**/ "9" CRLF
                //// second chunk data
                /**/ "9342, 0 ]" CRLF
                //// Last chunk
                /**/ "0" CRLF CRLF),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_NONE,
                    .httpTokenCount = 8,
                    .httpTokens =
                        (struct http_token[]){
                            // NOTE: when changing this array, do not forget to update httpTokenCount field
                            {.type = HTTP_TOKEN_HTTP_VERSION, .start = 0x0, .end = 0x8},
                            {.type = HTTP_TOKEN_STATUS_CODE, .start = 0x9, .end = 0xc},
                            {.type = HTTP_TOKEN_HEADER_CONTENT_TYPE, .start = 0x1f, .end = 0x2f},
                            {.type = HTTP_TOKEN_HEADER_TRANSFER_ENCODING, .start = 0x44, .end = 0x4b},
                            {.type = HTTP_TOKEN_CHUNK_SIZE, .start = 0x4f, .end = 0x50},
                            {.type = HTTP_TOKEN_CHUNK_DATA, .start = 0x52, .end = 0x5e},
                            {.type = HTTP_TOKEN_CHUNK_SIZE, .start = 0x60, .end = 0x61},
                            {.type = HTTP_TOKEN_CHUNK_DATA, .start = 0x63, .end = 0x6c},
                            // NOTE: when changing this array, do not forget to update httpTokenCount field
                        },
                    .httpTokenStrings =
                        (struct string[]){
                            StringFromLiteral("HTTP/1.1"),
                            StringFromLiteral("200"),
                            StringFromLiteral("application/json"),
                            StringFromLiteral("chunked"),
                            StringFromLiteral("c"),
                            StringFromLiteral("[ 4029, 2104"),
                            StringFromLiteral("9"),
                            StringFromLiteral("9342, 0 ]"),
                        },
                    .content = &StringFromLiteral("[ 4029, 21049342, 0 ]"),
                },
        },
        {
            .httpResponse = &StringFromLiteral(
                /*** --- Status-Line -------------------------------- ***/
                "HTTP/1.1 200 OK" CRLF
                /*** --- Header Fields ------------------------------ ***/
                /**/ "Server: nginx/1.18.0 (Ubuntu)" CRLF
                /**/ "Date: Fri, 18 Apr 2025 07:14:00 GMT" CRLF
                /**/ "Content-Type: application/json" CRLF
                /**/ "Content-Length: 11" CRLF
                /**/ "Connection: keep-alive" CRLF
                /**/ CRLF
                /*** --- Message Body ------------------------------- ***/
                /**/ "[ 1, 2, 3 ]"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_NONE,
                    .httpTokenCount = 8,
                    .httpTokens =
                        // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                    // fields
                    (struct http_token[]){
                        {.type = HTTP_TOKEN_HTTP_VERSION, .start = 0x0, .end = 0x8},
                        {.type = HTTP_TOKEN_STATUS_CODE, .start = 0x9, .end = 0xc},
                        {.type = HTTP_TOKEN_HEADER_SERVER, .start = 0x19, .end = 0x2e},
                        {.type = HTTP_TOKEN_HEADER_DATE, .start = 0x36, .end = 0x53},
                        {.type = HTTP_TOKEN_HEADER_CONTENT_TYPE, .start = 0x63, .end = 0x73},
                        {.type = HTTP_TOKEN_HEADER_CONTENT_LENGTH, .start = 0x85, .end = 0x87},
                        {.type = HTTP_TOKEN_HEADER_CONNECTION, .start = 0x95, .end = 0x9f},
                        {.type = HTTP_TOKEN_CONTENT, .start = 0xa3, .end = 0xae},
                        // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                        // fields
                    },
                    .httpTokenStrings =
                        (struct string[]){
                            StringFromLiteral("HTTP/1.1"),
                            StringFromLiteral("200"),
                            StringFromLiteral("nginx/1.18.0 (Ubuntu)"),
                            StringFromLiteral("Fri, 18 Apr 2025 07:14:00 GMT"),
                            StringFromLiteral("application/json"),
                            StringFromLiteral("11"),
                            StringFromLiteral("keep-alive"),
                            StringFromLiteral("[ 1, 2, 3 ]"),
                        },
                    .content = &StringFromLiteral("[ 1, 2, 3 ]"),
                },
        },
#undef CRLF
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      u32 failedTestCount = 0;

      memory_temp tempMemory = MemoryTempBegin(&stackMemory);
      struct test_case *testCase = testCases + testCaseIndex;

      struct string *httpResponse = testCase->httpResponse;
      struct http_parser *httpParser = MakeHttpParser(tempMemory.arena, maxHttpTokenCount);

      enum http_parser_error expectedError = testCase->expected.error;
      HttpParse(httpParser, httpResponse);

      // Test http parser error
      enum http_parser_error gotError = httpParser->error;
      if (gotError != expectedError) {
        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, httpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }
        StringBuilderAppendStringLiteral(sb, "\n  expected error: ");
        StringBuilderAppendHttpParserError(sb, expectedError);
        StringBuilderAppendStringLiteral(sb, "\n       got error: ");
        StringBuilderAppendHttpParserError(sb, gotError);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      // If error expected, do not test other information
      if (expectedError != HTTP_PARSER_ERROR_NONE)
        continue;

      // Test http token count
      u32 expectedHttpTokenCount = testCase->expected.httpTokenCount;
      u32 gotHttpTokenCount = httpParser->tokenCount;
      if (gotHttpTokenCount != expectedHttpTokenCount) {
        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, httpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }
        StringBuilderAppendStringLiteral(sb, "\n  expected http token count: ");
        StringBuilderAppendU64(sb, expectedHttpTokenCount);
        StringBuilderAppendStringLiteral(sb, "\n                        got: ");
        StringBuilderAppendU64(sb, gotHttpTokenCount);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      // Test every http token
      b8 failedHttpTokenTest = 0;
      for (u32 httpTokenIndex = 0; httpTokenIndex < httpParser->tokenCount; httpTokenIndex++) {
        struct http_token *httpToken = httpParser->tokens + httpTokenIndex;
        struct http_token *expectedHttpToken = testCase->expected.httpTokens + httpTokenIndex;
        struct string gotString = HttpTokenExtractString(httpToken, httpResponse);
        struct string *expectedString = testCase->expected.httpTokenStrings + httpTokenIndex;

        if (httpToken->type == expectedHttpToken->type && httpToken->start == expectedHttpToken->start &&
            httpToken->end == expectedHttpToken->end && IsStringEqual(&gotString, expectedString))
          continue;

        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, httpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }

        StringBuilderAppendStringLiteral(sb, "\n  expected http token type: ");
        StringBuilderAppendHttpTokenType(sb, expectedHttpToken->type);
        StringBuilderAppendStringLiteral(sb, ", start: ");
        StringBuilderAppendU64(sb, expectedHttpToken->start);
        StringBuilderAppendStringLiteral(sb, ", end: ");
        StringBuilderAppendU64(sb, expectedHttpToken->end);
        StringBuilderAppendStringLiteral(sb, " at index: ");
        StringBuilderAppendU64(sb, httpTokenIndex);
        StringBuilderAppendStringLiteral(sb, "\n");
        StringBuilderAppendPrintableHexDump(sb, expectedString);

        debug_assert(expectedString->length == expectedHttpToken->end - expectedHttpToken->start &&
                     "Check your test cases, you got this one wrong");

        StringBuilderAppendStringLiteral(sb, "\n              but got type: ");
        StringBuilderAppendHttpTokenType(sb, httpToken->type);
        StringBuilderAppendStringLiteral(sb, ", start: ");
        StringBuilderAppendU64(sb, httpToken->start);
        StringBuilderAppendStringLiteral(sb, ", end: ");
        StringBuilderAppendU64(sb, httpToken->end);
        StringBuilderAppendStringLiteral(sb, "\n");
        StringBuilderAppendPrintableHexDump(sb, &gotString);

        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        failedHttpTokenTest = 1;
      }

      if (failedHttpTokenTest)
        continue;

      // Test extracting response data from HTTP response
      u32 maxContentLength = 128;
      string_builder *contentBuilder = MakeStringBuilder(tempMemory.arena, maxContentLength, 0);
      for (u32 httpTokenIndex = 3; httpTokenIndex < httpParser->tokenCount; httpTokenIndex++) {
        struct http_token *httpToken = httpParser->tokens + httpTokenIndex;
        if (httpToken->type != HTTP_TOKEN_CHUNK_DATA && httpToken->type != HTTP_TOKEN_CONTENT)
          continue;
        struct string data = HttpTokenExtractString(httpToken, httpResponse);
        StringBuilderAppendString(contentBuilder, &data);
      }

      struct string content = StringBuilderFlush(contentBuilder);
      struct string *expectedContent = testCase->expected.content;
      if (!IsStringEqual(&content, expectedContent)) {
        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, httpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }
        StringBuilderAppendStringLiteral(sb, "\n  expected content to be:\n");
        StringBuilderAppendPrintableHexDump(sb, expectedContent);
        StringBuilderAppendStringLiteral(sb, "\n                 but got:\n");
        StringBuilderAppendPrintableHexDump(sb, &content);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      MemoryTempEnd(&tempMemory);
    }
  }

  {
    u32 maxHttpTokenCount = 128;

    struct test_case {
      u32 httpResponseCount;
      struct string *httpResponses;
      struct {
        enum http_parser_error error;
        u32 httpTokenCount;
        struct http_token *httpTokens;
        struct string *httpTokenStrings;
        struct string *content;
      } expected;
    } testCases[] = {
#define CRLF "\r\n"
        {
            .httpResponseCount = 2,
            .httpResponses =
                (struct string[]){
                    StringFromLiteral(
                        /*** --- Status-Line -------------------------------- ***/
                        "HTTP/1.1 200 OK" CRLF
                        /*** --- Header Fields ------------------------------ ***/
                        /**/ "Content-Type: application/json" CRLF
                        /**/ "Transfer-Encoding: chunked" CRLF
                        /**/ CRLF
                        /*** --- Message Body ------------------------------- ***/
                        //// first chunk size (format: hex digits)
                        /**/ "c" CRLF
                        //// first chunk data
                        /**/ "[ 4029, 2104" CRLF),
                    StringFromLiteral(
                        //// second chunk size (format: hex digits)
                        /**/ "9" CRLF
                             //// second chunk data
                             /**/ "9342, 0 ]" CRLF
                             //// Last chunk
                             /**/ "0" CRLF CRLF),
                },
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_NONE,
                    .httpTokenCount = 8,
                    .httpTokens =
                        (struct http_token[]){
                            // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                            // fields
                            {.type = HTTP_TOKEN_HTTP_VERSION, .start = 0x0, .end = 0x8},
                            {.type = HTTP_TOKEN_STATUS_CODE, .start = 0x9, .end = 0xc},
                            {.type = HTTP_TOKEN_HEADER_CONTENT_TYPE, .start = 0x1f, .end = 0x2f},
                            {.type = HTTP_TOKEN_HEADER_TRANSFER_ENCODING, .start = 0x44, .end = 0x4b},
                            {.type = HTTP_TOKEN_CHUNK_SIZE, .start = 0x4f, .end = 0x50},
                            {.type = HTTP_TOKEN_CHUNK_DATA, .start = 0x52, .end = 0x5e},
                            {.type = HTTP_TOKEN_CHUNK_SIZE, .start = 0x60, .end = 0x61},
                            {.type = HTTP_TOKEN_CHUNK_DATA, .start = 0x63, .end = 0x6c},
                            // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                            // fields
                        },
                    .httpTokenStrings =
                        (struct string[]){
                            StringFromLiteral("HTTP/1.1"),
                            StringFromLiteral("200"),
                            StringFromLiteral("application/json"),
                            StringFromLiteral("chunked"),
                            StringFromLiteral("b"),
                            StringFromLiteral("[ 4029, 2104"),
                            StringFromLiteral("9"),
                            StringFromLiteral("9342, 0 ]"),
                        },
                    .content = &StringFromLiteral("[ 4029, 21049342, 0 ]"),
                },
        },
        {
            .httpResponseCount = 3,
            .httpResponses =
                (struct string[]){
                    StringFromLiteral(
                        /*** --- Status-Line -------------------------------- ***/
                        "HTTP/1.1 200 OK" CRLF
                        /*** --- Header Fields ------------------------------ ***/
                        /**/ "Content-Type: application/json" CRLF
                        /**/ "Transfer-Encoding: chunked" CRLF
                        /**/ CRLF
                        /*** --- Message Body ------------------------------- ***/
                        //// first chunk size (format: hex digits)
                        /**/ "c" CRLF
                        //// first chunk data
                        /**/ "[ 4029"),
                    StringFromLiteral(
                        /**/ ", 2104"),
                    StringFromLiteral(
                        /**/ CRLF
                        //// second chunk size (format: hex digits)
                        /**/ "9" CRLF
                             //// second chunk data
                             /**/ "9342, 0 ]" CRLF
                             //// Last chunk
                             /**/ "0" CRLF CRLF),

                },
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_NONE,
                    .httpTokenCount = 8,
                    .httpTokens =
                        (struct http_token[]){
                            // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                            // fields
                            {.type = HTTP_TOKEN_HTTP_VERSION, .start = 0x0, .end = 0x8},
                            {.type = HTTP_TOKEN_STATUS_CODE, .start = 0x9, .end = 0xc},
                            {.type = HTTP_TOKEN_HEADER_CONTENT_TYPE, .start = 0x1f, .end = 0x2f},
                            {.type = HTTP_TOKEN_HEADER_TRANSFER_ENCODING, .start = 0x44, .end = 0x4b},
                            {.type = HTTP_TOKEN_CHUNK_SIZE, .start = 0x4f, .end = 0x50},
                            {.type = HTTP_TOKEN_CHUNK_DATA, .start = 0x52, .end = 0x5e},
                            {.type = HTTP_TOKEN_CHUNK_SIZE, .start = 0x60, .end = 0x61},
                            {.type = HTTP_TOKEN_CHUNK_DATA, .start = 0x63, .end = 0x6c},
                            // NOTE: when changing this array, do not forget to update httpTokenCount, httpTokenStrings
                            // fields
                        },
                    .httpTokenStrings =
                        (struct string[]){
                            StringFromLiteral("HTTP/1.1"),
                            StringFromLiteral("200"),
                            StringFromLiteral("application/json"),
                            StringFromLiteral("chunked"),
                            StringFromLiteral("c"),
                            StringFromLiteral("[ 4029, 2104"),
                            StringFromLiteral("9"),
                            StringFromLiteral("9342, 0 ]"),
                        },
                    .content = &StringFromLiteral("[ 4029, 21049342, 0 ]"),
                },
        },
#undef CRLF
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      u32 failedTestCount = 0;

      memory_temp tempMemory = MemoryTempBegin(&stackMemory);
      struct test_case *testCase = testCases + testCaseIndex;

      // combine http responses
      u64 httpResponseLength = 0;
      u32 httpResponseCount = testCase->httpResponseCount;
      for (u32 httpResponseIndex = 0; httpResponseIndex < httpResponseCount; httpResponseIndex++) {
        struct string *httpResponse = testCase->httpResponses + httpResponseIndex;
        httpResponseLength += httpResponse->length;
      }
      string_builder *httpResponseBuilder = MakeStringBuilder(tempMemory.arena, httpResponseLength, 0);
      for (u32 httpResponseIndex = 0; httpResponseIndex < httpResponseCount; httpResponseIndex++) {
        struct string *httpResponse = testCase->httpResponses + httpResponseIndex;
        StringBuilderAppendString(httpResponseBuilder, httpResponse);
      }
      struct string combinedHttpResponse = StringBuilderFlush(httpResponseBuilder);

      struct http_parser *httpParser = MakeHttpParser(tempMemory.arena, maxHttpTokenCount);

      enum http_parser_error expectedError = testCase->expected.error;

      // Parse http partial responses
      for (u32 httpResponseIndex = 0; httpResponseIndex < httpResponseCount; httpResponseIndex++) {
        struct string *httpResponse = testCase->httpResponses + httpResponseIndex;
        if (!HttpParse(httpParser, httpResponse) && httpParser->error != HTTP_PARSER_ERROR_PARTIAL)
          break;
      }

      // Test http parser error
      enum http_parser_error gotError = httpParser->error;
      if (gotError != expectedError) {
        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }
        StringBuilderAppendStringLiteral(sb, "\n  expected error: ");
        StringBuilderAppendHttpParserError(sb, expectedError);
        StringBuilderAppendStringLiteral(sb, "\n  got error: ");
        StringBuilderAppendHttpParserError(sb, gotError);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      // If error expected, do not test other information
      if (expectedError != HTTP_PARSER_ERROR_NONE)
        continue;

      // Test http token count
      u32 expectedHttpTokenCount = testCase->expected.httpTokenCount;
      u32 gotHttpTokenCount = httpParser->tokenCount;
      if (gotHttpTokenCount != expectedHttpTokenCount) {
        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }
        StringBuilderAppendStringLiteral(sb, "\n  expected http token count: ");
        StringBuilderAppendU64(sb, expectedHttpTokenCount);
        StringBuilderAppendStringLiteral(sb, "\n                        got: ");
        StringBuilderAppendU64(sb, gotHttpTokenCount);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      // Test every http token
      b8 failedHttpTokenTest = 0;
      for (u32 httpTokenIndex = 0; httpTokenIndex < httpParser->tokenCount; httpTokenIndex++) {
        struct http_token *httpToken = httpParser->tokens + httpTokenIndex;
        struct http_token *expectedHttpToken = testCase->expected.httpTokens + httpTokenIndex;

        if (httpToken->type == expectedHttpToken->type && httpToken->start == expectedHttpToken->start &&
            httpToken->end == expectedHttpToken->end)
          continue;

        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }

        StringBuilderAppendStringLiteral(sb, "\n  expected http token type: ");
        StringBuilderAppendHttpTokenType(sb, expectedHttpToken->type);
        StringBuilderAppendStringLiteral(sb, ", start: ");
        StringBuilderAppendU64(sb, expectedHttpToken->start);
        StringBuilderAppendStringLiteral(sb, ", end: ");
        StringBuilderAppendU64(sb, expectedHttpToken->end);
        StringBuilderAppendStringLiteral(sb, " at index: ");
        StringBuilderAppendU64(sb, httpTokenIndex);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string *expectedString = testCase->expected.httpTokenStrings + httpTokenIndex;
        StringBuilderAppendPrintableHexDump(sb, expectedString);

        debug_assert(expectedString->length == expectedHttpToken->end - expectedHttpToken->start &&
                     "Check your test cases, you got this one wrong");

        StringBuilderAppendStringLiteral(sb, "\n              but got type: ");
        StringBuilderAppendHttpTokenType(sb, httpToken->type);
        StringBuilderAppendStringLiteral(sb, ", start: ");
        StringBuilderAppendU64(sb, httpToken->start);
        StringBuilderAppendStringLiteral(sb, ", end: ");
        StringBuilderAppendU64(sb, httpToken->end);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string gotString = HttpTokenExtractString(httpToken, &combinedHttpResponse);
        StringBuilderAppendPrintableHexDump(sb, &gotString);

        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        failedHttpTokenTest = 1;
      }

      if (failedHttpTokenTest)
        continue;

      // Test extracting response data from HTTP response
      u32 maxContentLength = 128;
      string_builder *contentBuilder = MakeStringBuilder(tempMemory.arena, maxContentLength, 0);
      for (u32 httpTokenIndex = 3; httpTokenIndex < httpParser->tokenCount; httpTokenIndex++) {
        struct http_token *httpToken = httpParser->tokens + httpTokenIndex;
        if (httpToken->type != HTTP_TOKEN_CHUNK_DATA)
          continue;
        struct string data = HttpTokenExtractString(httpToken, &combinedHttpResponse);
        StringBuilderAppendString(contentBuilder, &data);
      }

      struct string content = StringBuilderFlush(contentBuilder);
      struct string *expectedContent = testCase->expected.content;
      if (!IsStringEqual(&content, expectedContent)) {
        errorCode = expectedError == HTTP_PARSER_ERROR_NONE ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                                            : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\nHTTP response:\n```\n");
          StringBuilderAppendPrintableHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendStringLiteral(sb, "\n```");
        }
        StringBuilderAppendStringLiteral(sb, "\n  expected content to be:\n");
        StringBuilderAppendPrintableHexDump(sb, expectedContent);
        StringBuilderAppendStringLiteral(sb, "\n                 but got:\n");
        StringBuilderAppendPrintableHexDump(sb, &content);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      MemoryTempEnd(&tempMemory);
    }
  }

  return (int)errorCode;
}
