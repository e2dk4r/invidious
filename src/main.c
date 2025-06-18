#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// for example usage see mbedtls/programs/ssl/ssl_client2.c
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/memory_buffer_alloc.h>
#include <mbedtls/net_sockets.h>

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

  // Mbed TLS

  mbedtls_ssl_context ssl;
  mbedtls_ssl_config sslConfig;
  mbedtls_x509_crt cacert;
  mbedtls_ctr_drbg_context ctrDrbg;
  mbedtls_entropy_context entropy;
};

#include "string_builder_extended.c"

internalfn void
MbedtlsDebugCallback(void *data, int level, const char *file, int line, const char *str)
{
  string_builder *sb = data;

  // see mbedtls-3.6.3/include/mbedtls/debug.h:144:33
  string levelTexts[] = {
      StringFromLiteral("[NDBG]"), // 0 No debug
      StringFromLiteral("[ERR ]"), // 1 Error
      StringFromLiteral("[CHG ]"), // 2 State change
      StringFromLiteral("[INFO]"), // 3 Informational
      StringFromLiteral("[VERB]"), // 4 Verbose
  };
  debug_assert(level >= 0 && level <= ARRAY_COUNT(levelTexts));

  StringBuilderAppendString(sb, &levelTexts[level]);
  StringBuilderAppendStringLiteral(sb, " ");
  string relativeFile = StringFromZeroTerminated((u8 *)file, 1024);
  {
    string_cursor cursor = StringCursorFromString(&relativeFile);
    StringCursorConsumeUntil(&cursor, &StringFromLiteral("mbedtls-3.6.3/"));
    relativeFile = StringCursorExtractRemaining(&cursor);
  }
  StringBuilderAppendString(sb, &relativeFile);
  StringBuilderAppendStringLiteral(sb, ":");
  StringBuilderAppendS32(sb, line);
  StringBuilderAppendStringLiteral(sb, ": ");
  StringBuilderAppendZeroTerminated(sb, str, 1024);

  string message = StringBuilderFlush(sb);
  PrintString(&message);
}

