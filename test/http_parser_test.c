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
#define XX(tag, msg) {.code = HTTP_PARSER_TEST_ERROR_##tag, .message = STRING_FROM_ZERO_TERMINATED(msg)},
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
      [HTTP_PARSER_ERROR_NONE] = STRING_FROM_ZERO_TERMINATED("None"),
      [HTTP_PARSER_ERROR_OUT_OF_MEMORY] = STRING_FROM_ZERO_TERMINATED("Out of memory"),
      [HTTP_PARSER_ERROR_HTTP_VERSION_INVALID] = STRING_FROM_ZERO_TERMINATED("HTTP version is invalid"),
      [HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1] = STRING_FROM_ZERO_TERMINATED("HTTP version 1.1 is expected"),
      [HTTP_PARSER_ERROR_STATUS_CODE_INVALID] = STRING_FROM_ZERO_TERMINATED("Status code is invalid"),
      [HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER] =
          STRING_FROM_ZERO_TERMINATED("Status code must be 3-digit integer"),
      [HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_BETWEEN_100_AND_999] =
          STRING_FROM_ZERO_TERMINATED("Status code must be [100, 999]"),
      [HTTP_PARSER_ERROR_REASON_PHRASE_INVALID] = STRING_FROM_ZERO_TERMINATED("Reason phrase is invalid"),
      [HTTP_PARSER_ERROR_HEADER_FIELD_NAME_REQUIRED] = STRING_FROM_ZERO_TERMINATED("Header field name is required"),
      [HTTP_PARSER_ERROR_HEADER_FIELD_VALUE_REQUIRED] = STRING_FROM_ZERO_TERMINATED("Header field value is required"),
      [HTTP_PARSER_ERROR_CONTENT_LENGTH_EXPECTED_POSITIVE_NUMBER] =
          STRING_FROM_ZERO_TERMINATED("Content length must be positive number"),
      [HTTP_PARSER_ERROR_UNSUPPORTED_TRANSFER_ENCODING] = STRING_FROM_ZERO_TERMINATED("Unsupported transfer encoding"),
      [HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID] = STRING_FROM_ZERO_TERMINATED("Chunk size is invalid"),
      [HTTP_PARSER_ERROR_CHUNK_DATA_MALFORMED] = STRING_FROM_ZERO_TERMINATED("Chunk data is malformed"),
      [HTTP_PARSER_ERROR_CONTENT_INVALID_LENGTH] = STRING_FROM_ZERO_TERMINATED("Content has invalid length"),
      [HTTP_PARSER_ERROR_PARTIAL] = STRING_FROM_ZERO_TERMINATED("Partial response"),
  };
  struct string *httpParserErrorText = httpParserErrorTexts + (u32)error;
  StringBuilderAppendString(sb, httpParserErrorText);
}

