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
#include "string_builder.h"
#include "text.h"
#include "type.h"

#include "http_parser.c"

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
  memory_arena stackMemory = {};
  u8 stackBuf[1 * MEGABYTES];

  stackMemory.block = stackBuf;
  stackMemory.total = ARRAY_COUNT(stackBuf);

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

  // hostname
  struct string hostname = STRING_FROM_ZERO_TERMINATED("inv.stealthy.club");
  // port
  struct string port = STRING_FROM_ZERO_TERMINATED("443");

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
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("TLS handshake failed.\n"));
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("  Mbed TLS error: "));
    StringBuilderAppendMbedtlsError(sb, mbedtlsError);
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
    struct string message = StringBuilderFlush(sb);
    write(STDOUT_FILENO, message.value, message.length);
    return 1;
  }

  // Send an HTTP Request
  StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("GET "));
  StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("/api/v1/videos/"));
  struct string videoId = STRING_FROM_ZERO_TERMINATED("IroPQ150F6c");
  StringBuilderAppendString(sb, &videoId);
  StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED(" HTTP/1.1"));
  StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\r\n"));
  StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("host: "));
  StringBuilderAppendString(sb, &hostname);
  StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\r\n"));
  StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\r\n"));
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

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("Mbed TLS write error: "));
        StringBuilderAppendMbedtlsError(sb, mbedtlsError);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string message = StringBuilderFlush(sb);
        write(STDOUT_FILENO, message.value, message.length);
        return 1;
      }

      u64 bytesWritten = (u64)ret;
      totalBytesWritten += bytesWritten;
      if (totalBytesWritten == request.length)
        break;
    }
  }

  // Recieve a HTTP Response
  u64 responseBufferMax = 256 * KILOBYTES;
  u8 *responseBuffer = MemoryArenaPushUnaligned(&stackMemory, sizeof(*responseBuffer) * responseBufferMax);
  struct string response;
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

        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("TLS read failed.\n"));
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("  Mbed TLS error: "));
        StringBuilderAppendMbedtlsError(sb, mbedtlsError);
        StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
        struct string message = StringBuilderFlush(sb);
        write(STDOUT_FILENO, message.value, message.length);
        return 1;
      }

      u64 bytesRead = (u64)ret;
      if (bytesRead == 0)
        break; // EOF
      struct string packet = StringFromBuffer(responseBuffer + totalBytesRead, bytesRead);
      totalBytesRead += bytesRead;

      struct string finalChunkedPacket = STRING_FROM_ZERO_TERMINATED("0\r\n\r\n");
      if (IsStringEqual(&packet, &finalChunkedPacket))
        break;
    }

    response = StringFromBuffer(responseBuffer, totalBytesRead);
  }

  if (response.length == responseBufferMax) {
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("Warning: response is truncated to "));
    StringBuilderAppendU64(sb, response.length);
    StringBuilderAppendString(sb, &STRING_FROM_ZERO_TERMINATED("\n"));
    struct string message = StringBuilderFlush(sb);
    write(STDOUT_FILENO, message.value, message.length);
  }

  // Parse HTTP Response
#if 0
  struct string *httpSeperator = &STRING_FROM_ZERO_TERMINATED("\r\n");

  u64 httpFieldCount;
  StringSplit(&response, httpSeperator, &httpFieldCount, 0);

  struct string *httpFields = MemoryArenaPushUnaligned(&stackMemory, sizeof(*httpFields) * httpFieldCount);
  StringSplit(&response, httpSeperator, &httpFieldCount, httpFields);

  for (u32 httpFieldIndex = 1; httpFieldIndex < httpFieldCount; httpFieldIndex++) {
    struct string *httpField = httpFields + httpFieldIndex;
    if (IsStringEqualIgnoreCase(httpField, &STRING_FROM_ZERO_TERMINATED("HTTP/1.1 200 OK"))) {
      struct string message = STRING_FROM_ZERO_TERMINATED("OK\n");
      write(STDOUT_FILENO, message.value, message.length);
    } else if (IsStringEqualIgnoreCase(httpField, &STRING_FROM_ZERO_TERMINATED("content-type: application/json"))) {
      struct string message = STRING_FROM_ZERO_TERMINATED("json\n");
      write(STDOUT_FILENO, message.value, message.length);
    }
  }
#endif

  struct http_parser parser;
  HttpParserInit(&parser);
  HttpParserMustHaveStatusCode(&parser, 200);

  struct json_token jsonTokens[128];
  struct json_parser jsonParser = JsonParser(jsonTokens, ARRAY_COUNT(jsonTokens));
  u32 jsonTokenCount;
  HttpParserMustBeJson(&parser, &jsonParser);
  if (HttpParserParse(&parser, &response)) {
    return 1;
  }

#if 0
  for (u32 index = 0; index < jsonTokenCount; index++) {
    json_token jsonToken = jsonTokens + index;
    if jsonToken is object
      index++
      jsonToken = jsonTokens + index;
      
      if 
  }
#endif

  return 0;
}
