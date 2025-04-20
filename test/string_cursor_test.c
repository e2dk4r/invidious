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
  XX(STRING_CURSOR_TEST_ERROR_EXTRACT_NUMBER_EXPECTED_FALSE, "Number must NOT be extracted from cursor position")      \
  XX(STRING_CURSOR_TEST_ERROR_EXTRACT_CONSUMED, "Consumed string not matching expected")

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
#define XX(tag, msg) {.code = tag, .message = StringFromLiteral(msg)},
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
  enum string_cursor_test_error errorCode = STRING_CURSOR_TEST_ERROR_NONE;

  // setup
  u32 KILOBYTES = 1 << 10;
  u8 stackBuffer[8 * KILOBYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

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
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral(" Lorem Ipsum"),
                    .position = 1,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral(" Lorem Ipsum "),
                    .position = 1,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 0,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("abc"),
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
        StringBuilderAppendStringLiteral(sb, "\n  cursor: '");
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendStringLiteral(sb, "' at position: ");
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendStringLiteral(sb, "\n  search: '");
        StringBuilderAppendString(sb, search);
        StringBuilderAppendStringLiteral(sb, "'");
        StringBuilderAppendStringLiteral(sb, "\n  expected: ");
        StringBuilderAppendBool(sb, expected);
        StringBuilderAppendStringLiteral(sb, "\n       got: ");
        StringBuilderAppendBool(sb, got);
        StringBuilderAppendStringLiteral(sb, "\n");
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
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral(" Lorem Ipsum"),
                    .position = 1,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 1,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral(" Lorem Ipsum "),
                    .position = 1,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 0,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &StringFromLiteral("Lorem Ipsum"),
            .expected = 0,
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("abc"),
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
        StringBuilderAppendStringLiteral(sb, "\n  cursor: '");
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendStringLiteral(sb, "' at position: ");
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendStringLiteral(sb, "\n  search: '");
        StringBuilderAppendString(sb, search);
        StringBuilderAppendStringLiteral(sb, "'");
        StringBuilderAppendStringLiteral(sb, "\n  expected: ");
        StringBuilderAppendBool(sb, expected);
        StringBuilderAppendStringLiteral(sb, "\n       got: ");
        StringBuilderAppendBool(sb, got);
        StringBuilderAppendStringLiteral(sb, "\n");
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
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("Lorem"),
            .expected =
                {
                    .value = 1,
                    .remaining = &StringFromLiteral(" Ipsum"),
                },
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &StringFromLiteral("Ipsum"),
            .expected =
                {
                    .value = 1,
                    .remaining = &StringFromLiteral(""),
                },
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &StringFromLiteral("Lorem"),
            .expected =
                {
                    .value = 0,
                    .position = StringFromLiteral("Lorem Ipsum").length,
                },
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 1,
                },
            .search = &StringFromLiteral("abc"),
            .expected =
                {
                    .value = 0,
                    .position = StringFromLiteral("Lorem Ipsum").length,
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
        StringBuilderAppendStringLiteral(sb, "\n  cursor: '");
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendStringLiteral(sb, "' at position: ");
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendStringLiteral(sb, "\n  search: '");
        StringBuilderAppendString(sb, search);
        StringBuilderAppendStringLiteral(sb, "'");
        if (got != expectedToFindString) {
          StringBuilderAppendStringLiteral(sb, "\n  expected to find search: ");
          StringBuilderAppendBool(sb, expectedToFindString);
          StringBuilderAppendStringLiteral(sb, "\n       got: ");
          StringBuilderAppendBool(sb, got);
        } else if (!expectedToFindString && cursor->position != expectedPosition) {
          StringBuilderAppendStringLiteral(sb, "\n  expected cursor position: ");
          StringBuilderAppendU64(sb, expectedPosition);
          StringBuilderAppendStringLiteral(sb, "\n       got: ");
          StringBuilderAppendU64(sb, cursor->position);
        } else {
          StringBuilderAppendStringLiteral(sb, "\n   expected: ");
          StringBuilderAppendString(sb, expectedRemaining);
          StringBuilderAppendStringLiteral(sb, "\n        got: ");
          struct string remaining = StringCursorExtractSubstring(cursor, cursor->source->length - cursor->position);
          StringBuilderAppendString(sb, &remaining);
        }
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  } else {
    PrintString(&StringFromLiteral(
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
                    .source = &StringFromLiteral("Lorem Ipsum"),
                },
            .search = &StringFromLiteral("Lorem"),
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                },
            .search = &StringFromLiteral("Ipsum"),
            .expected = StringFromLiteral("Lorem "),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                },
            .search = &StringFromLiteral(".2"),
            .expected = StringFromLiteral("1"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                    .position = 2,
                },
            .search = &StringFromLiteral(".3"),
            .expected = StringFromLiteral("2"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                },
            .search = &StringFromLiteral(".3"),
            .expected = StringFromLiteral("1.2"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                    .position = 2,
                },
            .search = &StringFromLiteral(".3"),
            .expected = StringFromLiteral("2"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("abcdefgh"),
                },
            .search = &StringFromLiteral("012345"),
            .expected = StringFromLiteral("abcdefgh"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("abcdefgh"),
                    .position = 2,
                },
            .search = &StringFromLiteral("012345"),
            .expected = StringFromLiteral("cdefgh"),
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
        StringBuilderAppendStringLiteral(sb, "\n  cursor: '");
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendStringLiteral(sb, "' at position: ");
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendStringLiteral(sb, "\n  search: '");
        StringBuilderAppendString(sb, search);
        StringBuilderAppendStringLiteral(sb, "'");
        StringBuilderAppendStringLiteral(sb, "\n  expected: ");
        StringBuilderAppendPrintableString(sb, expected);
        StringBuilderAppendStringLiteral(sb, "\n       got: ");
        StringBuilderAppendPrintableString(sb, &got);
        StringBuilderAppendStringLiteral(sb, "\n");
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
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("Lorem"),
            .expected = StringFromLiteral("Lorem"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("ab"),
                    .position = 0,
                },
            .search = &StringFromLiteral("c"),
            .expected = StringFromLiteral("ab"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("Ipsum"),
            .expected = StringFromLiteral("Lorem Ipsum"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                    .position = 0,
                },
            .search = &StringFromLiteral(".2"),
            .expected = StringFromLiteral("1.2"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                    .position = 2,
                },
            .search = &StringFromLiteral(".2"),
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .search = &StringFromLiteral("abc"),
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                    .position = 0,
                },
            .search = &StringFromLiteral(".3"),
            .expected = StringFromLiteral("1.2.3"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("1.2.3"),
                    .position = 2,
                },
            .search = &StringFromLiteral(".3"),
            .expected = StringFromLiteral("2.3"),
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
        StringBuilderAppendStringLiteral(sb, "\n  cursor: '");
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendStringLiteral(sb, "' at position: ");
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendStringLiteral(sb, "\n  search: '");
        StringBuilderAppendString(sb, search);
        StringBuilderAppendStringLiteral(sb, "'");
        StringBuilderAppendStringLiteral(sb, "\n  expected: '");
        StringBuilderAppendPrintableString(sb, expected);
        StringBuilderAppendStringLiteral(sb, "'");
        StringBuilderAppendStringLiteral(sb, "\n       got: '");
        StringBuilderAppendPrintableString(sb, &got);
        StringBuilderAppendStringLiteral(sb, "'");
        StringBuilderAppendStringLiteral(sb, "\n");
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
                    .source = &StringFromLiteral("90876"),
                    .position = 0,
                },
            .expected = StringFromLiteral("90876"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("5933 abcdef"),
                    .position = 0,
                },
            .expected = StringFromLiteral("5933"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("-10203 fool"),
                    .position = 0,
                },
            .expected = StringFromLiteral("-10203"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("-54.3023 fool"),
                    .position = 0,
                },
            .expected = StringFromLiteral("-54.3023"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("86774.60272.062713"),
                    .position = 0,
                },
            .expected = StringFromLiteral("86774.60272"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("54.-3023 fool"),
                    .position = 0,
                },
            .expected = StringFromLiteral("54."),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("5933 abcdef"),
                    .position = 1,
                },
            .expected = StringFromLiteral("933"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("abcdef"),
                    .position = 0,
                },
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral(""),
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
        StringBuilderAppendStringLiteral(sb, "\n  cursor: '");
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendStringLiteral(sb, "' at position: ");
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendStringLiteral(sb, "\n  expected: ");
        StringBuilderAppendPrintableString(sb, expected);
        StringBuilderAppendStringLiteral(sb, "\n       got: ");
        StringBuilderAppendPrintableString(sb, &got);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  }

  // struct string StringCursorExtractConsumed(struct string_cursor *cursor)
  {
    struct test_case {
      struct string_cursor cursor;
      struct string expected;
    } testCases[] = {
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 6,
                },
            .expected = StringFromLiteral("Lorem "),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 11,
                },
            .expected = StringFromLiteral("Lorem Ipsum"),
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral("Lorem Ipsum"),
                    .position = 0,
                },
            .expected = {},
        },
        {
            .cursor =
                {
                    .source = &StringFromLiteral(""),
                    .position = 0,
                },
            .expected = {},
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      struct test_case *testCase = testCases + testCaseIndex;

      struct string *expected = &testCase->expected;

      struct string_cursor *cursor = &testCase->cursor;
      struct string got = StringCursorExtractConsumed(cursor);
      if (!IsStringEqual(&got, expected)) {
        errorCode = STRING_CURSOR_TEST_ERROR_EXTRACT_CONSUMED;

        StringBuilderAppendString(sb, GetTextTestErrorMessage(errorCode));
        StringBuilderAppendStringLiteral(sb, "\n  cursor: '");
        StringBuilderAppendString(sb, cursor->source);
        StringBuilderAppendStringLiteral(sb, "' at position: ");
        StringBuilderAppendU64(sb, cursor->position);
        StringBuilderAppendStringLiteral(sb, "\n  expected: ");
        StringBuilderAppendString(sb, expected);
        StringBuilderAppendStringLiteral(sb, "\n       got: ");
        StringBuilderAppendString(sb, &got);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }
    }
  }

  return (int)errorCode;
}
