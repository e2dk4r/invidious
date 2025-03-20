#include "http_parser.c"
#include "log.h"
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
GetTestErrorMessage(enum http_parser_test_error errorCode)
{
  for (u32 index = 0; index < ARRAY_COUNT(TEXT_TEST_ERRORS); index++) {
    const struct http_parser_test_error_info *info = TEXT_TEST_ERRORS + index;
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
  enum http_parser_test_error errorCode = HTTP_PARSER_TEST_ERROR_NONE;

  // setup
  u32 KILOBYTES = 1 << 10;
  u8 stackBuffer[8 * KILOBYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

  return (int)errorCode;
}
