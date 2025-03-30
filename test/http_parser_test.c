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
      [HTTP_RESPONSE_IS_NOT_HTTP_1_1] = STRING_FROM_ZERO_TERMINATED("response is not HTTP 1.1"),
      [HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER] =
          STRING_FROM_ZERO_TERMINATED("status code is not 3 digit integer"),
      [HTTP_RESPONSE_STATUS_CODE_NOT_EXPECTED] = STRING_FROM_ZERO_TERMINATED("status code is not what is expected"),
      [HTTP_RESPONSE_PARTIAL] = STRING_FROM_ZERO_TERMINATED("response is partial"),
      [HTTP_RESPONSE_NOT_CHUNKED_TRANSFER_ENCODED] =
          STRING_FROM_ZERO_TERMINATED("response is not chunked transfer encoded"),
      [HTTP_RESPONSE_CONTENT_TYPE_NOT_EXPECTED] = STRING_FROM_ZERO_TERMINATED("content type is not what is expected"),
      [HTTP_RESPONSE_CHUNK_TRANSFER_ENCODING_CHUNK_SIZE_IS_NOT_HEX] =
          STRING_FROM_ZERO_TERMINATED("chunk size is not hexadecimal"),
      [HTTP_RESPONSE_CHUNK_TRANSFER_ENCODING_CHUNK_DATA_MALFORMED] =
          STRING_FROM_ZERO_TERMINATED("chunk data is malformed"),
  };
  struct string *httpParserErrorText = httpParserErrorTexts + (u32)error;
  StringBuilderAppendString(sb, httpParserErrorText);
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

