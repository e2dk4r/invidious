#include "http_request.c"
#include "print.h"
#include "string_builder.h"

#define TEST_ERROR_LIST(X)                                                                                             \
  X(REQUEST_BUILD_EXPECTED_VAILD, "Valid HTTP request must be built from info")                                        \
  X(REQUEST_BUILD_EXPECTED_INVALID, "HTTP request must fail to be built from info")

enum http_request_test_error {
  HTTP_REQUEST_TEST_ERROR_NONE = 0,
#define X(tag, message) HTTP_REQUEST_TEST_ERROR_##tag,
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
StringBuilderAppendTestError(struct string_builder *sb, enum http_request_test_error errorCode)
{
  struct string message = StringFromLiteral("Unknown error");
  struct error {
    enum http_request_test_error code;
    struct string message;
  } errors[] = {
#define XX(tag, msg) {.code = HTTP_REQUEST_TEST_ERROR_##tag, .message = StringFromLiteral(msg)},
      TEST_ERROR_LIST(XX)
#undef XX
  };

  for (u32 errorIndex = 0; errorIndex < ARRAY_COUNT(errors); errorIndex++) {
    struct error *error = errors + errorIndex;
    if (error->code != errorCode)
      continue;

    message = error->message;
    break;
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
  enum http_request_test_error errorCode = HTTP_REQUEST_TEST_ERROR_NONE;

  // setup
  enum {
    KILOBYTES = (1 << 10),
    MEGABYTES = (1 << 20),
  };
  u8 stackBuffer[1 * MEGABYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 128 * KILOBYTES, 32);

  // struct string HttpRequestBuild(struct http_request_info *info, struct string *buffer)
  {
    struct test_case {
      struct http_request_info requestInfo;
      struct string expected;
    } testCases[] = {
        {
            .requestInfo =
                {
                    .method = HTTP_METHOD_GET,
                    .version = HTTP_VERSION_11,
                    .host = StringFromLiteral("i.iii.st"),
                    .path = StringFromLiteral("/api/v1/videos/d_oVysaqG_0"),
                },
            .expected = StringFromLiteral(
                // request line
                "GET /api/v1/videos/d_oVysaqG_0 HTTP/1.1"
                "\r\n"
                // headers
                "host:i.iii.st"
                "\r\n"
                "\r\n"
                // content
                ),
        },
        {
            .requestInfo =
                {
                    .method = HTTP_METHOD_GET,
                    .version = HTTP_VERSION_11,
                    .url = StringFromLiteral("https://i.iii.st/api/v1/videos/d_oVysaqG_0"),
                },
            .expected = StringFromLiteral(
                // request line
                "GET /api/v1/videos/d_oVysaqG_0 HTTP/1.1"
                "\r\n"
                // headers
                "host:i.iii.st"
                "\r\n"
                "\r\n"
                // content
                ),
        },
        {
            .requestInfo =
                {
                    .method = HTTP_METHOD_GET,
                    .version = HTTP_VERSION_11,
                    .host = StringFromLiteral("127.0.0.1"),
                },
            .expected = StringFromLiteral(
                // request line
                "GET / HTTP/1.1"
                "\r\n"
                // headers
                "host:127.0.0.1"
                "\r\n"
                "\r\n"
                // content
                ),
        },
        {
            .requestInfo =
                {
                    .method = HTTP_METHOD_POST,
                    .version = HTTP_VERSION_11,
                    .path = StringFromLiteral("/test"),
                    .host = StringFromLiteral("example.com"),
                    .contentType = HTTP_CONTENT_TYPE_FORM_URLENCODED,
                    .content = &((struct http_form_urlencoded_list){
                        .itemCount = 2,
                        .items =
                            (struct http_form_urlencoded_item[]){
                                {.name = StringFromLiteral("fruit"), .value = StringFromLiteral("apple")},
                                {.name = StringFromLiteral("kind"), .value = StringFromLiteral("fuji")},
                            },
                    }),
                },
            .expected = StringFromLiteral(
                // request line
                "POST /test HTTP/1.1"
                "\r\n"
                // headers
                "host:example.com"
                "\r\n"
                "content-type:application/x-www-form-urlencoded"
                "\r\n"
                "content-length:21"
                "\r\n"
                "\r\n"
                // content
                "fruit=apple&kind=fuji"),
        },
        {
            .requestInfo =
                {
                    .method = HTTP_METHOD_POST,
                    .version = HTTP_VERSION_11,
                    .path = StringFromLiteral("/test"),
                    .host = StringFromLiteral("127.0.0.1"),
                    .contentType = HTTP_CONTENT_TYPE_JSON,
                    .content = &StringFromLiteral("{ \"car\": \"Toyota\", \"model\": \"Corolla\", \"year\": 2005 }"),
                },
            .expected = StringFromLiteral(
                // request line
                "POST /test HTTP/1.1"
                "\r\n"
                // headers
                "host:127.0.0.1"
                "\r\n"
                "content-type:application/json"
                "\r\n"
                "content-length:53"
                "\r\n"
                "\r\n"
                // content
                "{ \"car\": \"Toyota\", \"model\": \"Corolla\", \"year\": 2005 }"),
        },
        {
            .requestInfo = {},
            .expected = StringNull(),
        },
        {
            // invalid url
            .requestInfo =
                {
                    .url = StringFromLiteral("invalid url"),
                },
            .expected = StringNull(),
        },
    };

    for (u32 testCaseIndex = 0; testCaseIndex < ARRAY_COUNT(testCases); testCaseIndex++) {
      memory_temp tempMemory = MemoryTempBegin(&stackMemory);
      struct test_case *testCase = testCases + testCaseIndex;
      struct string *expected = &testCase->expected;

      struct http_request_info *requestInfo = &testCase->requestInfo;
      u32 breakHere = 1;
      struct string got = HttpRequestBuild(requestInfo, tempMemory.arena);

      if (!IsStringEqual(expected, &got)) {
        errorCode = !IsStringNull(expected) ? HTTP_REQUEST_TEST_ERROR_REQUEST_BUILD_EXPECTED_VAILD
                                            : HTTP_REQUEST_TEST_ERROR_REQUEST_BUILD_EXPECTED_INVALID;

        StringBuilderAppendTestError(sb, errorCode);
        StringBuilderAppendStringLiteral(sb, "\n  expected:\n");
        StringBuilderAppendHexDump(sb, expected);
        StringBuilderAppendStringLiteral(sb, "\n       got:\n");
        StringBuilderAppendHexDump(sb, &got);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string errorMessage = StringBuilderFlush(sb);
        PrintString(&errorMessage);
      }

      MemoryTempEnd(&tempMemory);
    }
  }

  return (int)errorCode;
}
