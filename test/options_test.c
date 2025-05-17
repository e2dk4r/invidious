#include "options.c"
#include "print.h"
#include "string_builder.h"

#define TEST_ERROR_LIST(X)                                                                                             \
  X(PARSE_EXPECTED_TRUE, "Options must be parsed successfully")                                                        \
  X(PARSE_EXPECTED_FALSE, "Options must NOT be able to parsed")                                                        \
  X(PARSE_EXPECTED_VIDEOID, "Video id must match with expected")

enum options_test_error {
  OPTIONS_TEST_ERROR_NONE = 0,
#define X(tag, message) OPTIONS_TEST_ERROR_##tag,
  TEST_ERROR_LIST(X)
#undef X

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

internalfn void
StringBuilderAppendErrorMessage(string_builder *sb, enum options_test_error errorCode)
{
  struct error {
    enum options_test_error code;
    struct string message;
  } errors[] = {
#define X(tag, msg) {.code = OPTIONS_TEST_ERROR_##tag, .message = StringFromLiteral(msg)},
      TEST_ERROR_LIST(X)
#undef X
  };

  string message = StringFromLiteral("Unknown error");
  for (u32 index = 0; index < ARRAY_COUNT(errors); index++) {
    struct error *error = errors + index;
    if (error->code == errorCode) {
      message = error->message;
      break;
    }
  }

  StringBuilderAppendString(sb, &message);
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

int
main(void)
{
  enum options_test_error errorCode = OPTIONS_TEST_ERROR_NONE;

  // setup
  u32 KILOBYTES = 1 << 10;
  u8 stackBuffer[8 * KILOBYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

  // b8 OptionsParse(struct options *options, u32 argumentCount, char **arguments)
  {
    struct test_case {
      u32 argumentCount;
      char **arguments;
      struct {
        b8 value;
        struct string videoId;
      } expected;
    } testCases[] = {
        {
            .argumentCount = 2,
            .arguments =
                (char *[]){
                    "program",
                    "https://www.youtube.com/watch?v=d_oVysaqG_0",
                },
            .expected =
                {
                    .value = 1,
                    .videoId = StringFromLiteral("d_oVysaqG_0"),
                },
        },
        {
            .argumentCount = 2,
            .arguments =
                (char *[]){
                    "program",
                    "https://www.youtube.com/watch?v=d_oVysaqG_0&t=10",
                },
            .expected =
                {
                    .value = 1,
                    .videoId = StringFromLiteral("d_oVysaqG_0"),
                },
        },
        {
            .argumentCount = 2,
            .arguments =
                (char *[]){
                    "program",
                    "https://www.youtube.com/watch?v=d_oVysaqG_0#comments",
                },
            .expected =
                {
                    .value = 1,
                    .videoId = StringFromLiteral("d_oVysaqG_0"),
                },
        },
        {
            .argumentCount = 2,
            .arguments =
                (char *[]){
                    "program",
                    "https://www.youtube.com/watch?v=d_oVysaqG_0&t=10#comments",
                },
            .expected =
                {
                    .value = 1,
                    .videoId = StringFromLiteral("d_oVysaqG_0"),
                },
        },
        {
            .argumentCount = 2,
            .arguments =
                (char *[]){
                    "program",
                    "https://www.youtube.com/embed/d_oVysaqG_0",
                },
            .expected =
                {
                    .value = 1,
                    .videoId = StringFromLiteral("d_oVysaqG_0"),
                },
        },
        {
            .argumentCount = 2,
            .arguments =
                (char *[]){
                    "program",
                    "https://youtu.be/d_oVysaqG_0",
                },
            .expected =
                {
                    .value = 1,
                    .videoId = StringFromLiteral("d_oVysaqG_0"),
                },
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      struct options optionsBuffer = {};
      struct options *options = &optionsBuffer;
      u32 argumentCount = testCase->argumentCount;
      char **arguments = testCase->arguments;

      b8 expected = testCase->expected.value;
      b8 got = OptionsParse(options, argumentCount, arguments);

      if (got != expected) {
        errorCode = expected ? OPTIONS_TEST_ERROR_PARSE_EXPECTED_TRUE : OPTIONS_TEST_ERROR_PARSE_EXPECTED_FALSE;

        StringBuilderAppendErrorMessage(sb, errorCode);
        StringBuilderAppendStringLiteral(sb, "\n  expected: ");
        StringBuilderAppendBool(sb, expected);
        StringBuilderAppendStringLiteral(sb, "\n       got: ");
        StringBuilderAppendBool(sb, got);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
        continue;
      }

      struct string *expectedVideoId = &testCase->expected.videoId;
      struct string *videoId = &options->videoId;
      if (!IsStringEqual(videoId, expectedVideoId)) {
        errorCode = OPTIONS_TEST_ERROR_PARSE_EXPECTED_VIDEOID;

        StringBuilderAppendErrorMessage(sb, errorCode);
        StringBuilderAppendStringLiteral(sb, "\n  expected: ");
        StringBuilderAppendPrintableString(sb, expectedVideoId);
        StringBuilderAppendStringLiteral(sb, "\n       got: ");
        StringBuilderAppendPrintableString(sb, videoId);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
        continue;
      }
    }
  }

  return (int)errorCode;
}
