#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// for example usage see mbedtls/programs/ssl/ssl_client2.c
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>

#include "memory.h"
#include "print.h"
#include "string_builder.h"
#include "text.h"
#include "type.h"

#include "http_parser.c"
#include "json_parser.c"

comptime u64 KILOBYTES = 1 << 10;
comptime u64 MEGABYTES = 1 << 20;

struct invidious_context {
  // Socket

  int sockfd;

  // Mbed TLS

  mbedtls_ssl_context ssl;
  mbedtls_ssl_config sslConfig;
  mbedtls_ctr_drbg_context ctrDrbg;
  mbedtls_entropy_context entropy;
};

static inline void
StringBuilderAppendMbedtlsError(string_builder *sb, int errnum)
{
  char buf[1024];
  mbedtls_strerror(errnum, buf, ARRAY_COUNT(buf));
  StringBuilderAppendZeroTerminated(sb, buf, 1024);
}

int
main(void)
{
  struct invidious_context context = {};
  u8 stackBuf[1 * MEGABYTES];
  memory_arena stackMemory = {
      .block = stackBuf,
      .total = ARRAY_COUNT(stackBuf),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

  // hostname
  struct string hostname = StringFromLiteral("i.iii.st");
  // port
  struct string port = StringFromLiteral("443");

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
  mbedtls_ssl_init(&context.ssl);
  mbedtls_ssl_config_init(&context.sslConfig);
  mbedtls_ctr_drbg_init(&context.ctrDrbg);
  mbedtls_entropy_init(&context.entropy);

  if (mbedtls_ctr_drbg_seed(&context.ctrDrbg, mbedtls_entropy_func, &context.entropy, 0, 0)) {
    return 1;
  }

  if (mbedtls_ssl_config_defaults(&context.sslConfig, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    return 1;
  }

  mbedtls_ssl_conf_authmode(&context.sslConfig, MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_rng(&context.sslConfig, mbedtls_ctr_drbg_random, &context.ctrDrbg);
  mbedtls_ssl_setup(&context.ssl, &context.sslConfig);

  // Attach Mbed TLS to unix socket
  mbedtls_ssl_set_bio(&context.ssl, &context.sockfd, mbedtls_net_send, mbedtls_net_recv, 0);
  int mbedtlsError;
  mbedtlsError = mbedtls_ssl_handshake(&context.ssl);
  if (mbedtlsError) {
    StringBuilderAppendStringLiteral(sb, "TLS handshake failed.\n");
    StringBuilderAppendStringLiteral(sb, "  Mbed TLS error: ");
    StringBuilderAppendMbedtlsError(sb, mbedtlsError);
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  // Send an HTTP Request
  StringBuilderAppendStringLiteral(sb, "GET ");
  StringBuilderAppendStringLiteral(sb, "/api/v1/videos/");
  struct string videoId = StringFromLiteral("d_oVysaqG_0");
  StringBuilderAppendString(sb, &videoId);
  StringBuilderAppendStringLiteral(sb, " HTTP/1.1");
  StringBuilderAppendStringLiteral(sb, "\r\n");
  StringBuilderAppendStringLiteral(sb, "host: ");
  StringBuilderAppendString(sb, &hostname);
  StringBuilderAppendStringLiteral(sb, "\r\n");
  StringBuilderAppendStringLiteral(sb, "\r\n");
  struct string request = StringBuilderFlush(sb);

  {
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
  }

  // Recieve a HTTP Response and parse it
  u64 responseBufferMax = 256 * KILOBYTES;
  u8 *responseBuffer = MemoryArenaPush(&stackMemory, sizeof(*responseBuffer) * responseBufferMax);
  struct string response;
  struct http_parser *httpParser = MakeHttpParser(&stackMemory, 32);
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
        StringBuilderAppendU64(sb, (u64)httpParser->error);
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

  return 0;
}