internalfn void
StringBuilderAppendHttpTokenType(string_builder *sb, enum http_token_type type)
{
  struct string table[] = {
      [HTTP_TOKEN_NONE] = STRING_FROM_ZERO_TERMINATED("None"),

      [HTTP_TOKEN_HTTP_VERSION] = STRING_FROM_ZERO_TERMINATED("HTTP version"),
      [HTTP_TOKEN_STATUS_CODE] = STRING_FROM_ZERO_TERMINATED("Status code"),
      [HTTP_TOKEN_REASON_PHRASE] = STRING_FROM_ZERO_TERMINATED("Reason Phrase"),

      [HTTP_TOKEN_HEADER_CACHE_CONTROL] = STRING_FROM_ZERO_TERMINATED("Cache-Control"),
      [HTTP_TOKEN_HEADER_CONNECTION] = STRING_FROM_ZERO_TERMINATED("Connection"),
      [HTTP_TOKEN_HEADER_DATE] = STRING_FROM_ZERO_TERMINATED("Date"),
      [HTTP_TOKEN_HEADER_PRAGMA] = STRING_FROM_ZERO_TERMINATED("Pragma"),
      [HTTP_TOKEN_HEADER_TRAILER] = STRING_FROM_ZERO_TERMINATED("Trailer"),
      [HTTP_TOKEN_HEADER_TRANSFER_ENCODING] = STRING_FROM_ZERO_TERMINATED("Transfer-Encoding"),
      [HTTP_TOKEN_HEADER_UPGRADE] = STRING_FROM_ZERO_TERMINATED("Upgrade"),
      [HTTP_TOKEN_HEADER_VIA] = STRING_FROM_ZERO_TERMINATED("Via"),
      [HTTP_TOKEN_HEADER_WARNING] = STRING_FROM_ZERO_TERMINATED("Warning"),

      [HTTP_TOKEN_HEADER_ACCEPT_RANGES] = STRING_FROM_ZERO_TERMINATED("Accept-Ranges"),
      [HTTP_TOKEN_HEADER_AGE] = STRING_FROM_ZERO_TERMINATED("Age"),
      [HTTP_TOKEN_HEADER_ETAG] = STRING_FROM_ZERO_TERMINATED("Etag"),
      [HTTP_TOKEN_HEADER_LOCATION] = STRING_FROM_ZERO_TERMINATED("Location"),
      [HTTP_TOKEN_HEADER_PROXY_AUTHENTICATE] = STRING_FROM_ZERO_TERMINATED("Proxy-Authenticate"),

      [HTTP_TOKEN_HEADER_ALLOW] = STRING_FROM_ZERO_TERMINATED("Allow"),
      [HTTP_TOKEN_HEADER_CONTENT_ENCODING] = STRING_FROM_ZERO_TERMINATED("Content-Encoding"),
      [HTTP_TOKEN_HEADER_CONTENT_LANGUAGE] = STRING_FROM_ZERO_TERMINATED("Content-Language"),
      [HTTP_TOKEN_HEADER_CONTENT_LENGTH] = STRING_FROM_ZERO_TERMINATED("Content-Length"),
      [HTTP_TOKEN_HEADER_CONTENT_LOCATION] = STRING_FROM_ZERO_TERMINATED("Content-Location"),
      [HTTP_TOKEN_HEADER_CONTENT_MD5] = STRING_FROM_ZERO_TERMINATED("Content-Md5"),
      [HTTP_TOKEN_HEADER_CONTENT_RANGE] = STRING_FROM_ZERO_TERMINATED("Content-Range"),
      [HTTP_TOKEN_HEADER_CONTENT_TYPE] = STRING_FROM_ZERO_TERMINATED("Content-Type"),
      [HTTP_TOKEN_HEADER_EXPIRES] = STRING_FROM_ZERO_TERMINATED("Expires"),
      [HTTP_TOKEN_HEADER_LAST_MODIFIED] = STRING_FROM_ZERO_TERMINATED("Last-Modified"),

      [HTTP_TOKEN_CHUNK_SIZE] = STRING_FROM_ZERO_TERMINATED("Chunk size"),
      [HTTP_TOKEN_CHUNK_DATA] = STRING_FROM_ZERO_TERMINATED("Chunk data"),
  };
  struct string *string = table + (u32)type;
  StringBuilderAppendString(sb, string);
}

internalfn void
StringBuilderAppendBool(string_builder *sb, b8 value)
{
  struct string *boolString = value ? &STRING_FROM_ZERO_TERMINATED("true") : &STRING_FROM_ZERO_TERMINATED("false");
  StringBuilderAppendString(sb, boolString);
}

internalfn void
StringBuilderAppendPrintableString(string_builder *sb, struct string *string)
{
  if (string->value == 0)
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(NULL)"));
  else if (string->value != 0 && string->length == 0)
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(EMPTY)"));
  else if (string->length == 1 && string->value[0] == ' ')
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(SPACE)"));
  else
    StringBuilderAppendString(sb, string);
}

