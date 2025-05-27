#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "memory.h"
#include "print.h"
#include "string_builder.h"
#include "text.h"
#include "type.h"

#include "http_parser.c"
#include "json_parser.c"
#include "platform.h"

struct invidious_context {
  // Socket

  int sockfd;

  struct {
    WOLFSSL_CTX *ctx;
    WOLFSSL *ssl;
  } wolfSSL;
};

#include "string_builder_extended.c"

int
main(void)
{
  enum {
    KILOBYTES = (1 << 10),
    MEGABYTES = (1 << 20),
  };

  struct invidious_context context = {};
  u8 stackBuf[2 * MEGABYTES];
  memory_arena stackMemory = {
      .block = stackBuf,
      .total = ARRAY_COUNT(stackBuf),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

  // hostname
  struct string hostname = StringFromLiteral("i.iii.st");
  // port
  struct string port = StringFromLiteral("443");
  // video id
  struct string videoId = StringFromLiteral("d_oVysaqG_0");

  // Socket
  context.sockfd = -1;

  {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;

    if (getaddrinfo((char *)hostname.value, (char *)port.value, &hints, &res)) {
      return 1;
    }

    int sockfd;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
      sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sockfd == -1)
        continue;

      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
        break;

      close(sockfd);
    }

    freeaddrinfo(res);

    if (sockfd == -1)
      return 1;

    context.sockfd = sockfd;
  }

  // wolfSSL Setup
  int wolfsslError;
  // enum { WOLFSSL_ALLOCATED_MEMORY = 512 * KILOBYTES };
  // wolfssl buffer init (MemoryArenaPush(&stackMemory, WOLFSSL_ALLOCATED_MEMORY), WOLFSSL_ALLOCATED_MEMORY);

  wolfSSL_Init();
  context.wolfSSL.ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
  if (!context.wolfSSL.ctx) {
    struct string message = StringFromLiteral("wolfSSL_CTX_new\n");
    PrintString(&message);
    return 1;
  }

  // - Load system CA
  {
    memory_temp tempMemory = MemoryTempBegin(&stackMemory);

    struct string systemCAPath = StringFromLiteral("/etc/ssl/certs/ca-certificates.crt");
    u64 systemCALength = PlatformFileSize(&systemCAPath);
    if (systemCALength == 0) {
      StringBuilderAppendStringLiteral(sb, "CA certificates not found or invalid on system.");
      StringBuilderAppendStringLiteral(sb, "\n  Path: ");
      StringBuilderAppendString(sb, &systemCAPath);
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }

    struct string buffer = {
        .length = systemCALength,
        .value = MemoryArenaPushAligned(tempMemory.arena, systemCALength, 32),
    };

    struct string systemCA = PlatformReadFile(&buffer, &systemCAPath);
    if (IsStringNullOrEmpty(&systemCA)) {
      StringBuilderAppendStringLiteral(sb, "Reading CA certificates from system failed.");
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }

    systemCA = StringStripWhitespace(&systemCA);
    // mbedtls_x509_crt_parse expectes zero-terminated
    // systemCA.value[systemCA.length] = 0;
    // systemCA.length += 1;

    wolfsslError =
        wolfSSL_CTX_load_verify_buffer(context.wolfSSL.ctx, systemCA.value, (s64)systemCA.length, WOLFSSL_FILETYPE_PEM);
    if (wolfsslError != WOLFSSL_SUCCESS) {
      StringBuilderAppendStringLiteral(sb, "Failed to parse CA cert.");
      StringBuilderAppendStringLiteral(sb, "\n");
      // StringBuilderAppendWolfsslError(sb, wolfsslError);
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }

    MemoryTempEnd(&tempMemory);
  }

  // - WOLFSSL Object
#if 0 && IS_DEBUG_BUILD
  wolfSSL_CTX_set_verify(context.wolfSSL.ctx, SSL_VERIFY_NONE, 0);
