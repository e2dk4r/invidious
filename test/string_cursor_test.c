#include "print.h"
#include "string_builder.h"
#include "string_cursor.h"

#define TEST_ERROR_LIST(XX)                                                                                            \
  XX(STRING_CURSOR_TEST_ERROR_STARTS_WITH_EXPECTED_TRUE, "Expected string to start with the given prefix")             \
  XX(STRING_CURSOR_TEST_ERROR_STARTS_WITH_EXPECTED_FALSE, "Expected string NOT to start with the given prefix")        \
  XX(STRING_CURSOR_TEST_ERROR_ADVANCE_AFTER_EXPECTED, "Cursor must advance after search string or go to end")          \
  XX(STRING_CURSOR_TEST_ERROR_IS_REMAINING_EQUAL_EXPECTED_TRUE, "Remaining text must match the search string")         \
  XX(STRING_CURSOR_TEST_ERROR_IS_REMAINING_EQUAL_EXPECTED_FALSE, "Remaining text must NOT match the search string")    \
  XX(STRING_CURSOR_TEST_ERROR_CONSUME_UNTIL_EXPECTED, "Text consumed must match the expected")                         \
  XX(STRING_CURSOR_TEST_ERROR_EXTRACT_THROUGH_EXPECTED, "Text extracted through search must match the expected")       \
  XX(STRING_CURSOR_TEST_ERROR_EXTRACT_NUMBER_EXPECTED_TRUE, "Number must be extracted from cursor position")           \
  XX(STRING_CURSOR_TEST_ERROR_EXTRACT_NUMBER_EXPECTED_FALSE, "Number must NOT be extracted from cursor position")

