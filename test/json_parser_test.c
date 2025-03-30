#include "json_parser.c"
#include "print.h"
#include "string_builder.h"

#define TEST_ERROR_LIST(XX)                                                                                            \
  XX(PARSE_EXPECTED_TRUE, "Json must be parsed successfully")                                                          \
  XX(PARSE_EXPECTED_FALSE, "Json must NOT be able to parse anything")                                                  \
  XX(PARSE_CHUNKED_JSON_EXPECTED_TRUE, "Chunked jsons must be parsed successfully")                                    \
  XX(PARSE_CHUNKED_JSON_EXPECTED_FALSE, "Chunked jsons must NOT be parsed")

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
#define XX(tag, msg) {.code = JSON_PARSER_TEST_ERROR_##tag, .message = STRING_FROM_ZERO_TERMINATED(msg)},
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
  struct string *boolString = value ? &STRING_FROM_ZERO_TERMINATED("true") : &STRING_FROM_ZERO_TERMINATED("false");
  StringBuilderAppendString(sb, boolString);
}

internalfn void
StringBuilderAppendJsonTokenType(string_builder *sb, enum json_token_type type)
{
  struct string table[] = {
      [JSON_TOKEN_NULL] = STRING_FROM_ZERO_TERMINATED("NULL"),
      [JSON_TOKEN_OBJECT] = STRING_FROM_ZERO_TERMINATED("OBJECT"),
      [JSON_TOKEN_ARRAY] = STRING_FROM_ZERO_TERMINATED("ARRAY"),
      [JSON_TOKEN_STRING] = STRING_FROM_ZERO_TERMINATED("STRING"),
      [JSON_TOKEN_BOOLEAN_FALSE] = STRING_FROM_ZERO_TERMINATED("BOOLEAN FALSE"),
      [JSON_TOKEN_BOOLEAN_TRUE] = STRING_FROM_ZERO_TERMINATED("BOOLEAN TRUE"),
      [JSON_TOKEN_NUMBER] = STRING_FROM_ZERO_TERMINATED("NUMBER"),
  };
  struct string *string = table + (u32)type;
  StringBuilderAppendString(sb, string);
}

