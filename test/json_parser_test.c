#include "json_parser.c"
#include "print.h"
#include "string_builder.h"

#define TEST_ERROR_LIST(XX)                                                                                            \
  XX(PARSE_EXPECTED_TRUE, "Json must be parsed successfully")                                                          \
  XX(PARSE_EXPECTED_FALSE, "Json must NOT be able to parse anything")

enum json_parser_test_error {
  JSON_PARSER_TEST_ERROR_NONE = 0,
#define XX(tag, message) JSON_PARSER_TEST_ERROR_##tag,
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

comptime struct json_parser_test_error_info {
  enum json_parser_test_error code;
  struct string message;
} TEXT_TEST_ERRORS[] = {
#define XX(tag, msg) {.code = JSON_PARSER_TEST_ERROR_##tag, .message = StringFromLiteral(msg)},
    TEST_ERROR_LIST(XX)
#undef XX
};

internalfn string *
GetTextTestErrorMessage(enum json_parser_test_error errorCode)
{
  for (u32 index = 0; index < ARRAY_COUNT(TEXT_TEST_ERRORS); index++) {
    const struct json_parser_test_error_info *info = TEXT_TEST_ERRORS + index;
    if (info->code == errorCode)
      return (struct string *)&info->message;
  }
  return 0;
}

internalfn void
StringBuilderAppendBool(string_builder *sb, b8 value)
{
  struct string *boolString = value ? &StringFromLiteral("true") : &StringFromLiteral("false");
  StringBuilderAppendString(sb, boolString);
}

internalfn void
StringBuilderAppendJsonTokenType(string_builder *sb, enum json_token_type type)
{
  struct string table[] = {
      [JSON_TOKEN_NONE] = StringFromLiteral("NONE"),
      [JSON_TOKEN_NULL] = StringFromLiteral("NULL"),
      [JSON_TOKEN_OBJECT] = StringFromLiteral("OBJECT"),
      [JSON_TOKEN_ARRAY] = StringFromLiteral("ARRAY"),
      [JSON_TOKEN_STRING] = StringFromLiteral("STRING"),
      [JSON_TOKEN_BOOLEAN_FALSE] = StringFromLiteral("BOOLEAN FALSE"),
      [JSON_TOKEN_BOOLEAN_TRUE] = StringFromLiteral("BOOLEAN TRUE"),
      [JSON_TOKEN_NUMBER] = StringFromLiteral("NUMBER"),
  };
  struct string *string = table + (u32)type;
  StringBuilderAppendString(sb, string);
}

internalfn void
StringBuilderAppendJsonParserError(string_builder *sb, enum json_parser_error error)
{
  struct string table[] = {
      [JSON_PARSER_ERROR_NONE] = StringFromLiteral("no errors"),
      [JSON_PARSER_ERROR_OUT_OF_TOKENS] = StringFromLiteral("out of memory. Token count exceeded."),
      [JSON_PARSER_ERROR_NO_OPENING_BRACKET] = StringFromLiteral("no opening bracket found"),
      [JSON_PARSER_ERROR_PARTIAL] = StringFromLiteral("partial json"),
      [JSON_PARSER_ERROR_INVALID_BOOLEAN] = StringFromLiteral("invalid boolean"),
      [JSON_PARSER_ERROR_INVALID_CHAR] = StringFromLiteral("invalid character"),
  };
  struct string *string = table + (u32)error;
  StringBuilderAppendString(sb, string);
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
  enum json_parser_test_error errorCode = JSON_PARSER_TEST_ERROR_NONE;

  // setup
  enum {
    KILOBYTES = (1 << 10),
  };
  u8 stackBuffer[8 * KILOBYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

  // u32 JsonParse(struct json_parser *parser, struct string *json)
  {
    struct test_case {
      struct string *json;
      struct {
        u32 tokenCount;
        struct json_token *tokens;
        struct string *strings;
      } expected;
    } testCases[] = {
        {
            .json = &StringFromLiteral("{ \"Lorem\": \"ipsum\" }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 20},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 8},
                            {.type = JSON_TOKEN_STRING, .start = 12, .end = 17},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"Lorem\": \"ipsum\" }"),
                            StringFromLiteral("Lorem"),
                            StringFromLiteral("ipsum"),
                        },
                },
        },
        {
            // '{ "Lorem": "ipsum \"dolor\"" }'
            .json = &StringFromLiteral("{ \"Lorem\": \"ipsum \\\"dolor\\\"\" }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 30},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 8},
                            {.type = JSON_TOKEN_STRING, .start = 12, .end = 27},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"Lorem\": \"ipsum \\\"dolor\\\"\" }"),
                            StringFromLiteral("Lorem"),
                            StringFromLiteral("ipsum \\\"dolor\\\""),
                        },
                },
        },
        {
            .json = &StringFromLiteral("{ \"Lorem\": \"ipsum\", \"dolor\": \"sit\" }"),
            .expected =
                {
                    .tokenCount = 5,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 36},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 8},
                            {.type = JSON_TOKEN_STRING, .start = 12, .end = 17},
                            {.type = JSON_TOKEN_STRING, .start = 21, .end = 26},
                            {.type = JSON_TOKEN_STRING, .start = 30, .end = 33},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"Lorem\": \"ipsum\", \"dolor\": \"sit\" }"),
                            StringFromLiteral("Lorem"),
                            StringFromLiteral("ipsum"),
                            StringFromLiteral("dolor"),
                            StringFromLiteral("sit"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("{ \"positive number\": 97168748 }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0x00, .end = 0x1f},
                            {.type = JSON_TOKEN_STRING, .start = 0x03, .end = 0x12},
                            {.type = JSON_TOKEN_NUMBER, .start = 0x15, .end = 0x1d},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"positive number\": 97168748 }"),
                            StringFromLiteral("positive number"),
                            StringFromLiteral("97168748"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("{ \"negative number\": -27845898 }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0x00, .end = 0x20},
                            {.type = JSON_TOKEN_STRING, .start = 0x03, .end = 0x12},
                            {.type = JSON_TOKEN_NUMBER, .start = 0x15, .end = 0x1e},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"negative number\": -27845898 }"),
                            StringFromLiteral("negative number"),
                            StringFromLiteral("-27845898"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("{ \"decimal numeral\": 7415.2305 }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0x00, .end = 0x20},
                            {.type = JSON_TOKEN_STRING, .start = 0x03, .end = 0x12},
                            {.type = JSON_TOKEN_NUMBER, .start = 0x15, .end = 0x1e},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"decimal numeral\": 7415.2305 }"),
                            StringFromLiteral("decimal numeral"),
                            StringFromLiteral("7415.2305"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("{ \"a\": false }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 14},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 4},
                            {.type = JSON_TOKEN_BOOLEAN_FALSE, .start = 7, .end = 12},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"a\": false }"),
                            StringFromLiteral("a"),
                            StringFromLiteral("false"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("{ \"a\": true }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 13},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 4},
                            {.type = JSON_TOKEN_BOOLEAN_TRUE, .start = 7, .end = 11},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"a\": true }"),
                            StringFromLiteral("a"),
                            StringFromLiteral("true"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("{ \"a\": null }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 13},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 4},
                            {.type = JSON_TOKEN_NULL, .start = 7, .end = 11},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"a\": null }"),
                            StringFromLiteral("a"),
                            StringFromLiteral("null"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("[ 1, 20, 300 ]"),
            .expected =
                {
                    .tokenCount = 4,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_ARRAY, .start = 0, .end = 14},
                            {.type = JSON_TOKEN_NUMBER, .start = 2, .end = 3},
                            {.type = JSON_TOKEN_NUMBER, .start = 5, .end = 7},
                            {.type = JSON_TOKEN_NUMBER, .start = 9, .end = 12},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("[ 1, 20, 300 ]"),
                            StringFromLiteral("1"),
                            StringFromLiteral("20"),
                            StringFromLiteral("300"),
                        },
                },
        },
        {
            .json = &StringFromLiteral(
                "{ \"title\": \"Test Video\", \"length\": 329021, \"allowedRegions\": [ \"US\", \"CA\", \"DE\" ] }"),
            .expected =
                {
                    .tokenCount = 10,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 83},
                            // title
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 8},
                            {.type = JSON_TOKEN_STRING, .start = 12, .end = 22},
                            // length
                            {.type = JSON_TOKEN_STRING, .start = 26, .end = 32},
                            {.type = JSON_TOKEN_NUMBER, .start = 35, .end = 41},
                            // allowed regions
                            {.type = JSON_TOKEN_STRING, .start = 44, .end = 58},
                            {.type = JSON_TOKEN_ARRAY, .start = 61, .end = 81},
                            {.type = JSON_TOKEN_STRING, .start = 64, .end = 66},
                            {.type = JSON_TOKEN_STRING, .start = 70, .end = 72},
                            {.type = JSON_TOKEN_STRING, .start = 76, .end = 78},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("{ \"title\": \"Test Video\", \"length\": 329021, "
                                              "\"allowedRegions\": [ \"US\", \"CA\", \"DE\" ] }"),
                            StringFromLiteral("title"),
                            StringFromLiteral("Test Video"),
                            StringFromLiteral("length"),
                            StringFromLiteral("329021"),
                            StringFromLiteral("allowedRegions"),
                            StringFromLiteral("[ \"US\", \"CA\", \"DE\" ]"),
                            StringFromLiteral("US"),
                            StringFromLiteral("CA"),
                            StringFromLiteral("DE"),
                        },
                },
        },
        {
            .json = &StringFromLiteral("[ { \"grades\": [10, 20, 30] }, { \"grades\": [100, 90, 80]} ]"),
            .expected =
                {
                    .tokenCount = 13,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_ARRAY, .start = 0, .end = 58},
                            {.type = JSON_TOKEN_OBJECT, .start = 2, .end = 28},
                            {.type = JSON_TOKEN_STRING, .start = 5, .end = 11},
                            {.type = JSON_TOKEN_ARRAY, .start = 14, .end = 26},
                            {.type = JSON_TOKEN_NUMBER, .start = 15, .end = 17},
                            {.type = JSON_TOKEN_NUMBER, .start = 19, .end = 21},
                            {.type = JSON_TOKEN_NUMBER, .start = 23, .end = 25},
                            {.type = JSON_TOKEN_OBJECT, .start = 30, .end = 56},
                            {.type = JSON_TOKEN_STRING, .start = 33, .end = 39},
                            {.type = JSON_TOKEN_ARRAY, .start = 42, .end = 55},
                            {.type = JSON_TOKEN_NUMBER, .start = 43, .end = 46},
                            {.type = JSON_TOKEN_NUMBER, .start = 48, .end = 50},
                            {.type = JSON_TOKEN_NUMBER, .start = 52, .end = 54},
                        },
                    .strings =
                        (struct string[]){
                            StringFromLiteral("[ { \"grades\": [10, 20, 30] }, { \"grades\": [100, 90, 80]} ]"),
                            StringFromLiteral("{ \"grades\": [10, 20, 30] }"),
                            StringFromLiteral("grades"),
                            StringFromLiteral("[10, 20, 30]"),
                            StringFromLiteral("10"),
                            StringFromLiteral("20"),
                            StringFromLiteral("30"),
                            StringFromLiteral("{ \"grades\": [100, 90, 80]}"),
                            StringFromLiteral("grades"),
                            StringFromLiteral("[100, 90, 80]"),
                            StringFromLiteral("100"),
                            StringFromLiteral("90"),
                            StringFromLiteral("80"),
                        },
                },
        },
        {
            .json = &StringFromLiteral(""),
            .expected =
                {
                    .tokenCount = 0,
                },
        },
        {
            .json = &StringFromLiteral("not a json"),
            .expected =
                {
                    .tokenCount = 0,
                },
        },
        {
            .json = &StringFromLiteral("{ not a json"),
            .expected =
                {
                    .tokenCount = 0,
                },
        },
    };

    u32 failedTestCount = 0;
    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      memory_temp tempMemory = MemoryTempBegin(&stackMemory); // for token allocations
      struct test_case *testCase = testCases + testCaseIndex;

      u32 expectedTokenCount = testCase->expected.tokenCount;
      struct json_token *expectedTokens = testCase->expected.tokens;
      struct string *json = testCase->json;

      u32 allocatedTokenCount = 128;
      struct json_parser *parser = MakeJsonParser(tempMemory.arena, allocatedTokenCount);

      b8 expectedValue = expectedTokenCount > 0;
      b8 gotValue = JsonParse(parser, json);
      if (gotValue != expectedValue) {
        errorCode =
            expectedValue ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\n");
          StringBuilderAppendPrintableHexDump(sb, json);
        }

        StringBuilderAppendStringLiteral(sb, "\n  expected to return: ");
        StringBuilderAppendBool(sb, expectedValue);
        StringBuilderAppendStringLiteral(sb, "\n             but got: ");
        StringBuilderAppendBool(sb, gotValue);
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      u32 gotTokenCount = parser->tokenCount;
      if (gotTokenCount != expectedTokenCount) {
        errorCode =
            expectedValue ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\n");
          StringBuilderAppendPrintableHexDump(sb, json);
        }

        StringBuilderAppendStringLiteral(sb, "\n  expected ");
        StringBuilderAppendU64(sb, expectedTokenCount);
        StringBuilderAppendStringLiteral(sb, " token(s) to be parsed but got ");
        StringBuilderAppendU64(sb, gotTokenCount);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
        continue;
      }

      // if tokenCount is correct

      for (u32 tokenIndex = 0; tokenIndex < parser->tokenCount; tokenIndex++) {
        struct json_token *token = parser->tokens + tokenIndex;
        struct json_token *expectedToken = expectedTokens + tokenIndex;

        if (token->type == expectedToken->type && token->start == expectedToken->start &&
            token->end == expectedToken->end) {
          struct string *expectedString = testCase->expected.strings + tokenIndex;
          struct string string = JsonTokenExtractString(token, json);
          if (IsStringEqual(&string, expectedString))
            continue;
        }

        errorCode = expectedTokenCount ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                       : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        if (failedTestCount == 0) {
          StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
          StringBuilderAppendStringLiteral(sb, "\n");
          StringBuilderAppendPrintableHexDump(sb, json);
        }

        if (token->type != expectedToken->type) {
          StringBuilderAppendStringLiteral(sb, "\n  expected token type to be ");
          StringBuilderAppendJsonTokenType(sb, expectedToken->type);
          StringBuilderAppendStringLiteral(sb, " but got ");
          StringBuilderAppendJsonTokenType(sb, token->type);
          StringBuilderAppendStringLiteral(sb, " at index ");
          StringBuilderAppendU64(sb, tokenIndex);
        }

        else if (token->start != expectedToken->start) {
          StringBuilderAppendStringLiteral(sb, "\n  expected token with type ");
          StringBuilderAppendJsonTokenType(sb, expectedToken->type);
          StringBuilderAppendStringLiteral(sb, " start to be ");
          StringBuilderAppendU64(sb, expectedToken->start);
          StringBuilderAppendStringLiteral(sb, " but got ");
          StringBuilderAppendU64(sb, token->start);
          StringBuilderAppendStringLiteral(sb, " at index ");
          StringBuilderAppendU64(sb, tokenIndex);
        }

        else if (token->end != expectedToken->end) {
          StringBuilderAppendStringLiteral(sb, "\n  expected token with type ");
          StringBuilderAppendJsonTokenType(sb, token->type);
          StringBuilderAppendStringLiteral(sb, " end to be ");
          StringBuilderAppendU64(sb, expectedToken->end);
          StringBuilderAppendStringLiteral(sb, " but got ");
          StringBuilderAppendU64(sb, token->end);
          StringBuilderAppendStringLiteral(sb, " at index ");
          StringBuilderAppendU64(sb, tokenIndex);
        }

        else {
          struct string *expectedString = testCase->expected.strings + tokenIndex;
          struct string string = JsonTokenExtractString(token, json);
          StringBuilderAppendStringLiteral(sb, "\n  expected string extracted from token to be:");
          StringBuilderAppendStringLiteral(sb, "\n    \"");
          StringBuilderAppendString(sb, expectedString);
          StringBuilderAppendStringLiteral(sb, "\"");
          StringBuilderAppendStringLiteral(sb, "\n  but got:");
          StringBuilderAppendStringLiteral(sb, "\n    \"");
          StringBuilderAppendString(sb, &string);
          StringBuilderAppendStringLiteral(sb, "\"");
          StringBuilderAppendStringLiteral(sb, "\n  at index: ");
          StringBuilderAppendU64(sb, tokenIndex);
        }

        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);

        failedTestCount++;
      }

      MemoryTempEnd(&tempMemory);
    }
  }

  return (int)errorCode;
}
