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

internalfn inline void
StringBuilderAppendHumanReadableBytes(string_builder *sb, u64 bytes)
{
  enum {
    TERABYTES = (1UL << 40),
    GIGABYTES = (1UL << 30),
    MEGABYTES = (1UL << 20),
    KILOBYTES = (1UL << 10),
  };

  struct order {
    u64 magnitude;
    struct string unit;
  } orders[] = {
      // order is important, bigger one first
      {.magnitude = TERABYTES, .unit = StringFromLiteral("TiB")},
      {.magnitude = GIGABYTES, .unit = StringFromLiteral("GiB")},
      {.magnitude = MEGABYTES, .unit = StringFromLiteral("MiB")},
      {.magnitude = KILOBYTES, .unit = StringFromLiteral("KiB")},
      {.magnitude = 1, .unit = StringFromLiteral("B")},
  };

  if (bytes == 0) {
    StringBuilderAppendStringLiteral(sb, "0");
    return;
  }

  for (u32 orderIndex = 0; orderIndex < ARRAY_COUNT(orders); orderIndex++) {
    struct order *order = orders + orderIndex;
    if (bytes >= order->magnitude) {
      u64 value = bytes / order->magnitude;
      bytes -= value * order->magnitude;

      StringBuilderAppendU64(sb, value);
      StringBuilderAppendString(sb, &order->unit);
      if (bytes != 0)
        StringBuilderAppendStringLiteral(sb, " ");
    }
  }
}

internalfn inline void
StringBuilderAppendHttpParserError(string_builder *sb, enum http_parser_error errorCode)
{
  struct error {
    enum http_parser_error code;
    struct string message;
  } errors[] = {
      {
          .code = HTTP_PARSER_ERROR_NONE,
          .message = StringFromLiteral("No error"),
      },
      {
          .code = HTTP_PARSER_ERROR_OUT_OF_MEMORY,
          .message = StringFromLiteral("Tokens are not enough"),
      },
      {
          .code = HTTP_PARSER_ERROR_HTTP_VERSION_INVALID,
          .message = StringFromLiteral("Http version is invalid"),
      },
      {
          .code = HTTP_PARSER_ERROR_HTTP_VERSION_EXPECTED_1_1,
          .message = StringFromLiteral("Expected server to be HTTP 1.1"),
      },
      {
          .code = HTTP_PARSER_ERROR_STATUS_CODE_INVALID,
          .message = StringFromLiteral("Http status code is invalid"),
      },
      {
          .code = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_3_DIGIT_INTEGER,
          .message = StringFromLiteral("Http status code must be 3 digits"),
      },
      {
          .code = HTTP_PARSER_ERROR_STATUS_CODE_EXPECTED_BETWEEN_100_AND_999,
          .message = StringFromLiteral("Http status code must be between 100 and 999"),
      },
      {
          .code = HTTP_PARSER_ERROR_REASON_PHRASE_INVALID,
          .message = StringFromLiteral("Http reason phrase is invalid"),
      },
      {
          .code = HTTP_PARSER_ERROR_HEADER_FIELD_NAME_REQUIRED,
          .message = StringFromLiteral("Http header field name required"),
      },
      {
          .code = HTTP_PARSER_ERROR_HEADER_FIELD_VALUE_REQUIRED,
          .message = StringFromLiteral("Http header field value required"),
      },
      {
          .code = HTTP_PARSER_ERROR_CONTENT_LENGTH_EXPECTED_POSITIVE_NUMBER,
          .message = StringFromLiteral("Http content length must be positive number"),
      },
      {
          .code = HTTP_PARSER_ERROR_UNSUPPORTED_TRANSFER_ENCODING,
          .message = StringFromLiteral("Transfer encoding is unsupported"),
      },
      {
          .code = HTTP_PARSER_ERROR_CHUNK_SIZE_IS_INVALID,
          .message = StringFromLiteral("Chunk size is invalid"),
      },
      {
          .code = HTTP_PARSER_ERROR_CHUNK_DATA_MALFORMED,
          .message = StringFromLiteral("Chunk data is malformed"),
      },
      {
          .code = HTTP_PARSER_ERROR_CONTENT_INVALID_LENGTH,
          .message = StringFromLiteral("Content is not matching with specified"),
      },
      {
          .code = HTTP_PARSER_ERROR_PARTIAL,
          .message = StringFromLiteral("Partial http"),
      },
  };

  StringBuilderAppendStringLiteral(sb, "HttpParser: ");
  struct string message = StringFromLiteral("Unknown error");
  for (u32 errorIndex = 0; errorIndex < ARRAY_COUNT(errors); errorIndex++) {
    struct error *error = errors + errorIndex;
    if (error->code == errorCode) {
      message = error->message;
      break;
    }
  }
  StringBuilderAppendString(sb, &message);
}

int
main(void)
{
  enum {
    KILOBYTES = (1 << 10),
    MEGABYTES = (1 << 20),
  };

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