internalfn void
StringBuilderAppendHexDump(string_builder *sb, struct string *string)
{
  if (string->value == 0) {
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(NULL)"));
    return;
  } else if (string->value != 0 && string->length == 0) {
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(EMPTY)"));
    return;
  }

  struct string_cursor cursor = StringCursorFromString(string);
  u8 offsetBuffer[8];
  struct string offsetBufferString = StringFromBuffer(offsetBuffer, ARRAY_COUNT(offsetBuffer));
  u8 hexBuffer[2];
  struct string hexBufferString = StringFromBuffer(hexBuffer, ARRAY_COUNT(hexBuffer));

  while (!IsStringCursorAtEnd(&cursor)) {
    if (cursor.position == 0) {
      struct string header = STRING_FROM_ZERO_TERMINATED("          0  1  2  3  4  5  6  7   8  9  a  b  c  d  e  f\n");
      StringBuilderAppendString(sb, &header);
    }

    // offset
    struct string offsetText = FormatHex(&offsetBufferString, cursor.position);
    for (u32 offsetTextPrefixIndex = 0; offsetTextPrefixIndex < offsetBufferString.length - offsetText.length;
         offsetTextPrefixIndex++) {
      // offset length must be 8, so fill prefix with zeros
      StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("0"));
    }
    StringBuilderAppendString(sb, &offsetText);

    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" "));

    // hex
    u64 width = 16;
    struct string substring = StringCursorConsumeSubstring(&cursor, width);
    for (u64 substringIndex = 0; substringIndex < substring.length; substringIndex++) {
      u8 character = *(substring.value + substringIndex);
      struct string hexText = FormatHex(&hexBufferString, (u64)character);
      debug_assert(hexText.length == 2);
      StringBuilderAppendString(sb, &hexText);

      StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" "));

      if (substringIndex + 1 == 8)
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" "));
    }

    for (u64 index = 0; index < width - substring.length; index++) {
      // align ascii to right
      StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("   "));
      if (index + substring.length + 1 == 8)
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" "));
    }

    // ascii input
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("|"));
    for (u64 substringIndex = 0; substringIndex < substring.length; substringIndex++) {
      u8 character = *(substring.value + substringIndex);
      b8 disallowed[255] = {
          [0x00 ... 0x1a] = 1,
      };
      if (disallowed[character])
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("."));
      else {
        struct string characterString = StringFromBuffer(&character, 1);
        StringBuilderAppendString(sb, &characterString);
      }
    }
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("|"));

    if (!IsStringCursorAtEnd(&cursor))
      StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
  }
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
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(""),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_HTTP_VERSION_INVALID,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.0"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/2.0"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 ABC"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 1"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 10"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 1000 Custom"),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 200 " CRLF),
            .expected =
                {
                    .error = HTTP_PARSER_ERROR_REASON_PHRASE_INVALID,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(
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
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(
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
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(
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
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(
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
                            STRING_FROM_ZERO_TERMINATED("HTTP/1.1"),
                            STRING_FROM_ZERO_TERMINATED("200"),
                            STRING_FROM_ZERO_TERMINATED("application/json"),
                            STRING_FROM_ZERO_TERMINATED("11"),
                            STRING_FROM_ZERO_TERMINATED("[ 1, 2, 3 ]"),
                        },
                    .content = &STRING_FROM_ZERO_TERMINATED("[ 1, 2, 3 ]"),
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(
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
                            STRING_FROM_ZERO_TERMINATED("HTTP/1.1"),
                            STRING_FROM_ZERO_TERMINATED("200"),
                            STRING_FROM_ZERO_TERMINATED("application/json"),
                            STRING_FROM_ZERO_TERMINATED("chunked"),
                            STRING_FROM_ZERO_TERMINATED("b"),
                            STRING_FROM_ZERO_TERMINATED("[ 1, 2, 3 ]"),
                        },
                    .content = &STRING_FROM_ZERO_TERMINATED("[ 1, 2, 3 ]"),
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(
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
                            STRING_FROM_ZERO_TERMINATED("HTTP/1.1"),
                            STRING_FROM_ZERO_TERMINATED("200"),
                            STRING_FROM_ZERO_TERMINATED("application/json"),
                            STRING_FROM_ZERO_TERMINATED("chunked"),
                            STRING_FROM_ZERO_TERMINATED("b"),
                            STRING_FROM_ZERO_TERMINATED("[ 4029, 2104"),
                            STRING_FROM_ZERO_TERMINATED("9"),
                            STRING_FROM_ZERO_TERMINATED("9342, 0 ]"),
                        },
                    .content = &STRING_FROM_ZERO_TERMINATED("[ 4029, 21049342, 0 ]"),
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, httpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected error: "));
        StringBuilderAppendHttpParserError(sb, expectedError);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got error: "));
        StringBuilderAppendHttpParserError(sb, gotError);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, httpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected http token count: "));
        StringBuilderAppendU64(sb, expectedHttpTokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n                        got: "));
        StringBuilderAppendU64(sb, gotHttpTokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, httpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected http token type: "));
        StringBuilderAppendHttpTokenType(sb, expectedHttpToken->type);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", start: "));
        StringBuilderAppendU64(sb, expectedHttpToken->start);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", end: "));
        StringBuilderAppendU64(sb, expectedHttpToken->end);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index: "));
        StringBuilderAppendU64(sb, httpTokenIndex);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string *expectedString = testCase->expected.httpTokenStrings + httpTokenIndex;
        StringBuilderAppendHexDump(sb, expectedString);

        debug_assert(expectedString->length == expectedHttpToken->end - expectedHttpToken->start &&
                     "Check your test cases, you got this one wrong");

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n              but got type: "));
        StringBuilderAppendHttpTokenType(sb, httpToken->type);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", start: "));
        StringBuilderAppendU64(sb, httpToken->start);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", end: "));
        StringBuilderAppendU64(sb, httpToken->end);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string gotString = HttpTokenExtractString(httpToken, httpResponse);
        StringBuilderAppendHexDump(sb, &gotString);

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, httpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected content to be:\n"));
        StringBuilderAppendHexDump(sb, expectedContent);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n                 but got:\n"));
        StringBuilderAppendHexDump(sb, &content);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
                    STRING_FROM_ZERO_TERMINATED(
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
                    STRING_FROM_ZERO_TERMINATED(
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
                            STRING_FROM_ZERO_TERMINATED("HTTP/1.1"),
                            STRING_FROM_ZERO_TERMINATED("200"),
                            STRING_FROM_ZERO_TERMINATED("application/json"),
                            STRING_FROM_ZERO_TERMINATED("chunked"),
                            STRING_FROM_ZERO_TERMINATED("b"),
                            STRING_FROM_ZERO_TERMINATED("[ 4029, 2104"),
                            STRING_FROM_ZERO_TERMINATED("9"),
                            STRING_FROM_ZERO_TERMINATED("9342, 0 ]"),
                        },
                    .content = &STRING_FROM_ZERO_TERMINATED("[ 4029, 21049342, 0 ]"),
                },
        },
        {
            .httpResponseCount = 3,
            .httpResponses =
                (struct string[]){
                    STRING_FROM_ZERO_TERMINATED(
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
                    STRING_FROM_ZERO_TERMINATED(
                        /**/ ", 2104"),
                    STRING_FROM_ZERO_TERMINATED(
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
                            STRING_FROM_ZERO_TERMINATED("HTTP/1.1"),
                            STRING_FROM_ZERO_TERMINATED("200"),
                            STRING_FROM_ZERO_TERMINATED("application/json"),
                            STRING_FROM_ZERO_TERMINATED("chunked"),
                            STRING_FROM_ZERO_TERMINATED("b"),
                            STRING_FROM_ZERO_TERMINATED("[ 4029, 2104"),
                            STRING_FROM_ZERO_TERMINATED("9"),
                            STRING_FROM_ZERO_TERMINATED("9342, 0 ]"),
                        },
                    .content = &STRING_FROM_ZERO_TERMINATED("[ 4029, 21049342, 0 ]"),
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected error: "));
        StringBuilderAppendHttpParserError(sb, expectedError);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  got error: "));
        StringBuilderAppendHttpParserError(sb, gotError);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected http token count: "));
        StringBuilderAppendU64(sb, expectedHttpTokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n                        got: "));
        StringBuilderAppendU64(sb, gotHttpTokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected http token type: "));
        StringBuilderAppendHttpTokenType(sb, expectedHttpToken->type);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", start: "));
        StringBuilderAppendU64(sb, expectedHttpToken->start);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", end: "));
        StringBuilderAppendU64(sb, expectedHttpToken->end);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index: "));
        StringBuilderAppendU64(sb, httpTokenIndex);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string *expectedString = testCase->expected.httpTokenStrings + httpTokenIndex;
        StringBuilderAppendHexDump(sb, expectedString);

        debug_assert(expectedString->length == expectedHttpToken->end - expectedHttpToken->start &&
                     "Check your test cases, you got this one wrong");

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n              but got type: "));
        StringBuilderAppendHttpTokenType(sb, httpToken->type);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", start: "));
        StringBuilderAppendU64(sb, httpToken->start);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(", end: "));
        StringBuilderAppendU64(sb, httpToken->end);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string gotString = HttpTokenExtractString(httpToken, &combinedHttpResponse);
        StringBuilderAppendHexDump(sb, &gotString);

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, &combinedHttpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected content to be:\n"));
        StringBuilderAppendHexDump(sb, expectedContent);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n                 but got:\n"));
        StringBuilderAppendHexDump(sb, &content);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
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
