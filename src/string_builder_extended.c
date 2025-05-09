internalfn inline void
StringBuilderAppendHumanReadableBytes(string_builder *sb, u64 bytes)
{
  enum {
    PETABYTES = (1UL << 50),
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
      {.magnitude = PETABYTES, .unit = StringFromLiteral("PiB")},
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

#if defined(MBEDTLS_CIPHER_C)
#include <mbedtls/cipher.h>
#endif /* MBEDTLS_CIPHER_C */
#if defined(MBEDTLS_PEM_PARSE_C) || defined(MBEDTLS_PEM_WRITE_C)
#include <mbedtls/pem.h>
#endif /* MBEDTLS_PEM_PARSE_C || MBEDTLS_PEM_WRITE_C */
#if defined(MBEDTLS_BASE64_C)
#include <mbedtls/base64.h>
#endif /* MBEDTLS_BASE64_C */
#if defined(MBEDTLS_ERROR_C)
#include <mbedtls/error.h>
#endif /* MBEDTLS_ERROR_C */
#if defined(MBEDTLS_HKDF_C)
#include <mbedtls/hkdf.h>
#endif /* MBEDTLS_HKDF_C */
#if defined(MBEDTLS_OID_C)
#include <mbedtls/oid.h>
#endif /* MBEDTLS_OID_C */

internalfn inline void
StringBuilderAppendMbedtlsError(string_builder *sb, int errnum)
{
  int highLevelErrorCode;
  struct string highLevelError;
  int lowLevelErrorCode;
  struct string lowLevelError;

  b8 isErrnumNegative = 0;
  if (errnum < 0) {
    errnum = -errnum;
    isErrnumNegative = 1;
  }

  /* Extract the high-level part from the error code. */
  highLevelErrorCode = errnum & 0xFF80;
  highLevelError = (struct string){.value = 0, .length = 0};
  switch (highLevelErrorCode) {
#if defined(MBEDTLS_CIPHER_C)
  case -(MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("CIPHER - The selected feature is not available");
    break;
  case -(MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("CIPHER - Bad input parameters");
    break;
  case -(MBEDTLS_ERR_CIPHER_ALLOC_FAILED):
    highLevelError = StringFromLiteral("CIPHER - Failed to allocate memory");
    break;
  case -(MBEDTLS_ERR_CIPHER_INVALID_PADDING):
    highLevelError = StringFromLiteral("CIPHER - Input data contains invalid padding and is rejected");
    break;
  case -(MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED):
    highLevelError = StringFromLiteral("CIPHER - Decryption of block requires a full block");
    break;
  case -(MBEDTLS_ERR_CIPHER_AUTH_FAILED):
    highLevelError = StringFromLiteral("CIPHER - Authentication failed (for AEAD modes)");
    break;
  case -(MBEDTLS_ERR_CIPHER_INVALID_CONTEXT):
    highLevelError = StringFromLiteral("CIPHER - The context is invalid. For example, because it was freed");
    break;
#endif /* MBEDTLS_CIPHER_C */
#if defined(MBEDTLS_DHM_C)
  case -(MBEDTLS_ERR_DHM_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("DHM - Bad input parameters");
    break;
  case -(MBEDTLS_ERR_DHM_READ_PARAMS_FAILED):
    highLevelError = StringFromLiteral("DHM - Reading of the DHM parameters failed");
    break;
  case -(MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED):
    highLevelError = StringFromLiteral("DHM - Making of the DHM parameters failed");
    break;
  case -(MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED):
    highLevelError = StringFromLiteral("DHM - Reading of the public values failed");
    break;
  case -(MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED):
    highLevelError = StringFromLiteral("DHM - Making of the public value failed");
    break;
  case -(MBEDTLS_ERR_DHM_CALC_SECRET_FAILED):
    highLevelError = StringFromLiteral("DHM - Calculation of the DHM secret failed");
    break;
  case -(MBEDTLS_ERR_DHM_INVALID_FORMAT):
    highLevelError = StringFromLiteral("DHM - The ASN.1 data is not formatted correctly");
    break;
  case -(MBEDTLS_ERR_DHM_ALLOC_FAILED):
    highLevelError = StringFromLiteral("DHM - Allocation of memory failed");
    break;
  case -(MBEDTLS_ERR_DHM_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("DHM - Read or write of file failed");
    break;
  case -(MBEDTLS_ERR_DHM_SET_GROUP_FAILED):
    highLevelError = StringFromLiteral("DHM - Setting the modulus and generator failed");
    break;
#endif /* MBEDTLS_DHM_C */
#if defined(MBEDTLS_ECP_C)
  case -(MBEDTLS_ERR_ECP_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("ECP - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("ECP - The buffer is too small to write to");
    break;
  case -(MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral(
        "ECP - The requested feature is not available, for example, the requested curve is not supported");
    break;
  case -(MBEDTLS_ERR_ECP_VERIFY_FAILED):
    highLevelError = StringFromLiteral("ECP - The signature is not valid");
    break;
  case -(MBEDTLS_ERR_ECP_ALLOC_FAILED):
    highLevelError = StringFromLiteral("ECP - Memory allocation failed");
    break;
  case -(MBEDTLS_ERR_ECP_RANDOM_FAILED):
    highLevelError = StringFromLiteral("ECP - Generation of random value, such as ephemeral key, failed");
    break;
  case -(MBEDTLS_ERR_ECP_INVALID_KEY):
    highLevelError = StringFromLiteral("ECP - Invalid private or public key");
    break;
  case -(MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH):
    highLevelError = StringFromLiteral("ECP - The buffer contains a valid signature followed by more data");
    break;
  case -(MBEDTLS_ERR_ECP_IN_PROGRESS):
    highLevelError = StringFromLiteral("ECP - Operation in progress, call again with the same parameters to continue");
    break;
#endif /* MBEDTLS_ECP_C */
#if defined(MBEDTLS_MD_C)
  case -(MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("MD - The selected feature is not available");
    break;
  case -(MBEDTLS_ERR_MD_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("MD - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_MD_ALLOC_FAILED):
    highLevelError = StringFromLiteral("MD - Failed to allocate memory");
    break;
  case -(MBEDTLS_ERR_MD_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("MD - Opening or reading of file failed");
    break;
#endif /* MBEDTLS_MD_C */
#if defined(MBEDTLS_PEM_PARSE_C) || defined(MBEDTLS_PEM_WRITE_C)
  case -(MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT):
    highLevelError = StringFromLiteral("PEM - No PEM header or footer found");
    break;
  case -(MBEDTLS_ERR_PEM_INVALID_DATA):
    highLevelError = StringFromLiteral("PEM - PEM string is not as expected");
    break;
  case -(MBEDTLS_ERR_PEM_ALLOC_FAILED):
    highLevelError = StringFromLiteral("PEM - Failed to allocate memory");
    break;
  case -(MBEDTLS_ERR_PEM_INVALID_ENC_IV):
    highLevelError = StringFromLiteral("PEM - RSA IV is not in hex-format");
    break;
  case -(MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG):
    highLevelError = StringFromLiteral("PEM - Unsupported key encryption algorithm");
    break;
  case -(MBEDTLS_ERR_PEM_PASSWORD_REQUIRED):
    highLevelError = StringFromLiteral("PEM - Private key password can't be empty");
    break;
  case -(MBEDTLS_ERR_PEM_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PEM - Given private key password does not allow for correct decryption");
    break;
  case -(MBEDTLS_ERR_PEM_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PEM - Unavailable feature, e.g. hashing/encryption combination");
    break;
  case -(MBEDTLS_ERR_PEM_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PEM - Bad input parameters to function");
    break;
#endif /* MBEDTLS_PEM_PARSE_C || MBEDTLS_PEM_WRITE_C */
#if defined(MBEDTLS_PK_C)
  case -(MBEDTLS_ERR_PK_ALLOC_FAILED):
    highLevelError = StringFromLiteral("PK - Memory allocation failed");
    break;
  case -(MBEDTLS_ERR_PK_TYPE_MISMATCH):
    highLevelError = StringFromLiteral("PK - Type mismatch, eg attempt to encrypt with an ECDSA key");
    break;
  case -(MBEDTLS_ERR_PK_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PK - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_PK_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("PK - Read/write of file failed");
    break;
  case -(MBEDTLS_ERR_PK_KEY_INVALID_VERSION):
    highLevelError = StringFromLiteral("PK - Unsupported key version");
    break;
  case -(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PK - Invalid key tag or value");
    break;
  case -(MBEDTLS_ERR_PK_UNKNOWN_PK_ALG):
    highLevelError = StringFromLiteral("PK - Key algorithm is unsupported (only RSA and EC are supported)");
    break;
  case -(MBEDTLS_ERR_PK_PASSWORD_REQUIRED):
    highLevelError = StringFromLiteral("PK - Private key password can't be empty");
    break;
  case -(MBEDTLS_ERR_PK_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PK - Given private key password does not allow for correct decryption");
    break;
  case -(MBEDTLS_ERR_PK_INVALID_PUBKEY):
    highLevelError = StringFromLiteral("PK - The pubkey tag or value is invalid (only RSA and EC are supported)");
    break;
  case -(MBEDTLS_ERR_PK_INVALID_ALG):
    highLevelError = StringFromLiteral("PK - The algorithm tag or value is invalid");
    break;
  case -(MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE):
    highLevelError = StringFromLiteral("PK - Elliptic curve is unsupported (only NIST curves are supported)");
    break;
  case -(MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PK - Unavailable feature, e.g. RSA disabled for RSA key");
    break;
  case -(MBEDTLS_ERR_PK_SIG_LEN_MISMATCH):
    highLevelError = StringFromLiteral("PK - The buffer contains a valid signature followed by more data");
    break;
  case -(MBEDTLS_ERR_PK_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("PK - The output buffer is too small");
    break;
#endif /* MBEDTLS_PK_C */
#if defined(MBEDTLS_PKCS12_C)
  case -(MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PKCS12 - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_PKCS12_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PKCS12 - Feature not available, e.g. unsupported encryption scheme");
    break;
  case -(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PKCS12 - PBE ASN.1 data not as expected");
    break;
  case -(MBEDTLS_ERR_PKCS12_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PKCS12 - Given private key password does not allow for correct decryption");
    break;
#endif /* MBEDTLS_PKCS12_C */
#if defined(MBEDTLS_PKCS5_C)
  case -(MBEDTLS_ERR_PKCS5_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PKCS5 - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_PKCS5_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PKCS5 - Unexpected ASN.1 data");
    break;
  case -(MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PKCS5 - Requested encryption or digest alg not available");
    break;
  case -(MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PKCS5 - Given private key password does not allow for correct decryption");
    break;
#endif /* MBEDTLS_PKCS5_C */
#if defined(MBEDTLS_PKCS7_C)
  case -(MBEDTLS_ERR_PKCS7_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PKCS7 - The format is invalid, e.g. different type expected");
    break;
  case -(MBEDTLS_ERR_PKCS7_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PKCS7 - Unavailable feature, e.g. anything other than signed data");
    break;
  case -(MBEDTLS_ERR_PKCS7_INVALID_VERSION):
    highLevelError = StringFromLiteral("PKCS7 - The PKCS #7 version element is invalid or cannot be parsed");
    break;
  case -(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO):
    highLevelError = StringFromLiteral("PKCS7 - The PKCS #7 content info is invalid or cannot be parsed");
    break;
  case -(MBEDTLS_ERR_PKCS7_INVALID_ALG):
    highLevelError = StringFromLiteral("PKCS7 - The algorithm tag or value is invalid or cannot be parsed");
    break;
  case -(MBEDTLS_ERR_PKCS7_INVALID_CERT):
    highLevelError = StringFromLiteral("PKCS7 - The certificate tag or value is invalid or cannot be parsed");
    break;
  case -(MBEDTLS_ERR_PKCS7_INVALID_SIGNATURE):
    highLevelError = StringFromLiteral("PKCS7 - Error parsing the signature");
    break;
  case -(MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO):
    highLevelError = StringFromLiteral("PKCS7 - Error parsing the signer's info");
    break;
  case -(MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PKCS7 - Input invalid");
    break;
  case -(MBEDTLS_ERR_PKCS7_ALLOC_FAILED):
    highLevelError = StringFromLiteral("PKCS7 - Allocation of memory failed");
    break;
  case -(MBEDTLS_ERR_PKCS7_VERIFY_FAIL):
    highLevelError = StringFromLiteral("PKCS7 - Verification Failed");
    break;
  case -(MBEDTLS_ERR_PKCS7_CERT_DATE_INVALID):
    highLevelError = StringFromLiteral("PKCS7 - The PKCS #7 date issued/expired dates are invalid");
    break;
#endif /* MBEDTLS_PKCS7_C */
#if defined(MBEDTLS_RSA_C)
  case -(MBEDTLS_ERR_RSA_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("RSA - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_RSA_INVALID_PADDING):
    highLevelError = StringFromLiteral("RSA - Input data contains invalid padding and is rejected");
    break;
  case -(MBEDTLS_ERR_RSA_KEY_GEN_FAILED):
    highLevelError = StringFromLiteral("RSA - Something failed during generation of a key");
    break;
  case -(MBEDTLS_ERR_RSA_KEY_CHECK_FAILED):
    highLevelError = StringFromLiteral("RSA - Key failed to pass the validity check of the library");
    break;
  case -(MBEDTLS_ERR_RSA_PUBLIC_FAILED):
    highLevelError = StringFromLiteral("RSA - The public key operation failed");
    break;
  case -(MBEDTLS_ERR_RSA_PRIVATE_FAILED):
    highLevelError = StringFromLiteral("RSA - The private key operation failed");
    break;
  case -(MBEDTLS_ERR_RSA_VERIFY_FAILED):
    highLevelError = StringFromLiteral("RSA - The PKCS#1 verification failed");
    break;
  case -(MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE):
    highLevelError = StringFromLiteral("RSA - The output buffer for decryption is not large enough");
    break;
  case -(MBEDTLS_ERR_RSA_RNG_FAILED):
    highLevelError = StringFromLiteral("RSA - The random generator failed to generate non-zeros");
    break;
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_SSL_TLS_C)
  case -(MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS):
    highLevelError = StringFromLiteral("SSL - A cryptographic operation is in progress. Try again later");
    break;
  case -(MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("SSL - The requested feature is not available");
    break;
  case -(MBEDTLS_ERR_SSL_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("SSL - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_SSL_INVALID_MAC):
    highLevelError = StringFromLiteral("SSL - Verification of the message MAC failed");
    break;
  case -(MBEDTLS_ERR_SSL_INVALID_RECORD):
    highLevelError = StringFromLiteral("SSL - An invalid SSL record was received");
    break;
  case -(MBEDTLS_ERR_SSL_CONN_EOF):
    highLevelError = StringFromLiteral("SSL - The connection indicated an EOF");
    break;
  case -(MBEDTLS_ERR_SSL_DECODE_ERROR):
    highLevelError = StringFromLiteral("SSL - A message could not be parsed due to a syntactic error");
    break;
  case -(MBEDTLS_ERR_SSL_NO_RNG):
    highLevelError = StringFromLiteral("SSL - No RNG was provided to the SSL module");
    break;
  case -(MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE):
    highLevelError = StringFromLiteral(
        "SSL - No client certification received from the client, but required by the authentication mode");
    break;
  case -(MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION):
    highLevelError =
        StringFromLiteral("SSL - Client received an extended server hello containing an unsupported extension");
    break;
  case -(MBEDTLS_ERR_SSL_NO_APPLICATION_PROTOCOL):
    highLevelError = StringFromLiteral("SSL - No ALPN protocols supported that the client advertises");
    break;
  case -(MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED):
    highLevelError = StringFromLiteral("SSL - The own private key or pre-shared key is not set, but needed");
    break;
  case -(MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED):
    highLevelError = StringFromLiteral("SSL - No CA Chain is set, but required to operate");
    break;
  case -(MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE):
    highLevelError = StringFromLiteral("SSL - An unexpected message was received from our peer");
    break;
  case -(MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE):
    highLevelError = StringFromLiteral("SSL - A fatal alert message was received from our peer");
    break;
  case -(MBEDTLS_ERR_SSL_UNRECOGNIZED_NAME):
    highLevelError = StringFromLiteral("SSL - No server could be identified matching the client's SNI");
    break;
  case -(MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY):
    highLevelError = StringFromLiteral("SSL - The peer notified us that the connection is going to be closed");
    break;
  case -(MBEDTLS_ERR_SSL_BAD_CERTIFICATE):
    highLevelError = StringFromLiteral("SSL - Processing of the Certificate handshake message failed");
    break;
  case -(MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET):
    highLevelError = StringFromLiteral("SSL - A TLS 1.3 NewSessionTicket message has been received");
    break;
  case -(MBEDTLS_ERR_SSL_CANNOT_READ_EARLY_DATA):
    highLevelError = StringFromLiteral("SSL - Not possible to read early data");
    break;
  case -(MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA):
    highLevelError = StringFromLiteral(
        "SSL - * Early data has been received as part of an on-going handshake. This error code can be returned only "
        "on server side if and only if early data has been enabled by means of the mbedtls_ssl_conf_early_data() API. "
        "This error code can then be returned by mbedtls_ssl_handshake(), mbedtls_ssl_handshake_step(), "
        "mbedtls_ssl_read() or mbedtls_ssl_write() if early data has been received as part of the handshake sequence "
        "they triggered. To read the early data, call mbedtls_ssl_read_early_data()");
    break;
  case -(MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA):
    highLevelError = StringFromLiteral("SSL - Not possible to write early data");
    break;
  case -(MBEDTLS_ERR_SSL_CACHE_ENTRY_NOT_FOUND):
    highLevelError = StringFromLiteral("SSL - Cache entry not found");
    break;
  case -(MBEDTLS_ERR_SSL_ALLOC_FAILED):
    highLevelError = StringFromLiteral("SSL - Memory allocation failed");
    break;
  case -(MBEDTLS_ERR_SSL_HW_ACCEL_FAILED):
    highLevelError = StringFromLiteral("SSL - Hardware acceleration function returned with error");
    break;
  case -(MBEDTLS_ERR_SSL_HW_ACCEL_FALLTHROUGH):
    highLevelError = StringFromLiteral("SSL - Hardware acceleration function skipped / left alone data");
    break;
  case -(MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION):
    highLevelError = StringFromLiteral("SSL - Handshake protocol not within min/max boundaries");
    break;
  case -(MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE):
    highLevelError = StringFromLiteral("SSL - The handshake negotiation failed");
    break;
  case -(MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED):
    highLevelError = StringFromLiteral("SSL - Session ticket has expired");
    break;
  case -(MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH):
    highLevelError =
        StringFromLiteral("SSL - Public key type mismatch (eg, asked for RSA key exchange and presented EC key)");
    break;
  case -(MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY):
    highLevelError = StringFromLiteral("SSL - Unknown identity received (eg, PSK identity)");
    break;
  case -(MBEDTLS_ERR_SSL_INTERNAL_ERROR):
    highLevelError = StringFromLiteral("SSL - Internal error (eg, unexpected failure in lower-level module)");
    break;
  case -(MBEDTLS_ERR_SSL_COUNTER_WRAPPING):
    highLevelError = StringFromLiteral("SSL - A counter would wrap (eg, too many messages exchanged)");
    break;
  case -(MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO):
    highLevelError = StringFromLiteral("SSL - Unexpected message at ServerHello in renegotiation");
    break;
  case -(MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED):
    highLevelError = StringFromLiteral("SSL - DTLS client must retry for hello verification");
    break;
  case -(MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("SSL - A buffer is too small to receive or write a message");
    break;
  case -(MBEDTLS_ERR_SSL_WANT_READ):
    highLevelError = StringFromLiteral("SSL - No data of requested type currently available on underlying transport");
    break;
  case -(MBEDTLS_ERR_SSL_WANT_WRITE):
    highLevelError = StringFromLiteral("SSL - Connection requires a write call");
    break;
  case -(MBEDTLS_ERR_SSL_TIMEOUT):
    highLevelError = StringFromLiteral("SSL - The operation timed out");
    break;
  case -(MBEDTLS_ERR_SSL_CLIENT_RECONNECT):
    highLevelError = StringFromLiteral("SSL - The client initiated a reconnect from the same port");
    break;
  case -(MBEDTLS_ERR_SSL_UNEXPECTED_RECORD):
    highLevelError = StringFromLiteral("SSL - Record header looks valid but is not expected");
    break;
  case -(MBEDTLS_ERR_SSL_NON_FATAL):
    highLevelError = StringFromLiteral("SSL - The alert message received indicates a non-fatal error");
    break;
  case -(MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER):
    highLevelError = StringFromLiteral("SSL - A field in a message was incorrect or inconsistent with other fields");
    break;
  case -(MBEDTLS_ERR_SSL_CONTINUE_PROCESSING):
    highLevelError =
        StringFromLiteral("SSL - Internal-only message signaling that further message-processing should be done");
    break;
  case -(MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS):
    highLevelError = StringFromLiteral("SSL - The asynchronous operation is not completed yet");
    break;
  case -(MBEDTLS_ERR_SSL_EARLY_MESSAGE):
    highLevelError = StringFromLiteral("SSL - Internal-only message signaling that a message arrived early");
    break;
  case -(MBEDTLS_ERR_SSL_UNEXPECTED_CID):
    highLevelError = StringFromLiteral("SSL - An encrypted DTLS-frame with an unexpected CID was received");
    break;
  case -(MBEDTLS_ERR_SSL_VERSION_MISMATCH):
    highLevelError = StringFromLiteral("SSL - An operation failed due to an unexpected version or configuration");
    break;
  case -(MBEDTLS_ERR_SSL_BAD_CONFIG):
    highLevelError = StringFromLiteral("SSL - Invalid value in SSL config");
    break;
  case -(MBEDTLS_ERR_SSL_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME):
    highLevelError = StringFromLiteral(
        "SSL - Attempt to verify a certificate without an expected hostname. This is usually insecure.  In TLS "
        "clients, when a client authenticates a server through its certificate, the client normally checks three "
        "things: - the certificate chain must be valid; - the chain must start from a trusted CA; - the "
        "certificate must cover the server name that is expected by the client.  Omitting any of these checks is "
        "generally insecure, and can allow a malicious server to impersonate a legitimate server.  The third check "
        "may be safely skipped in some unusual scenarios, such as networks where eavesdropping is a risk but not "
        "active attacks, or a private PKI where the client equally trusts all servers that are accredited by the "
        "root CA.  You should call mbedtls_ssl_set_hostname() with the expected server name before starting a TLS "
        "handshake on a client (unless the client is set up to only use PSK-based authentication, which does not "
        "rely on the host name). If you have determined that server name verification is not required for security "
        "in your scenario, call mbedtls_ssl_set_hostname() with \\p NULL as the server name.  This error is raised "
        "if all of the following conditions are met:  - A TLS client is configured with the authentication mode "
        "#MBEDTLS_SSL_VERIFY_REQUIRED (default). - Certificate authentication is enabled. - The client does not "
        "call mbedtls_ssl_set_hostname(). - The configuration option "
        "#MBEDTLS_SSL_CLI_ALLOW_WEAK_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME is not enabled");
    break;
#endif /* MBEDTLS_SSL_TLS_C */
#if defined(MBEDTLS_X509_USE_C) || defined(MBEDTLS_X509_CREATE_C)
  case -(MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("X509 - Unavailable feature, e.g. RSA hashing/encryption combination");
    break;
  case -(MBEDTLS_ERR_X509_UNKNOWN_OID):
    highLevelError = StringFromLiteral("X509 - Requested OID is unknown");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_FORMAT):
    highLevelError = StringFromLiteral("X509 - The CRT/CRL/CSR format is invalid, e.g. different type expected");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_VERSION):
    highLevelError = StringFromLiteral("X509 - The CRT/CRL/CSR version element is invalid");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_SERIAL):
    highLevelError = StringFromLiteral("X509 - The serial tag or value is invalid");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_ALG):
    highLevelError = StringFromLiteral("X509 - The algorithm tag or value is invalid");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_NAME):
    highLevelError = StringFromLiteral("X509 - The name tag or value is invalid");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_DATE):
    highLevelError = StringFromLiteral("X509 - The date tag or value is invalid");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_SIGNATURE):
    highLevelError = StringFromLiteral("X509 - The signature tag or value invalid");
    break;
  case -(MBEDTLS_ERR_X509_INVALID_EXTENSIONS):
    highLevelError = StringFromLiteral("X509 - The extension tag or value is invalid");
    break;
  case -(MBEDTLS_ERR_X509_UNKNOWN_VERSION):
    highLevelError = StringFromLiteral("X509 - CRT/CRL/CSR has an unsupported version number");
    break;
  case -(MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG):
    highLevelError = StringFromLiteral("X509 - Signature algorithm (oid) is unsupported");
    break;
  case -(MBEDTLS_ERR_X509_SIG_MISMATCH):
    highLevelError =
        StringFromLiteral("X509 - Signature algorithms do not match. (see \\c ::mbedtls_x509_crt sig_oid)");
    break;
  case -(MBEDTLS_ERR_X509_CERT_VERIFY_FAILED):
    highLevelError =
        StringFromLiteral("X509 - Certificate verification failed, e.g. CRL, CA or signature check failed");
    break;
  case -(MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT):
    highLevelError = StringFromLiteral("X509 - Format not recognized as DER or PEM");
    break;
  case -(MBEDTLS_ERR_X509_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("X509 - Input invalid");
    break;
  case -(MBEDTLS_ERR_X509_ALLOC_FAILED):
    highLevelError = StringFromLiteral("X509 - Allocation of memory failed");
    break;
  case -(MBEDTLS_ERR_X509_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("X509 - Read/write of file failed");
    break;
  case -(MBEDTLS_ERR_X509_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("X509 - Destination buffer is too small");
    break;
  case -(MBEDTLS_ERR_X509_FATAL_ERROR):
    highLevelError =
        StringFromLiteral("X509 - A fatal error occurred, eg the chain is too long or the vrfy callback failed");
    break;
#endif /* MBEDTLS_X509_USE_C || MBEDTLS_X509_CREATE_C */
  }

  /* Extract the low-level part from the error code. */
  lowLevelErrorCode = errnum & ~0xFF80;

  lowLevelError = (struct string){.value = 0, .length = 0};
  switch (lowLevelErrorCode) {
#if defined(MBEDTLS_AES_C)
  case -(MBEDTLS_ERR_AES_INVALID_KEY_LENGTH):
    lowLevelError = StringFromLiteral("AES - Invalid key length");
    break;
  case -(MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("AES - Invalid data input length");
    break;
  case -(MBEDTLS_ERR_AES_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("AES - Invalid input data");
    break;
#endif /* MBEDTLS_AES_C */
#if defined(MBEDTLS_ARIA_C)
  case -(MBEDTLS_ERR_ARIA_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("ARIA - Bad input data");
    break;
  case -(MBEDTLS_ERR_ARIA_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("ARIA - Invalid data input length");
    break;
#endif /* MBEDTLS_ARIA_C */
#if defined(MBEDTLS_ASN1_PARSE_C)
  case -(MBEDTLS_ERR_ASN1_OUT_OF_DATA):
    lowLevelError = StringFromLiteral("ASN1 - Out of data when parsing an ASN1 data structure");
    break;
  case -(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG):
    lowLevelError = StringFromLiteral("ASN1 - ASN1 tag was of an unexpected value");
    break;
  case -(MBEDTLS_ERR_ASN1_INVALID_LENGTH):
    lowLevelError = StringFromLiteral("ASN1 - Error when trying to determine the length or invalid length");
    break;
  case -(MBEDTLS_ERR_ASN1_LENGTH_MISMATCH):
    lowLevelError = StringFromLiteral("ASN1 - Actual length differs from expected length");
    break;
  case -(MBEDTLS_ERR_ASN1_INVALID_DATA):
    lowLevelError = StringFromLiteral("ASN1 - Data is invalid");
    break;
  case -(MBEDTLS_ERR_ASN1_ALLOC_FAILED):
    lowLevelError = StringFromLiteral("ASN1 - Memory allocation failed");
    break;
  case -(MBEDTLS_ERR_ASN1_BUF_TOO_SMALL):
    lowLevelError = StringFromLiteral("ASN1 - Buffer too small when writing ASN.1 data structure");
    break;
#endif /* MBEDTLS_ASN1_PARSE_C */
#if defined(MBEDTLS_BASE64_C)
  case -(MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("BASE64 - Output buffer too small");
    break;
  case -(MBEDTLS_ERR_BASE64_INVALID_CHARACTER):
    lowLevelError = StringFromLiteral("BASE64 - Invalid character in input");
    break;
#endif /* MBEDTLS_BASE64_C */
#if defined(MBEDTLS_BIGNUM_C)
  case -(MBEDTLS_ERR_MPI_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("BIGNUM - An error occurred while reading from or writing to a file");
    break;
  case -(MBEDTLS_ERR_MPI_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("BIGNUM - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_MPI_INVALID_CHARACTER):
    lowLevelError = StringFromLiteral("BIGNUM - There is an invalid character in the digit string");
    break;
  case -(MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("BIGNUM - The buffer is too small to write to");
    break;
  case -(MBEDTLS_ERR_MPI_NEGATIVE_VALUE):
    lowLevelError = StringFromLiteral("BIGNUM - The input arguments are negative or result in illegal output");
    break;
  case -(MBEDTLS_ERR_MPI_DIVISION_BY_ZERO):
    lowLevelError = StringFromLiteral("BIGNUM - The input argument for division is zero, which is not allowed");
    break;
  case -(MBEDTLS_ERR_MPI_NOT_ACCEPTABLE):
    lowLevelError = StringFromLiteral("BIGNUM - The input arguments are not acceptable");
    break;
  case -(MBEDTLS_ERR_MPI_ALLOC_FAILED):
    lowLevelError = StringFromLiteral("BIGNUM - Memory allocation failed");
    break;
#endif /* MBEDTLS_BIGNUM_C */
#if defined(MBEDTLS_CAMELLIA_C)
  case -(MBEDTLS_ERR_CAMELLIA_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("CAMELLIA - Bad input data");
    break;
  case -(MBEDTLS_ERR_CAMELLIA_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("CAMELLIA - Invalid data input length");
    break;
#endif /* MBEDTLS_CAMELLIA_C */
#if defined(MBEDTLS_CCM_C)
  case -(MBEDTLS_ERR_CCM_BAD_INPUT):
    lowLevelError = StringFromLiteral("CCM - Bad input parameters to the function");
    break;
  case -(MBEDTLS_ERR_CCM_AUTH_FAILED):
    lowLevelError = StringFromLiteral("CCM - Authenticated decryption failed");
    break;
#endif /* MBEDTLS_CCM_C */
#if defined(MBEDTLS_CHACHA20_C)
  case -(MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("CHACHA20 - Invalid input parameter(s)");
    break;
#endif /* MBEDTLS_CHACHA20_C */
#if defined(MBEDTLS_CHACHAPOLY_C)
  case -(MBEDTLS_ERR_CHACHAPOLY_BAD_STATE):
    lowLevelError = StringFromLiteral("CHACHAPOLY - The requested operation is not permitted in the current state");
    break;
  case -(MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED):
    lowLevelError = StringFromLiteral("CHACHAPOLY - Authenticated decryption failed: data was not authentic");
    break;
#endif /* MBEDTLS_CHACHAPOLY_C */
#if defined(MBEDTLS_CTR_DRBG_C)
  case -(MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED):
    lowLevelError = StringFromLiteral("CTR_DRBG - The entropy source failed");
    break;
  case -(MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG):
    lowLevelError = StringFromLiteral("CTR_DRBG - The requested random buffer length is too big");
    break;
  case -(MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG):
    lowLevelError = StringFromLiteral("CTR_DRBG - The input (entropy + additional data) is too large");
    break;
  case -(MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("CTR_DRBG - Read or write error in file");
    break;
#endif /* MBEDTLS_CTR_DRBG_C */
#if defined(MBEDTLS_DES_C)
  case -(MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("DES - The data input has an invalid length");
    break;
#endif /* MBEDTLS_DES_C */

#if defined(MBEDTLS_ENTROPY_C)
  case -(MBEDTLS_ERR_ENTROPY_SOURCE_FAILED):
    lowLevelError = StringFromLiteral("ENTROPY - Critical entropy source failure");
    break;
  case -(MBEDTLS_ERR_ENTROPY_MAX_SOURCES):
    lowLevelError = StringFromLiteral("ENTROPY - No more sources can be added");
    break;
  case -(MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED):
    lowLevelError = StringFromLiteral("ENTROPY - No sources have been added to poll");
    break;
  case -(MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE):
    lowLevelError = StringFromLiteral("ENTROPY - No strong sources have been added to poll");
    break;
  case -(MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("ENTROPY - Read/write error in file");
    break;
#endif /* MBEDTLS_ENTROPY_C */
#if defined(MBEDTLS_ERROR_C)
  case -(MBEDTLS_ERR_ERROR_GENERIC_ERROR):
    lowLevelError = StringFromLiteral("ERROR - Generic error");
    break;
  case -(MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED):
    lowLevelError = StringFromLiteral("ERROR - This is a bug in the library");
    break;
#endif /* MBEDTLS_ERROR_C */
#if defined(MBEDTLS_PLATFORM_C)
  case -(MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED):
    lowLevelError = StringFromLiteral("PLATFORM - Hardware accelerator failed");
    break;
  case -(MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED):
    lowLevelError = StringFromLiteral("PLATFORM - The requested feature is not supported by the platform");
    break;
#endif /* MBEDTLS_PLATFORM_C */
#if defined(MBEDTLS_GCM_C)
  case -(MBEDTLS_ERR_GCM_AUTH_FAILED):
    lowLevelError = StringFromLiteral("GCM - Authenticated decryption failed");
    break;
  case -(MBEDTLS_ERR_GCM_BAD_INPUT):
    lowLevelError = StringFromLiteral("GCM - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_GCM_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("GCM - An output buffer is too small");
    break;
#endif /* MBEDTLS_GCM_C */
#if defined(MBEDTLS_HKDF_C)
  case -(MBEDTLS_ERR_HKDF_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("HKDF - Bad input parameters to function");
    break;
#endif /* MBEDTLS_HKDF_C */
#if defined(MBEDTLS_HMAC_DRBG_C)
  case -(MBEDTLS_ERR_HMAC_DRBG_REQUEST_TOO_BIG):
    lowLevelError = StringFromLiteral("HMAC_DRBG - Too many random requested in single call");
    break;
  case -(MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG):
    lowLevelError = StringFromLiteral("HMAC_DRBG - Input too large (Entropy + additional)");
    break;
  case -(MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("HMAC_DRBG - Read/write error in file");
    break;
  case -(MBEDTLS_ERR_HMAC_DRBG_ENTROPY_SOURCE_FAILED):
    lowLevelError = StringFromLiteral("HMAC_DRBG - The entropy source failed");
    break;
#endif /* MBEDTLS_HMAC_DRBG_C */
#if defined(MBEDTLS_LMS_C)
  case -(MBEDTLS_ERR_LMS_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("LMS - Bad data has been input to an LMS function");
    break;
  case -(MBEDTLS_ERR_LMS_OUT_OF_PRIVATE_KEYS):
    lowLevelError = StringFromLiteral("LMS - Specified LMS key has utilised all of its private keys");
    break;
  case -(MBEDTLS_ERR_LMS_VERIFY_FAILED):
    lowLevelError = StringFromLiteral("LMS - LMS signature verification failed");
    break;
  case -(MBEDTLS_ERR_LMS_ALLOC_FAILED):
    lowLevelError = StringFromLiteral("LMS - LMS failed to allocate space for a private key");
    break;
  case -(MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("LMS - Input/output buffer is too small to contain requited data");
    break;
#endif /* MBEDTLS_LMS_C */
#if defined(MBEDTLS_NET_C)
  case -(MBEDTLS_ERR_NET_SOCKET_FAILED):
    lowLevelError = StringFromLiteral("NET - Failed to open a socket");
    break;
  case -(MBEDTLS_ERR_NET_CONNECT_FAILED):
    lowLevelError = StringFromLiteral("NET - The connection to the given server / port failed");
    break;
  case -(MBEDTLS_ERR_NET_BIND_FAILED):
    lowLevelError = StringFromLiteral("NET - Binding of the socket failed");
    break;
  case -(MBEDTLS_ERR_NET_LISTEN_FAILED):
    lowLevelError = StringFromLiteral("NET - Could not listen on the socket");
    break;
  case -(MBEDTLS_ERR_NET_ACCEPT_FAILED):
    lowLevelError = StringFromLiteral("NET - Could not accept the incoming connection");
    break;
  case -(MBEDTLS_ERR_NET_RECV_FAILED):
    lowLevelError = StringFromLiteral("NET - Reading information from the socket failed");
    break;
  case -(MBEDTLS_ERR_NET_SEND_FAILED):
    lowLevelError = StringFromLiteral("NET - Sending information through the socket failed");
    break;
  case -(MBEDTLS_ERR_NET_CONN_RESET):
    lowLevelError = StringFromLiteral("NET - Connection was reset by peer");
    break;
  case -(MBEDTLS_ERR_NET_UNKNOWN_HOST):
    lowLevelError = StringFromLiteral("NET - Failed to get an IP address for the given hostname");
    break;
  case -(MBEDTLS_ERR_NET_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("NET - Buffer is too small to hold the data");
    break;
  case -(MBEDTLS_ERR_NET_INVALID_CONTEXT):
    lowLevelError = StringFromLiteral("NET - The context is invalid, eg because it was free()ed");
    break;
  case -(MBEDTLS_ERR_NET_POLL_FAILED):
    lowLevelError = StringFromLiteral("NET - Polling the net context failed");
    break;
  case -(MBEDTLS_ERR_NET_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("NET - Input invalid");
    break;
#endif /* MBEDTLS_NET_C */
#if defined(MBEDTLS_OID_C)
  case -(MBEDTLS_ERR_OID_NOT_FOUND):
    lowLevelError = StringFromLiteral("OID - OID is not found");
    break;
  case -(MBEDTLS_ERR_OID_BUF_TOO_SMALL):
    lowLevelError = StringFromLiteral("OID - output buffer is too small");
    break;
#endif /* MBEDTLS_OID_C */
#if defined(MBEDTLS_POLY1305_C)
  case -(MBEDTLS_ERR_POLY1305_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("POLY1305 - Invalid input parameter(s)");
    break;
#endif /* MBEDTLS_POLY1305_C */
#if defined(MBEDTLS_SHA1_C)
  case -(MBEDTLS_ERR_SHA1_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA1 - SHA-1 input data was malformed");
    break;
#endif /* MBEDTLS_SHA1_C */
#if defined(MBEDTLS_SHA256_C)
  case -(MBEDTLS_ERR_SHA256_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA256 - SHA-256 input data was malformed");
    break;
#endif /* MBEDTLS_SHA256_C */
#if defined(MBEDTLS_SHA3_C)
  case -(MBEDTLS_ERR_SHA3_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA3 - SHA-3 input data was malformed");
    break;
#endif /* MBEDTLS_SHA3_C */
#if defined(MBEDTLS_SHA512_C)
  case -(MBEDTLS_ERR_SHA512_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA512 - SHA-512 input data was malformed");
    break;
#endif /* MBEDTLS_SHA512_C */
#if defined(MBEDTLS_THREADING_C)
  case -(MBEDTLS_ERR_THREADING_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("THREADING - Bad input parameters to function");
    break;
  case -(MBEDTLS_ERR_THREADING_MUTEX_ERROR):
    lowLevelError = StringFromLiteral("THREADING - Locking / unlocking / free failed with error code");
    break;
#endif /* MBEDTLS_THREADING_C */
  }

  if (!IsStringNull(&highLevelError))
    StringBuilderAppendString(sb, &highLevelError);

  if (!IsStringNull(&lowLevelError))
    StringBuilderAppendString(sb, &lowLevelError);

  StringBuilderAppendStringLiteral(sb, " (Errnum ");
  if (isErrnumNegative)
    StringBuilderAppendStringLiteral(sb, "-");
  StringBuilderAppendStringLiteral(sb, "0x");
  StringBuilderAppendHex(sb, (u64)errnum);
  StringBuilderAppendStringLiteral(sb, ")");
}

internalfn inline void
StringBuilderAppendWolfsslError(string_builder *sb, int errnum)
{
  /* see wolfssl-5.8.0/src/internal.c:26288
   *   const char* wolfSSL_ERR_reason_error_string(unsigned long e)
   */
  // TODO: get wolfssl errors
}