#endif
  context.wolfSSL.ssl = wolfSSL_new(context.wolfSSL.ctx);
  if (!context.wolfSSL.ssl) {
    struct string message = StringFromLiteral("wolfSSL_new()\n");
    PrintString(&message);
    return 1;
  }

  wolfSSL_UseSNI(context.wolfSSL.ssl, WOLFSSL_SNI_HOST_NAME, hostname.value, (u16)hostname.length);

  // - Attach unix socket to WOLFSSL object
  wolfSSL_set_fd(context.wolfSSL.ssl, context.sockfd);
  wolfsslError = wolfSSL_connect(context.wolfSSL.ssl);
  if (wolfsslError != WOLFSSL_SUCCESS) {
    StringBuilderAppendStringLiteral(sb, "TLS handshake failed.\n");
    StringBuilderAppendStringLiteral(sb, "  wolfSSL error: ");
    StringBuilderAppendWolfsslError(sb, wolfsslError);
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  // Send an HTTP Request
  {
    // memory_temp tempMemory = MemoryTempBegin(&stackMemory);
    // struct http_request_builder *hrb = MakeHttpRequestBuilder(tempMemory.arena);
    // HttpRequestMethod(hrb, HTTP_METHOD_GET);
    // HttpRequestVersion(hrb, HTTP_VERSION_1_1);
    // HttpRequestPath(hrb, "/api/v1/videos/");
    // HttpRequestPathAppend(hrb, &videoId);
    // HttpRequestHost(hrb, &hostname);
    // HttpRequestAccept(hrb, &StringFromLiteral("application/json"));
    // struct string httpRequest = HttpRequestBuild(hrb);

    StringBuilderAppendStringLiteral(sb, "GET ");
    StringBuilderAppendStringLiteral(sb, "/api/v1/videos/");
    StringBuilderAppendString(sb, &videoId);
    StringBuilderAppendStringLiteral(sb, " HTTP/1.1");
    StringBuilderAppendStringLiteral(sb, "\r\n");
    StringBuilderAppendStringLiteral(sb, "host: ");
    StringBuilderAppendString(sb, &hostname);
    StringBuilderAppendStringLiteral(sb, "\r\n");
    StringBuilderAppendStringLiteral(sb, "\r\n");
    struct string request = StringBuilderFlush(sb);

    u64 totalBytesWritten = 0;
    while (1) {
      // https://www.wolfssl.com/documentation/manuals/wolfssl/group__IO.html#function-wolfssl_write
      int ret = wolfSSL_write(context.wolfSSL.ssl, request.value + totalBytesWritten,
                              (int)(request.length - totalBytesWritten));
      if (ret < 0) {
        wolfsslError = wolfSSL_get_error(context.wolfSSL.ssl, ret);
        if (wolfsslError == WOLFSSL_ERROR_WANT_READ || wolfsslError == WOLFSSL_ERROR_WANT_WRITE)
          continue;

        StringBuilderAppendStringLiteral(sb, "wolfSSL write error: ");
        // StringBuilderAppendMbedtlsError(sb, mbedtlsError);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string message = StringBuilderFlush(sb);
        PrintString(&message);
        return 1;
      }

      u64 bytesWritten = (u64)ret;
      totalBytesWritten += bytesWritten;
      if (totalBytesWritten == request.length)
        break;
    }

    // MemoryTempEnd(&tempMemory);
  }

  // Recieve a HTTP Response and parse it
  u64 responseBufferMax = 256 * KILOBYTES;
  u8 *responseBuffer = MemoryArenaPush(&stackMemory, sizeof(*responseBuffer) * responseBufferMax);
  struct string response;
  struct http_parser *httpParser = MakeHttpParser(&stackMemory, 1024);
  {
    u64 totalBytesRead = 0;
    while (1) {
      // https://www.wolfssl.com/documentation/manuals/wolfssl/group__IO.html#function-wolfssl_read
      int ret =
          wolfSSL_read(context.wolfSSL.ssl, responseBuffer + totalBytesRead, (int)(responseBufferMax - totalBytesRead));
      if (ret < 0) {
        wolfsslError = wolfSSL_get_error(context.wolfSSL.ssl, ret);
        if (wolfsslError == WOLFSSL_ERROR_WANT_READ || wolfsslError == WOLFSSL_ERROR_WANT_WRITE)
          continue;

        StringBuilderAppendStringLiteral(sb, "TLS read failed.\n");
        StringBuilderAppendStringLiteral(sb, "  wolfSSL error: ");
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string message = StringBuilderFlush(sb);
        PrintString(&message);
        return 1;
      }

      u64 bytesRead = (u64)ret;
      if (bytesRead == 0)
        break; // EOF

      struct string packet = StringFromBuffer(responseBuffer + totalBytesRead, bytesRead);
      b8 ok = HttpParse(httpParser, &packet);
      if (ok)
        break;
      if (httpParser->error != HTTP_PARSER_ERROR_PARTIAL) {
        StringBuilderAppendStringLiteral(sb, "Http parser failed.");
        StringBuilderAppendStringLiteral(sb, "\n     error: ");
        StringBuilderAppendHttpParserError(sb, httpParser->error);
        StringBuilderAppendStringLiteral(sb, "\n  position: ");
        StringBuilderAppendU64(sb, httpParser->position);
        StringBuilderAppendStringLiteral(sb, "\n");
        struct string message = StringBuilderFlush(sb);
        PrintString(&message);
        return 1;
      }

      totalBytesRead += bytesRead;
    }

    response = StringFromBuffer(responseBuffer, totalBytesRead);
  }

  if (response.length == responseBufferMax) {
    StringBuilderAppendStringLiteral(sb, "Server responded with too large file than we expected");
    StringBuilderAppendU64(sb, response.length);
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  /* Q: Can we feed json parser http chunked encoded json?
   * A: Yes. But data must not be cut at any token into split.
   *    Below is ok:
   *      First data:  { "a": 1,
   *      Second data: 2, 3 }
   *    But below is not:
   *      First data:  { "versi
   *      Second data: on": "1.0" }
   */
  // extract data from http response
  struct string json;
  if (httpParser->state & HTTP_PARSER_STATE_HAS_CHUNKED_ENCODED_BODY) {
    u64 jsonBufferLength = 0;
    for (u32 httpTokenIndex = 3; httpTokenIndex < httpParser->tokenCount; httpTokenIndex++) {
      struct http_token *httpToken = httpParser->tokens + httpTokenIndex;
      if (httpToken->type != HTTP_TOKEN_CHUNK_DATA)
        continue;

      struct string data = HttpTokenExtractString(httpToken, &response);
      jsonBufferLength += data.length;
    }

    string_builder *jsonBuilder = MakeStringBuilder(&stackMemory, jsonBufferLength, 0);
    for (u32 httpTokenIndex = 3; httpTokenIndex < httpParser->tokenCount; httpTokenIndex++) {
      struct http_token *httpToken = httpParser->tokens + httpTokenIndex;
      if (httpToken->type != HTTP_TOKEN_CHUNK_DATA)
        continue;
      struct string data = HttpTokenExtractString(httpToken, &response);
      StringBuilderAppendString(jsonBuilder, &data);
    }
    json = StringBuilderFlush(jsonBuilder);
  } else if (httpParser->state & HTTP_PARSER_STATE_HAS_CONTENT_LENGTH_BODY) {
    struct http_token *lastHttpToken = httpParser->tokens + httpParser->tokenCount - 1;
    json = HttpTokenExtractString(lastHttpToken, &response);
  } else {
    PrintString(&StringFromLiteral("No body found\n"));
    return 1;
  }

  // parse data as json
  struct json_parser *jsonParser = MakeJsonParser(&stackMemory, 4096);
  if (!JsonParse(jsonParser, &json)) {
    StringBuilderAppendStringLiteral(sb, "Json parser failed.");
    StringBuilderAppendStringLiteral(sb, "\n  error: ");
    StringBuilderAppendU64(sb, (u64)jsonParser->error);
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  {
    struct json_token *firstJsonToken = jsonParser->tokens + 0;
    if (firstJsonToken->type != JSON_TOKEN_OBJECT) {
      StringBuilderAppendStringLiteral(sb, "Got unexpected json from server");
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }

    // { "error": "message" }
    if (jsonParser->tokenCount == 3) {
      struct string field = JsonTokenExtractString(jsonParser->tokens + 1, &json);
      struct string message;
      if (IsStringEqual(&field, &StringFromLiteral("error")))
        message = StringFromLiteral("Got invalid json from server\n");
      message = JsonTokenExtractString(jsonParser->tokens + 2, &json);
      PrintString(&message);
      return 1;
    }
  }

  for (u32 jsonTokenIndex = 1; jsonTokenIndex < jsonParser->tokenCount; jsonTokenIndex++) {
    struct json_token *jsonToken = jsonParser->tokens + jsonTokenIndex;
    if (jsonToken->type != JSON_TOKEN_STRING)
      continue;

    struct json_token *keyToken = jsonToken;
    struct string key = JsonTokenExtractString(keyToken, &json);
    if (IsStringEqual(&key, &StringFromLiteral("title"))) {
      jsonTokenIndex++;
      struct json_token *titleToken = jsonParser->tokens + jsonTokenIndex;
      if (titleToken->type != JSON_TOKEN_STRING) {
        // expected title to be string
        return 1;
      }
      struct string title = JsonTokenExtractString(titleToken, &json);

      StringBuilderAppendStringLiteral(sb, "Title: ");
      StringBuilderAppendString(sb, &title);
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      continue;
    }

    if (IsStringEqual(&key, &StringFromLiteral("type"))) {
      jsonTokenIndex++;
      struct json_token *titleToken = jsonParser->tokens + jsonTokenIndex;
      if (titleToken->type != JSON_TOKEN_STRING) {
        // expected title to be string
        return 1;
      }
      struct string type = JsonTokenExtractString(titleToken, &json);

      StringBuilderAppendStringLiteral(sb, "Type: ");
      StringBuilderAppendString(sb, &type);
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      continue;
    }

    jsonTokenIndex++;
    struct json_token *valueToken = jsonParser->tokens + jsonTokenIndex;

    // go to next key value pair
    while (jsonTokenIndex < jsonParser->tokenCount - 1) {
      struct json_token *token = jsonParser->tokens + jsonTokenIndex + 1;
      if (valueToken->end < token->start)
        break;
      jsonTokenIndex++;
    }
  }

#if IS_BUILD_DEBUG
  {
    StringBuilderAppendStringLiteral(sb, "Memory total:  ");
    StringBuilderAppendHumanReadableBytes(sb, stackMemory.total);
    StringBuilderAppendStringLiteral(sb, "\n");
    StringBuilderAppendStringLiteral(sb, "Memory used:   ");
    StringBuilderAppendHumanReadableBytes(sb, stackMemory.used);
    StringBuilderAppendStringLiteral(sb, "\n");
    StringBuilderAppendStringLiteral(sb, "Memory wasted: ");
    StringBuilderAppendHumanReadableBytes(sb, stackMemory.total - stackMemory.used);
    StringBuilderAppendStringLiteral(sb, "\n");

    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
  }
#endif

  return 0;
}