internalfn string_builder *
MakeStringBuilder(memory_arena *arena, u64 outBufferLength, u64 stringBufferLength)
{
  debug_assert(outBufferLength > 0);
  string_builder *sb = MemoryArenaPushUnaligned(arena, sizeof(*sb));

  string *outBuffer = MemoryArenaPushUnaligned(arena, sizeof(*outBuffer));
  outBuffer->length = outBufferLength;
  outBuffer->value = MemoryArenaPushUnaligned(arena, sizeof(u8) * outBuffer->length);
  sb->outBuffer = outBuffer;

  if (stringBufferLength == 0) {
    // do not need to convert any variable, all strings
    sb->stringBuffer = 0;
  } else {
    string *stringBuffer = MemoryArenaPushUnaligned(arena, sizeof(*stringBuffer));
    stringBuffer->length = stringBufferLength;
    stringBuffer->value = MemoryArenaPushUnaligned(arena, sizeof(u8) * stringBuffer->length);
    sb->stringBuffer = stringBuffer;
  }
  sb->length = 0;

  return sb;
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
    struct test_case {
      struct string *httpResponse;
      struct {
        b8 value;
        enum http_parser_error error;
        u32 tokenCount;
        struct json_token *tokens;
      } expected;
    } testCases[] = {
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(""),
            .expected =
                {
                    .value = 0,
                    .error = HTTP_RESPONSE_PARTIAL,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.0"),
            .expected =
                {
                    .value = 0,
                    .error = HTTP_RESPONSE_IS_NOT_HTTP_1_1,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/2.0"),
            .expected =
                {
                    .value = 0,
                    .error = HTTP_RESPONSE_IS_NOT_HTTP_1_1,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 ABC"),
            .expected =
                {
                    .value = 0,
                    .error = HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 1"),
            .expected =
                {
                    .value = 0,
                    .error = HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 10"),
            .expected =
                {
                    .value = 0,
                    .error = HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 1000 Custom"),
            .expected =
                {
                    .value = 0,
                    .error = HTTP_RESPONSE_STATUS_CODE_IS_NOT_3_DIGIT_INTEGER,
                },
        },
        {
            .httpResponse = &STRING_FROM_ZERO_TERMINATED(
#define CRLF "\r\n"
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
                /**/ "{ \"a\": 97 }" CRLF
                //// Last chunk
                /**/ "0" CRLF CRLF),
            .expected =
                {
                    .value = 1,
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 82 + 0, .end = 82 + 11},
                            {.type = JSON_TOKEN_STRING, .start = 82 + 3, .end = 82 + 4},
                            {.type = JSON_TOKEN_STRING, .start = 82 + 7, .end = 82 + 9},
                        },
                },
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      memory_temp tempMemory = MemoryTempBegin(&stackMemory);
      struct test_case *testCase = testCases + testCaseIndex;

      struct string *httpResponse = testCase->httpResponse;
      b8 expectedValue = testCase->expected.value;
      enum http_parser_error expectedError = testCase->expected.error;

      struct http_parser parser;
      HttpParserInit(&parser);
      struct json_parser *jsonParser = MakeJsonParser(tempMemory.arena, 128);
      HttpParserMustBeJson(&parser, jsonParser);

      if (expectedValue == 1) {
        u32 breakHere = 1;
      }

      b8 gotValue = HttpParserParse(&parser, httpResponse);
      if (gotValue == expectedValue && (!expectedValue && parser.error == expectedError))
        continue;

      if (gotValue != expectedValue) {
        errorCode =
            expectedValue ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
        StringBuilderAppendHexDump(sb, httpResponse);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      } else if (parser.error != expectedError) {
        errorCode =
            expectedValue ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
        StringBuilderAppendHexDump(sb, httpResponse);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected error: "));
        StringBuilderAppendHttpParserError(sb, expectedError);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got error: "));
        StringBuilderAppendHttpParserError(sb, parser.error);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      } else {
        u32 expectedTokenCount = testCase->expected.tokenCount;
        u32 gotTokenCount = jsonParser->tokenCount;
        if (gotTokenCount != expectedTokenCount) {
          errorCode =
              expectedValue ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

          StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
          StringBuilderAppendHexDump(sb, httpResponse);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected json token count: "));
          StringBuilderAppendU64(sb, expectedTokenCount);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got json token count: "));
          StringBuilderAppendU64(sb, gotTokenCount);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
          struct string errorMessage = StringBuilderFlush(sb);
          PrintString(&errorMessage);
        } else {
          struct json_token *expectedTokens = testCase->expected.tokens;
          struct json_token *gotTokens = jsonParser->tokens;
          u32 wrongTokenCount = 0;

          for (u32 tokenIndex = 0; tokenIndex < gotTokenCount; tokenIndex++) {
            struct json_token *expectedToken = expectedTokens + tokenIndex;
            struct json_token *gotToken = gotTokens + tokenIndex;
            struct string expectedTokenString = JsonTokenExtractString(expectedToken, httpResponse);
            struct string gotTokenString = JsonTokenExtractString(gotToken, httpResponse);

            if (IsStringEqual(&gotTokenString, &expectedTokenString))
              continue;

            errorCode = expectedValue ? HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                      : HTTP_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

            if (wrongTokenCount == 0) {
              StringBuilderAppendString(sb, GetHttpParserTestErrorMessage(errorCode));
              StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\nHTTP response:\n```\n"));
              StringBuilderAppendHexDump(sb, httpResponse);
              StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n```"));
            }

            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected json token at index: "));
            StringBuilderAppendU64(sb, tokenIndex);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    start: "));
            StringBuilderAppendU64(sb, expectedToken->start);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" end: "));
            StringBuilderAppendU64(sb, expectedToken->end);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    string: "));
            StringBuilderAppendPrintableString(sb, &expectedTokenString);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  got json token: "));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    start: "));
            StringBuilderAppendU64(sb, gotToken->start);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" end: "));
            StringBuilderAppendU64(sb, gotToken->end);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    string: "));
            StringBuilderAppendPrintableString(sb, &gotTokenString);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
            struct string errorMessage = StringBuilderFlush(sb);
            PrintString(&errorMessage);

            wrongTokenCount++;
          }
        }
      }

      MemoryTempEnd(&tempMemory);
    }
  }

  return (int)errorCode;
}