int
main(int argc, char *argv[])
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

  string_builder *sb = MakeStringBuilder(&stackMemory, 2048, 32);

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

  // Mbed TLS Setup
  int mbedtlsError;
  enum { MBEDTLS_ALLOCATED_MEMORY = 544 * KILOBYTES };
  mbedtls_memory_buffer_alloc_init(MemoryArenaPush(&stackMemory, MBEDTLS_ALLOCATED_MEMORY), MBEDTLS_ALLOCATED_MEMORY);

  mbedtls_ssl_init(&context.ssl);
  mbedtls_ssl_config_init(&context.sslConfig);
  mbedtls_x509_crt_init(&context.cacert);
  mbedtls_ctr_drbg_init(&context.ctrDrbg);
  mbedtls_entropy_init(&context.entropy);

  // Load system CA
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

    systemCALength += 1; // zero-termination
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
    systemCA.value[systemCA.length] = 0;
    systemCA.length += 1;

    mbedtlsError = mbedtls_x509_crt_parse(&context.cacert, systemCA.value, systemCA.length);
    if (mbedtlsError < 0) {
      StringBuilderAppendStringLiteral(sb, "Failed to parse CA cert.");
      StringBuilderAppendStringLiteral(sb, "\n");
      StringBuilderAppendMbedtlsError(sb, mbedtlsError);
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }

    MemoryTempEnd(&tempMemory);
  }

  // Seed RNG
  mbedtlsError = mbedtls_ctr_drbg_seed(&context.ctrDrbg, mbedtls_entropy_func, &context.entropy, 0, 0);
  if (mbedtlsError != 0) {
    StringBuilderAppendStringLiteral(sb, "Entropy seed failed.");
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  if (mbedtls_ssl_config_defaults(&context.sslConfig, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    StringBuilderAppendStringLiteral(sb, "SSL configuration failed.");
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

#if 0 && IS_BUILD_DEBUG
  // NOTE: Enable for troubleshooting handshake errors
  mbedtls_ssl_conf_dbg(&context.sslConfig, MbedtlsDebugCallback, sb);
  mbedtls_debug_set_threshold(4);
#endif

  mbedtls_ssl_conf_min_version(&context.sslConfig, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_4);
  mbedtls_ssl_conf_max_version(&context.sslConfig, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_4);

#if 0 && IS_BUILD_DEBUG
  mbedtls_ssl_conf_authmode(&context.sslConfig, MBEDTLS_SSL_VERIFY_OPTIONAL);
#else
  mbedtls_ssl_conf_authmode(&context.sslConfig, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_ca_chain(&context.sslConfig, &context.cacert, NULL);
#endif
  mbedtls_ssl_conf_rng(&context.sslConfig, mbedtls_ctr_drbg_random, &context.ctrDrbg);

  mbedtlsError = mbedtls_ssl_setup(&context.ssl, &context.sslConfig);
  if (mbedtlsError) {
    u32 breakHere = 1;
    StringBuilderAppendStringLiteral(sb, "SSL setup failed.");
    StringBuilderAppendStringLiteral(sb, "  Mbed TLS error: ");
    StringBuilderAppendMbedtlsError(sb, mbedtlsError);
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  // Attach Mbed TLS to unix socket
  mbedtls_ssl_set_bio(&context.ssl, &context.sockfd, mbedtls_net_send, mbedtls_net_recv, 0);
  mbedtls_ssl_set_hostname(&context.ssl, (char *)hostname.value);

  // Handshake
  mbedtlsError = mbedtls_ssl_handshake(&context.ssl);
  if (mbedtlsError) {
    u32 breakHere = 1;
    StringBuilderAppendStringLiteral(sb, "TLS handshake failed.\n");
    StringBuilderAppendStringLiteral(sb, "  Mbed TLS error: ");
    StringBuilderAppendMbedtlsError(sb, mbedtlsError);
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
      int ret = mbedtls_ssl_write(&context.ssl, request.value + totalBytesWritten, request.length - totalBytesWritten);
      if (ret < 0) {
        mbedtlsError = ret;
        if (mbedtlsError == MBEDTLS_ERR_SSL_WANT_READ || mbedtlsError == MBEDTLS_ERR_SSL_WANT_WRITE ||
            mbedtlsError == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
          continue;
        }

        StringBuilderAppendStringLiteral(sb, "Mbed TLS write error: ");
        StringBuilderAppendMbedtlsError(sb, mbedtlsError);
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
      int ret = mbedtls_ssl_read(&context.ssl, responseBuffer + totalBytesRead, responseBufferMax - totalBytesRead);
      if (ret < 0) {
        mbedtlsError = ret;
        if (mbedtlsError == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
          break;
        if (mbedtlsError == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS)
          continue;
        if (mbedtlsError == MBEDTLS_ERR_SSL_WANT_READ || mbedtlsError == MBEDTLS_ERR_SSL_WANT_WRITE)
          continue;

        StringBuilderAppendStringLiteral(sb, "TLS read failed.\n");
        StringBuilderAppendStringLiteral(sb, "  Mbed TLS error: ");
        StringBuilderAppendMbedtlsError(sb, mbedtlsError);
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

  {
    struct string type = StringNull();
    struct string title = StringNull();

    struct json_cursor cursor = JsonCursor(&json, jsonParser);
    if (!JsonCursorIsObject(&cursor))
      return 1; // error invalid json
    if (!JsonCursorNext(&cursor))
      return 1; // error invalid json

    while (!JsonCursorIsAtEnd(&cursor) && (IsStringNullOrEmpty(&type) || IsStringNullOrEmpty(&title))) {
      struct string key = JsonCursorExtractString(&cursor);

      if (IsStringEqual(&key, &StringFromLiteral("type"))) {
        if (!JsonCursorNext(&cursor))
          return 1; // error invalid json
        if (!JsonCursorIsString(&cursor))
          return 1; // error invalid json

        type = JsonCursorExtractString(&cursor);
        JsonCursorNext(&cursor);
        continue;
      }

      else if (IsStringEqual(&key, &StringFromLiteral("title"))) {
        if (!JsonCursorNext(&cursor))
          return 1; // error invalid json
        if (!JsonCursorIsString(&cursor))
          return 1; // error invalid json

        title = JsonCursorExtractString(&cursor);
        JsonCursorNext(&cursor);
        continue;
      }

      if (!JsonCursorNextKey(&cursor))
        return 1; // error invalid json
    }

    if (IsStringNullOrEmpty(&type) || IsStringNullOrEmpty(&title))
      return 1; // error invalid json

    StringBuilderAppendStringLiteral(sb, "Type: ");
    StringBuilderAppendString(sb, &type);
    StringBuilderAppendStringLiteral(sb, "\n");
    StringBuilderAppendStringLiteral(sb, "Title: ");
    StringBuilderAppendString(sb, &title);
    StringBuilderAppendStringLiteral(sb, "\n");

    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
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