internalfn void
StringBuilderAppendJsonParserError(string_builder *sb, enum json_parser_error error)
{
  struct string table[] = {
      [JSON_PARSER_ERROR_NONE] = STRING_FROM_ZERO_TERMINATED("no errors"),
      [JSON_PARSER_ERROR_OUT_OF_TOKENS] = STRING_FROM_ZERO_TERMINATED("out of memory. Token count exceeded."),
      [JSON_PARSER_ERROR_NO_OPENING_BRACKET] = STRING_FROM_ZERO_TERMINATED("no opening bracket found"),
      [JSON_PARSER_ERROR_PARTIAL] = STRING_FROM_ZERO_TERMINATED("partial json"),
      [JSON_PARSER_ERROR_INVALID_BOOLEAN] = STRING_FROM_ZERO_TERMINATED("invalid boolean"),
  };
  struct string *string = table + (u32)error;
  StringBuilderAppendString(sb, string);
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
  enum json_parser_test_error errorCode = JSON_PARSER_TEST_ERROR_NONE;

  // setup
  u32 KILOBYTES = 1 << 10;
  u8 stackBuffer[8 * KILOBYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

  // u32 JsonParse(struct json_parser *parser, struct string *json)
  {
    u32 allocatedTokenCount = 128;

    struct test_case {
      struct string *json;
      struct {
        u32 tokenCount;
        struct json_token *tokens;
        struct string *strings;
      } expected;
    } testCases[] = {
        {
            .json = &STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum\" }"),
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
                            STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum\" }"),
                            STRING_FROM_ZERO_TERMINATED("Lorem"),
                            STRING_FROM_ZERO_TERMINATED("ipsum"),
                        },
                },
        },
        {
            // '{ "Lorem": "ipsum \"dolor\"" }'
            .json = &STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum \\\"dolor\\\"\" }"),
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
                            STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum \\\"dolor\\\"\" }"),
                            STRING_FROM_ZERO_TERMINATED("Lorem"),
                            STRING_FROM_ZERO_TERMINATED("ipsum \\\"dolor\\\""),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum\", \"dolor\": \"sit\" }"),
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
                            STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum\", \"dolor\": \"sit\" }"),
                            STRING_FROM_ZERO_TERMINATED("Lorem"),
                            STRING_FROM_ZERO_TERMINATED("ipsum"),
                            STRING_FROM_ZERO_TERMINATED("dolor"),
                            STRING_FROM_ZERO_TERMINATED("sit"),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("{ \"a\": 1234 }"),
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 13},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 4},
                            {.type = JSON_TOKEN_NUMBER, .start = 7, .end = 11},
                        },
                    .strings =
                        (struct string[]){
                            STRING_FROM_ZERO_TERMINATED("{ \"a\": 1234 }"),
                            STRING_FROM_ZERO_TERMINATED("a"),
                            STRING_FROM_ZERO_TERMINATED("1234"),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("{ \"a\": false }"),
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
                            STRING_FROM_ZERO_TERMINATED("{ \"a\": false }"),
                            STRING_FROM_ZERO_TERMINATED("a"),
                            STRING_FROM_ZERO_TERMINATED("false"),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("{ \"a\": true }"),
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
                            STRING_FROM_ZERO_TERMINATED("{ \"a\": true }"),
                            STRING_FROM_ZERO_TERMINATED("a"),
                            STRING_FROM_ZERO_TERMINATED("true"),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("[ 1, 20, 300 ]"),
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
                            STRING_FROM_ZERO_TERMINATED("[ 1, 20, 300 ]"),
                            STRING_FROM_ZERO_TERMINATED("1"),
                            STRING_FROM_ZERO_TERMINATED("20"),
                            STRING_FROM_ZERO_TERMINATED("300"),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED(
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
                            STRING_FROM_ZERO_TERMINATED("{ \"title\": \"Test Video\", \"length\": 329021, "
                                                        "\"allowedRegions\": [ \"US\", \"CA\", \"DE\" ] }"),
                            STRING_FROM_ZERO_TERMINATED("title"),
                            STRING_FROM_ZERO_TERMINATED("Test Video"),
                            STRING_FROM_ZERO_TERMINATED("length"),
                            STRING_FROM_ZERO_TERMINATED("329021"),
                            STRING_FROM_ZERO_TERMINATED("allowedRegions"),
                            STRING_FROM_ZERO_TERMINATED("[ \"US\", \"CA\", \"DE\" ]"),
                            STRING_FROM_ZERO_TERMINATED("US"),
                            STRING_FROM_ZERO_TERMINATED("CA"),
                            STRING_FROM_ZERO_TERMINATED("DE"),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("[ { \"grades\": [10, 20, 30] }, { \"grades\": [100, 90, 80]} ]"),
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
                            STRING_FROM_ZERO_TERMINATED(
                                "[ { \"grades\": [10, 20, 30] }, { \"grades\": [100, 90, 80]} ]"),
                            STRING_FROM_ZERO_TERMINATED("{ \"grades\": [10, 20, 30] }"),
                            STRING_FROM_ZERO_TERMINATED("grades"),
                            STRING_FROM_ZERO_TERMINATED("[10, 20, 30]"),
                            STRING_FROM_ZERO_TERMINATED("10"),
                            STRING_FROM_ZERO_TERMINATED("20"),
                            STRING_FROM_ZERO_TERMINATED("30"),
                            STRING_FROM_ZERO_TERMINATED("{ \"grades\": [100, 90, 80]}"),
                            STRING_FROM_ZERO_TERMINATED("grades"),
                            STRING_FROM_ZERO_TERMINATED("[100, 90, 80]"),
                            STRING_FROM_ZERO_TERMINATED("100"),
                            STRING_FROM_ZERO_TERMINATED("90"),
                            STRING_FROM_ZERO_TERMINATED("80"),
                        },
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED(""),
            .expected =
                {
                    .tokenCount = 0,
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("not a json"),
            .expected =
                {
                    .tokenCount = 0,
                },
        },
        {
            .json = &STRING_FROM_ZERO_TERMINATED("{ not a json"),
            .expected =
                {
                    .tokenCount = 0,
                },
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      memory_temp tempMemory = MemoryTempBegin(&stackMemory); // for token allocations
      struct test_case *testCase = testCases + testCaseIndex;

      u32 expectedTokenCount = testCase->expected.tokenCount;
      struct json_token *expectedTokens = testCase->expected.tokens;
      struct string *json = testCase->json;

      struct json_token *tokens = MemoryArenaPushUnaligned(tempMemory.arena, sizeof(*tokens) * allocatedTokenCount);
      struct json_parser parser = JsonParser(tokens, allocatedTokenCount);

      u32 tokenCount = JsonParse(&parser, json);
      if (tokenCount != expectedTokenCount) {
        errorCode = expectedTokenCount ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                       : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  json: "));
        StringBuilderAppendString(sb, json);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected "));
        StringBuilderAppendU64(sb, expectedTokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" token(s) to be parsed but got "));
        StringBuilderAppendU64(sb, tokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      } else {
        // if tokenCount is correct

        u32 wrongTokenCount = 0;
        for (u32 tokenIndex = 0; tokenIndex < tokenCount; tokenIndex++) {
          struct json_token *token = tokens + tokenIndex;
          struct json_token *expectedToken = expectedTokens + tokenIndex;

          if (token->type == expectedToken->type && token->start == expectedToken->start &&
              token->end == expectedToken->end) {
            struct string *expectedString = testCase->expected.strings + tokenIndex;
            struct string string = JsonTokenExtractString(token, json);
            if (IsStringEqual(&string, expectedString))
              continue;
          }

          if (wrongTokenCount == 0) {
            errorCode = expectedTokenCount ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                           : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;
            StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  json: "));
            StringBuilderAppendString(sb, json);
          }

          if (token->type != expectedToken->type) {
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected token type to be "));
            StringBuilderAppendJsonTokenType(sb, expectedToken->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" but got "));
            StringBuilderAppendJsonTokenType(sb, token->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          else if (token->start != expectedToken->start) {
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected token with type "));
            StringBuilderAppendJsonTokenType(sb, expectedToken->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" start to be "));
            StringBuilderAppendU64(sb, expectedToken->start);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" but got "));
            StringBuilderAppendU64(sb, token->start);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          else if (token->end != expectedToken->end) {
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected token with type "));
            StringBuilderAppendJsonTokenType(sb, token->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" end to be "));
            StringBuilderAppendU64(sb, expectedToken->end);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" but got "));
            StringBuilderAppendU64(sb, token->end);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          else {
            struct string *expectedString = testCase->expected.strings + tokenIndex;
            struct string string = JsonTokenExtractString(token, json);
            StringBuilderAppendString(sb,
                                      &STRING_FROM_ZERO_TERMINATED("\n  expected string extracted from token to be:"));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    \""));
            StringBuilderAppendString(sb, expectedString);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\""));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  but got:"));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    \""));
            StringBuilderAppendString(sb, &string);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\""));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  at index: "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
          struct string errorMessage = StringBuilderFlush(sb);
          PrintString(&errorMessage);

          wrongTokenCount++;
        }
      }

      MemoryTempEnd(&tempMemory);
    }
  }

  {
    u32 allocatedTokenCount = 128;

    struct test_case {
      u32 chunkedJsonCount;
      struct string *chunkedJsons;
      struct {
        u32 tokenCount;
        struct json_token *tokens;
        struct string *strings;
      } expected;
    } testCases[] = {
        {
            .chunkedJsonCount = 2,
            .chunkedJsons =
                (struct string[]){
                    STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": "),
                    STRING_FROM_ZERO_TERMINATED("\"ipsum\" }"),
                },
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
                            STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum\" }"),
                            STRING_FROM_ZERO_TERMINATED("Lorem"),
                            STRING_FROM_ZERO_TERMINATED("ipsum"),
                        },
                },
        },
        // partial string
        {
            .chunkedJsonCount = 2,
            .chunkedJsons =
                (struct string[]){
                    STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ip"),
                    STRING_FROM_ZERO_TERMINATED("sum\" }"),
                },
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
                            STRING_FROM_ZERO_TERMINATED("{ \"Lorem\": \"ipsum\" }"),
                            STRING_FROM_ZERO_TERMINATED("Lorem"),
                            STRING_FROM_ZERO_TERMINATED("ipsum"),
                        },
                },
        },
        // partial boolean
        {
            .chunkedJsonCount = 2,
            .chunkedJsons =
                (struct string[]){
                    STRING_FROM_ZERO_TERMINATED("{ \"a\": tr"),
                    STRING_FROM_ZERO_TERMINATED("ue }"),
                },
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
                            STRING_FROM_ZERO_TERMINATED("{ \"a\": true }"),
                            STRING_FROM_ZERO_TERMINATED("a"),
                            STRING_FROM_ZERO_TERMINATED("true"),
                        },
                },
        },
        {
            .chunkedJsonCount = 2,
            .chunkedJsons =
                (struct string[]){
                    STRING_FROM_ZERO_TERMINATED("{ \"a\": fa"),
                    STRING_FROM_ZERO_TERMINATED("lse }"),
                },
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
                            STRING_FROM_ZERO_TERMINATED("{ \"a\": false }"),
                            STRING_FROM_ZERO_TERMINATED("a"),
                            STRING_FROM_ZERO_TERMINATED("false"),
                        },
                },
        },
        // partial number
        {
            .chunkedJsonCount = 2,
            .chunkedJsons =
                (struct string[]){
                    STRING_FROM_ZERO_TERMINATED("{ \"a\": 234"),
                    STRING_FROM_ZERO_TERMINATED("093 }"),
                },
            .expected =
                {
                    .tokenCount = 3,
                    .tokens =
                        (struct json_token[]){
                            {.type = JSON_TOKEN_OBJECT, .start = 0, .end = 15},
                            {.type = JSON_TOKEN_STRING, .start = 3, .end = 4},
                            {.type = JSON_TOKEN_NUMBER, .start = 7, .end = 13},
                        },
                    .strings =
                        (struct string[]){
                            STRING_FROM_ZERO_TERMINATED("{ \"a\": 234093 }"),
                            STRING_FROM_ZERO_TERMINATED("a"),
                            STRING_FROM_ZERO_TERMINATED("234093"),
                        },
                },
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      memory_temp tempMemory = MemoryTempBegin(&stackMemory); // for token allocations
      struct test_case *testCase = testCases + testCaseIndex;

      u32 expectedTokenCount = testCase->expected.tokenCount;
      struct json_token *expectedTokens = testCase->expected.tokens;

      // combine chunked json strings for printing
      struct string *chunkedJsons = testCase->chunkedJsons;
      u32 chunkedJsonCount = testCase->chunkedJsonCount;
      u64 jsonLength = 0;
      for (u32 chunkedJsonIndex = 0; chunkedJsonIndex < chunkedJsonCount; chunkedJsonIndex++) {
        struct string *chunkedJson = chunkedJsons + chunkedJsonIndex;
        jsonLength += chunkedJson->length;
      }
      string_builder *jsonBuilder = MakeStringBuilder(tempMemory.arena, jsonLength, 0);
      for (u32 chunkedJsonIndex = 0; chunkedJsonIndex < chunkedJsonCount; chunkedJsonIndex++) {
        struct string *chunkedJson = chunkedJsons + chunkedJsonIndex;
        StringBuilderAppendString(jsonBuilder, chunkedJson);
      }
      struct string json = StringBuilderFlush(jsonBuilder);

      // parse chunked json
      struct json_token *tokens = MemoryArenaPushUnaligned(tempMemory.arena, sizeof(*tokens) * allocatedTokenCount);
      struct json_parser parser = JsonParser(tokens, allocatedTokenCount);

      u32 totalTokenCount = 0;
      for (u32 chunkedJsonIndex = 0; chunkedJsonIndex < chunkedJsonCount; chunkedJsonIndex++) {
        struct string *chunkedJson = chunkedJsons + chunkedJsonIndex;
        u32 tokenCount = JsonParse(&parser, chunkedJson);
        if (!(parser.error == JSON_PARSER_ERROR_NONE || parser.error == JSON_PARSER_ERROR_PARTIAL)) {
          break;
        }
        totalTokenCount += tokenCount;
      }

      if (parser.error) {
        errorCode = expectedTokenCount ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                       : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        for (u32 chunkedJsonIndex = 0; chunkedJsonIndex < chunkedJsonCount; chunkedJsonIndex++) {
          struct string *chunkedJson = chunkedJsons + chunkedJsonIndex;
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  json "));
          StringBuilderAppendU64(sb, chunkedJsonIndex);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(": "));
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(length: "));
          StringBuilderAppendU64(sb, chunkedJson->length);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(") '"));
          StringBuilderAppendString(sb, chunkedJson);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected to be parsed without any errors"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  but got "));
        StringBuilderAppendJsonParserError(sb, parser.error);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      } else if (totalTokenCount != expectedTokenCount) {
        errorCode = expectedTokenCount ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                       : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        for (u32 chunkedJsonIndex = 0; chunkedJsonIndex < chunkedJsonCount; chunkedJsonIndex++) {
          struct string *chunkedJson = chunkedJsons + chunkedJsonIndex;
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  json "));
          StringBuilderAppendU64(sb, chunkedJsonIndex);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(": "));
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(length: "));
          StringBuilderAppendU64(sb, chunkedJson->length);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(") '"));
          StringBuilderAppendString(sb, chunkedJson);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected "));
        StringBuilderAppendU64(sb, expectedTokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" token(s) to be parsed but got "));
        StringBuilderAppendU64(sb, totalTokenCount);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      } else {
        // if tokenCount is correct

        u32 wrongTokenCount = 0;
        for (u32 tokenIndex = 0; tokenIndex < totalTokenCount; tokenIndex++) {
          struct json_token *token = tokens + tokenIndex;
          struct json_token *expectedToken = expectedTokens + tokenIndex;

          if (token->type == expectedToken->type && token->start == expectedToken->start &&
              token->end == expectedToken->end) {
            struct string *expectedString = testCase->expected.strings + tokenIndex;
            struct string string = JsonTokenExtractString(token, &json);
            if (IsStringEqual(&string, expectedString))
              continue;
          }

          errorCode = expectedTokenCount ? JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_TRUE
                                         : JSON_PARSER_TEST_ERROR_PARSE_EXPECTED_FALSE;

          if (wrongTokenCount == 0) {
            StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));

            for (u32 chunkedJsonIndex = 0; chunkedJsonIndex < chunkedJsonCount; chunkedJsonIndex++) {
              struct string *chunkedJson = chunkedJsons + chunkedJsonIndex;
              StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  json "));
              StringBuilderAppendU64(sb, chunkedJsonIndex);
              StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(": "));
              StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("(length: "));
              StringBuilderAppendU64(sb, chunkedJson->length);
              StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(") '"));
              StringBuilderAppendString(sb, chunkedJson);
              StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
            }
          }

          if (token->type != expectedToken->type) {
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected token type to be "));
            StringBuilderAppendJsonTokenType(sb, expectedToken->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" but got "));
            StringBuilderAppendJsonTokenType(sb, token->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          else if (token->start != expectedToken->start) {
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected token with type "));
            StringBuilderAppendJsonTokenType(sb, expectedToken->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" start to be "));
            StringBuilderAppendU64(sb, expectedToken->start);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" but got "));
            StringBuilderAppendU64(sb, token->start);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          else if (token->end != expectedToken->end) {
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected token with type "));
            StringBuilderAppendJsonTokenType(sb, token->type);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" end to be "));
            StringBuilderAppendU64(sb, expectedToken->end);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" but got "));
            StringBuilderAppendU64(sb, token->end);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" at index "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          else {
            struct string *expectedString = testCase->expected.strings + tokenIndex;
            struct string string = JsonTokenExtractString(token, &json);
            StringBuilderAppendString(sb,
                                      &STRING_FROM_ZERO_TERMINATED("\n  expected string extracted from token to be:"));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    \""));
            StringBuilderAppendString(sb, expectedString);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\""));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  but got:"));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n    \""));
            StringBuilderAppendString(sb, &string);
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\""));
            StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  at index: "));
            StringBuilderAppendU64(sb, tokenIndex);
          }

          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
          struct string errorMessage = StringBuilderFlush(sb);
          PrintString(&errorMessage);

          wrongTokenCount++;
        }
      }

      MemoryTempEnd(&tempMemory);
    }
  }

  return (int)errorCode;
}