enum string_cursor_test_error {
  STRING_CURSOR_TEST_ERROR_NONE = 0,
#define XX(tag, message) tag,
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

comptime struct string_cursor_test_error_info {
  enum string_cursor_test_error code;
  struct string message;
} TEXT_TEST_ERRORS[] = {
#define XX(tag, msg) {.code = tag, .message = STRING_FROM_ZERO_TERMINATED(msg)},
    TEST_ERROR_LIST(XX)
#undef XX
};

internalfn string *
GetTextTestErrorMessage(enum string_cursor_test_error errorCode)
{
  for (u32 index = 0; index < ARRAY_COUNT(TEXT_TEST_ERRORS); index++) {
    const struct string_cursor_test_error_info *info = TEXT_TEST_ERRORS + index;
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

int
main(void)
{
  enum string_cursor_test_error errorCode = STRING_CURSOR_TEST_ERROR_NONE;

  // setup
  u32 KILOBYTES = 1 << 10;
  u8 stackBuffer[8 * KILOBYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MemoryArenaPushUnaligned(&stackMemory, sizeof(*sb));
  {
    string *outBuffer = MemoryArenaPushUnaligned(&stackMemory, sizeof(*outBuffer));
    outBuffer->length = 1024;
    outBuffer->value = MemoryArenaPushUnaligned(&stackMemory, outBuffer->length);
    sb->outBuffer = outBuffer;

    string *stringBuffer = MemoryArenaPushUnaligned(&stackMemory, sizeof(*stringBuffer));
    stringBuffer->length = 32;
    stringBuffer->value = MemoryArenaPushUnaligned(&stackMemory, stringBuffer->length);
    sb->stringBuffer = stringBuffer;

    sb->length = 0;
  }

  // b8 IsStringCursorStartsWith(struct string_cursor *cursor, struct string *search)
  {
    struct test_case {
      struct string_cursor cursor;
      struct string *search;
      b8 expected;
    } testCases[] = {
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED(" Lorem Ipsum"),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED(" Lorem Ipsum "),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 0,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("abc"),
            .expected = 0,
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      b8 expected = testCase->expected;
      struct string_cursor *cursor = &testCase->cursor;
      struct string *search = testCase->search;

      b8 got = IsStringCursorStartsWith(cursor, search);
      if (got != expected) {
        errorCode = expected ? STRING_CURSOR_TEST_ERROR_STARTS_WITH_EXPECTED_TRUE
                             : STRING_CURSOR_TEST_ERROR_STARTS_WITH_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  cursor: '"));
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("' at position: "));
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  search: '"));
        StringBuilderAppendString(sb, search);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected: "));
        StringBuilderAppendBool(sb, expected);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got: "));
        StringBuilderAppendBool(sb, got);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  }

  // b8 IsStringCursorRemainingEqual(struct string_cursor *cursor, struct string *search)
  b8 IsStringCursorRemainingEqualOK = 1;
  {
    struct test_case {
      struct string_cursor cursor;
      struct string *search;
      b8 expected;
    } testCases[] = {
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED(" Lorem Ipsum"),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED(" Lorem Ipsum "),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 0,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
            .expected = 0,
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("abc"),
            .expected = 0,
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      b8 expected = testCase->expected;
      struct string_cursor *cursor = &testCase->cursor;
      struct string *search = testCase->search;

      b8 got = IsStringCursorRemainingEqual(cursor, search);
      if (got != expected) {
        IsStringCursorRemainingEqualOK = 0;
        errorCode = expected ? STRING_CURSOR_TEST_ERROR_IS_REMAINING_EQUAL_EXPECTED_TRUE
                             : STRING_CURSOR_TEST_ERROR_IS_REMAINING_EQUAL_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  cursor: '"));
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("' at position: "));
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  search: '"));
        StringBuilderAppendString(sb, search);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected: "));
        StringBuilderAppendBool(sb, expected);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got: "));
        StringBuilderAppendBool(sb, got);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  }

  // b8 StringCursorAdvanceAfter(struct string_cursor *cursor, struct string *search)
  // Dependencies (for testing): IsStringCursorRemainingEqual()
  if (IsStringCursorRemainingEqualOK) {
    struct test_case {
      struct string_cursor cursor;
      struct string *search;
      struct {
        b8 value;
        struct string *remaining;
        u64 position;
      } expected;
    } testCases[] = {
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem"),
            .expected =
                {
                    .value = 1,
                    .remaining = &STRING_FROM_ZERO_TERMINATED(" Ipsum"),
                },
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Ipsum"),
            .expected =
                {
                    .value = 1,
                    .remaining = &STRING_FROM_ZERO_TERMINATED(""),
                },
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem"),
            .expected =
                {
                    .value = 0,
                    .position = STRING_FROM_ZERO_TERMINATED("Lorem Ipsum").length,
                },
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("abc"),
            .expected =
                {
                    .value = 0,
                    .position = STRING_FROM_ZERO_TERMINATED("Lorem Ipsum").length,
                },
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      b8 expectedToFindString = testCase->expected.value;
      u64 expectedPosition = testCase->expected.position;
      struct string *expectedRemaining = testCase->expected.remaining;
      struct string_cursor *cursor = &testCase->cursor;
      struct string *search = testCase->search;

      b8 got = StringCursorAdvanceAfter(cursor, search);
      if (got != expectedToFindString ||
          (expectedToFindString && !IsStringCursorRemainingEqual(cursor, expectedRemaining)) ||
          (!expectedToFindString && cursor->position != expectedPosition)) {
        errorCode = STRING_CURSOR_TEST_ERROR_ADVANCE_AFTER_EXPECTED;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  cursor: '"));
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("' at position: "));
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  search: '"));
        StringBuilderAppendString(sb, search);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        if (got != expectedToFindString) {
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected to find search: "));
          StringBuilderAppendBool(sb, expectedToFindString);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got: "));
          StringBuilderAppendBool(sb, got);
        } else if (!expectedToFindString && cursor->position != expectedPosition) {
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected cursor position: "));
          StringBuilderAppendU64(sb, expectedPosition);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got: "));
          StringBuilderAppendU64(sb, cursor->position);
        } else {
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n   expected: "));
          StringBuilderAppendString(sb, expectedRemaining);
          StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n        got: "));
          struct string remaining = StringCursorExtractSubstring(cursor, cursor->source->length - cursor->position);
          StringBuilderAppendString(sb, &remaining);
        }
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  } else {
    PrintString(&STRING_FROM_ZERO_TERMINATED(
        "StringCursorAdvanceAfter() tests are skipped because IsStringCursorRemainingEqual() failed\n"));
  }

  // struct string StringCursorConsumeUntil(struct string_cursor *cursor, struct string *search)
  {
    struct test_case {
      struct string_cursor cursor;
      struct string *search;
      struct string expected;
    } testCases[] = {
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem"),
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Ipsum"),
            .expected = STRING_FROM_ZERO_TERMINATED("Lorem "),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".2"),
            .expected = STRING_FROM_ZERO_TERMINATED("1"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                    .position = 2,
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".3"),
            .expected = STRING_FROM_ZERO_TERMINATED("2"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".3"),
            .expected = STRING_FROM_ZERO_TERMINATED("1.2"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                    .position = 2,
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".3"),
            .expected = STRING_FROM_ZERO_TERMINATED("2"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("abcdefgh"),
                },
            .search = &STRING_FROM_ZERO_TERMINATED("012345"),
            .expected = STRING_FROM_ZERO_TERMINATED("abcdefgh"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("abcdefgh"),
                    .position = 2,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("012345"),
            .expected = STRING_FROM_ZERO_TERMINATED("cdefgh"),
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      struct string *expected = &testCase->expected;
      struct string_cursor *cursor = &testCase->cursor;
      struct string *search = testCase->search;

      struct string got = StringCursorConsumeUntil(cursor, search);
      if (!IsStringEqual(&got, expected)) {
        errorCode = STRING_CURSOR_TEST_ERROR_CONSUME_UNTIL_EXPECTED;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  cursor: '"));
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("' at position: "));
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  search: '"));
        StringBuilderAppendString(sb, search);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected: "));
        StringBuilderAppendPrintableString(sb, expected);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got: "));
        StringBuilderAppendPrintableString(sb, &got);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  }

  // struct string StringCursorExtractThrough(struct string_cursor *cursor, struct string *search)
  {
    struct test_case {
      struct string_cursor cursor;
      struct string *search;
      struct string expected;
    } testCases[] = {
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Lorem"),
            .expected = STRING_FROM_ZERO_TERMINATED("Lorem"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("ab"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("c"),
            .expected = STRING_FROM_ZERO_TERMINATED("ab"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("Ipsum"),
            .expected = STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".2"),
            .expected = STRING_FROM_ZERO_TERMINATED("1.2"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                    .position = 2,
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".2"),
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED("abc"),
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                    .position = 0,
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".3"),
            .expected = STRING_FROM_ZERO_TERMINATED("1.2.3"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("1.2.3"),
                    .position = 2,
                },
            .search = &STRING_FROM_ZERO_TERMINATED(".3"),
            .expected = STRING_FROM_ZERO_TERMINATED("2.3"),
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      struct string *expected = &testCase->expected;
      struct string_cursor *cursor = &testCase->cursor;
      struct string *search = testCase->search;

      struct string got = StringCursorExtractThrough(cursor, search);
      if (!IsStringEqual(&got, expected)) {
        errorCode = STRING_CURSOR_TEST_ERROR_EXTRACT_THROUGH_EXPECTED;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  cursor: '"));
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("' at position: "));
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  search: '"));
        StringBuilderAppendString(sb, search);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected: '"));
        StringBuilderAppendPrintableString(sb, expected);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got: '"));
        StringBuilderAppendPrintableString(sb, &got);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("'"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  }

  // struct string StringCursorExtractNumber(struct string_cursor *cursor)
  {
    struct test_case {
      struct string_cursor cursor;
      struct string expected;
    } testCases[] = {
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("90876"),
                    .position = 0,
                },
            .expected = STRING_FROM_ZERO_TERMINATED("90876"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("5933 abcdef"),
                    .position = 0,
                },
            .expected = STRING_FROM_ZERO_TERMINATED("5933"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("-10203 fool"),
                    .position = 0,
                },
            .expected = STRING_FROM_ZERO_TERMINATED("-10203"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("-54.3023 fool"),
                    .position = 0,
                },
            .expected = STRING_FROM_ZERO_TERMINATED("-54.3023"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("54.-3023 fool"),
                    .position = 0,
                },
            .expected = STRING_FROM_ZERO_TERMINATED("54."),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("5933 abcdef"),
                    .position = 1,
                },
            .expected = STRING_FROM_ZERO_TERMINATED("933"),
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED("abcdef"),
                    .position = 0,
                },
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &STRING_FROM_ZERO_TERMINATED(""),
                    .position = 0,
                },
            .expected = {},
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      struct string *expected = &testCase->expected;
      struct string_cursor *cursor = &testCase->cursor;

      struct string got = StringCursorExtractNumber(cursor);
      if (!IsStringEqual(&got, expected)) {
        errorCode = expected->length ? STRING_CURSOR_TEST_ERROR_EXTRACT_NUMBER_EXPECTED_TRUE
                                     : STRING_CURSOR_TEST_ERROR_EXTRACT_NUMBER_EXPECTED_FALSE;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  cursor: '"));
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("' at position: "));
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n  expected: "));
        StringBuilderAppendPrintableString(sb, expected);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n       got: "));
        StringBuilderAppendPrintableString(sb, &got);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  }

  return (int)errorCode;
}
