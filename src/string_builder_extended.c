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
  highLevelError = StringNull();
  switch (highLevelErrorCode) {
#if defined(MBEDTLS_CIPHER_C)
  case 0x6080: // -(MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("CIPHER - The selected feature is not available");
    break;
  case 0x6100: // -(MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("CIPHER - Bad input parameters");
    break;
  case 0x6180: // -(MBEDTLS_ERR_CIPHER_ALLOC_FAILED):
    highLevelError = StringFromLiteral("CIPHER - Failed to allocate memory");
    break;
  case 0x6200: // -(MBEDTLS_ERR_CIPHER_INVALID_PADDING):
    highLevelError = StringFromLiteral("CIPHER - Input data contains invalid padding and is rejected");
    break;
  case 0x6280: // -(MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED):
    highLevelError = StringFromLiteral("CIPHER - Decryption of block requires a full block");
    break;
  case 0x6300: // -(MBEDTLS_ERR_CIPHER_AUTH_FAILED):
    highLevelError = StringFromLiteral("CIPHER - Authentication failed (for AEAD modes)");
    break;
  case 0x6380: // -(MBEDTLS_ERR_CIPHER_INVALID_CONTEXT):
    highLevelError = StringFromLiteral("CIPHER - The context is invalid. For example, because it was freed");
    break;
#endif /* MBEDTLS_CIPHER_C */
#if defined(MBEDTLS_DHM_C)
  case 0x3080: // -(MBEDTLS_ERR_DHM_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("DHM - Bad input parameters");
    break;
  case 0x3100: // -(MBEDTLS_ERR_DHM_READ_PARAMS_FAILED):
    highLevelError = StringFromLiteral("DHM - Reading of the DHM parameters failed");
    break;
  case 0x3180: // -(MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED):
    highLevelError = StringFromLiteral("DHM - Making of the DHM parameters failed");
    break;
  case 0x3200: // -(MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED):
    highLevelError = StringFromLiteral("DHM - Reading of the public values failed");
    break;
  case 0x3280: // -(MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED):
    highLevelError = StringFromLiteral("DHM - Making of the public value failed");
    break;
  case 0x3300: // -(MBEDTLS_ERR_DHM_CALC_SECRET_FAILED):
    highLevelError = StringFromLiteral("DHM - Calculation of the DHM secret failed");
    break;
  case 0x3380: // -(MBEDTLS_ERR_DHM_INVALID_FORMAT):
    highLevelError = StringFromLiteral("DHM - The ASN.1 data is not formatted correctly");
    break;
  case 0x3400: // -(MBEDTLS_ERR_DHM_ALLOC_FAILED):
    highLevelError = StringFromLiteral("DHM - Allocation of memory failed");
    break;
  case 0x3480: // -(MBEDTLS_ERR_DHM_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("DHM - Read or write of file failed");
    break;
  case 0x3580: // -(MBEDTLS_ERR_DHM_SET_GROUP_FAILED):
    highLevelError = StringFromLiteral("DHM - Setting the modulus and generator failed");
    break;
#endif /* MBEDTLS_DHM_C */
#if defined(MBEDTLS_ECP_C)
  case 0x4F80: // -(MBEDTLS_ERR_ECP_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("ECP - Bad input parameters to function");
    break;
  case 0x4F00: // -(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("ECP - The buffer is too small to write to");
    break;
  case 0x4E80: // -(MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral(
        "ECP - The requested feature is not available, for example, the requested curve is not supported");
    break;
  case 0x4E00: // -(MBEDTLS_ERR_ECP_VERIFY_FAILED):
    highLevelError = StringFromLiteral("ECP - The signature is not valid");
    break;
  case 0x4D80: // -(MBEDTLS_ERR_ECP_ALLOC_FAILED):
    highLevelError = StringFromLiteral("ECP - Memory allocation failed");
    break;
  case 0x4D00: // -(MBEDTLS_ERR_ECP_RANDOM_FAILED):
    highLevelError = StringFromLiteral("ECP - Generation of random value, such as ephemeral key, failed");
    break;
  case 0x4C80: // -(MBEDTLS_ERR_ECP_INVALID_KEY):
    highLevelError = StringFromLiteral("ECP - Invalid private or public key");
    break;
  case 0x4C00: // -(MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH):
    highLevelError = StringFromLiteral("ECP - The buffer contains a valid signature followed by more data");
    break;
  case 0x4B00: // -(MBEDTLS_ERR_ECP_IN_PROGRESS):
    highLevelError = StringFromLiteral("ECP - Operation in progress, call again with the same parameters to continue");
    break;
#endif /* MBEDTLS_ECP_C */
#if defined(MBEDTLS_MD_C)
  case 0x5080: // -(MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("MD - The selected feature is not available");
    break;
  case 0x5100: // -(MBEDTLS_ERR_MD_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("MD - Bad input parameters to function");
    break;
  case 0x5180: // -(MBEDTLS_ERR_MD_ALLOC_FAILED):
    highLevelError = StringFromLiteral("MD - Failed to allocate memory");
    break;
  case 0x5200: // -(MBEDTLS_ERR_MD_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("MD - Opening or reading of file failed");
    break;
#endif /* MBEDTLS_MD_C */
#if defined(MBEDTLS_PEM_PARSE_C) || defined(MBEDTLS_PEM_WRITE_C)
  case 0x1080: // -(MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT):
    highLevelError = StringFromLiteral("PEM - No PEM header or footer found");
    break;
  case 0x1100: // -(MBEDTLS_ERR_PEM_INVALID_DATA):
    highLevelError = StringFromLiteral("PEM - PEM string is not as expected");
    break;
  case 0x1180: // -(MBEDTLS_ERR_PEM_ALLOC_FAILED):
    highLevelError = StringFromLiteral("PEM - Failed to allocate memory");
    break;
  case 0x1200: // -(MBEDTLS_ERR_PEM_INVALID_ENC_IV):
    highLevelError = StringFromLiteral("PEM - RSA IV is not in hex-format");
    break;
  case 0x1280: // -(MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG):
    highLevelError = StringFromLiteral("PEM - Unsupported key encryption algorithm");
    break;
  case 0x1300: // -(MBEDTLS_ERR_PEM_PASSWORD_REQUIRED):
    highLevelError = StringFromLiteral("PEM - Private key password can't be empty");
    break;
  case 0x1380: // -(MBEDTLS_ERR_PEM_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PEM - Given private key password does not allow for correct decryption");
    break;
  case 0x1400: // -(MBEDTLS_ERR_PEM_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PEM - Unavailable feature, e.g. hashing/encryption combination");
    break;
  case 0x1480: // -(MBEDTLS_ERR_PEM_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PEM - Bad input parameters to function");
    break;
#endif /* MBEDTLS_PEM_PARSE_C || MBEDTLS_PEM_WRITE_C */
#if defined(MBEDTLS_PK_C)
  case 0x3F80: // -(MBEDTLS_ERR_PK_ALLOC_FAILED):
    highLevelError = StringFromLiteral("PK - Memory allocation failed");
    break;
  case 0x3F00: // -(MBEDTLS_ERR_PK_TYPE_MISMATCH):
    highLevelError = StringFromLiteral("PK - Type mismatch, eg attempt to encrypt with an ECDSA key");
    break;
  case 0x3E80: // -(MBEDTLS_ERR_PK_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PK - Bad input parameters to function");
    break;
  case 0x3E00: // -(MBEDTLS_ERR_PK_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("PK - Read/write of file failed");
    break;
  case 0x3D80: // -(MBEDTLS_ERR_PK_KEY_INVALID_VERSION):
    highLevelError = StringFromLiteral("PK - Unsupported key version");
    break;
  case 0x3D00: // -(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PK - Invalid key tag or value");
    break;
  case 0x3C80: // -(MBEDTLS_ERR_PK_UNKNOWN_PK_ALG):
    highLevelError = StringFromLiteral("PK - Key algorithm is unsupported (only RSA and EC are supported)");
    break;
  case 0x3C00: // -(MBEDTLS_ERR_PK_PASSWORD_REQUIRED):
    highLevelError = StringFromLiteral("PK - Private key password can't be empty");
    break;
  case 0x3B80: // -(MBEDTLS_ERR_PK_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PK - Given private key password does not allow for correct decryption");
    break;
  case 0x3B00: // -(MBEDTLS_ERR_PK_INVALID_PUBKEY):
    highLevelError = StringFromLiteral("PK - The pubkey tag or value is invalid (only RSA and EC are supported)");
    break;
  case 0x3A80: // -(MBEDTLS_ERR_PK_INVALID_ALG):
    highLevelError = StringFromLiteral("PK - The algorithm tag or value is invalid");
    break;
  case 0x3A00: // -(MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE):
    highLevelError = StringFromLiteral("PK - Elliptic curve is unsupported (only NIST curves are supported)");
    break;
  case 0x3980: // -(MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PK - Unavailable feature, e.g. RSA disabled for RSA key");
    break;
  case 0x3900: // -(MBEDTLS_ERR_PK_SIG_LEN_MISMATCH):
    highLevelError = StringFromLiteral("PK - The buffer contains a valid signature followed by more data");
    break;
  case 0x3880: // -(MBEDTLS_ERR_PK_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("PK - The output buffer is too small");
    break;
#endif /* MBEDTLS_PK_C */
#if defined(MBEDTLS_PKCS12_C)
  case 0x1F80: // -(MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PKCS12 - Bad input parameters to function");
    break;
  case 0x1F00: // -(MBEDTLS_ERR_PKCS12_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PKCS12 - Feature not available, e.g. unsupported encryption scheme");
    break;
  case 0x1E80: // -(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PKCS12 - PBE ASN.1 data not as expected");
    break;
  case 0x1E00: // -(MBEDTLS_ERR_PKCS12_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PKCS12 - Given private key password does not allow for correct decryption");
    break;
#endif /* MBEDTLS_PKCS12_C */
#if defined(MBEDTLS_PKCS5_C)
  case 0x2f80: // -(MBEDTLS_ERR_PKCS5_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PKCS5 - Bad input parameters to function");
    break;
  case 0x2f00: // -(MBEDTLS_ERR_PKCS5_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PKCS5 - Unexpected ASN.1 data");
    break;
  case 0x2e80: // -(MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PKCS5 - Requested encryption or digest alg not available");
    break;
  case 0x2e00: // -(MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH):
    highLevelError = StringFromLiteral("PKCS5 - Given private key password does not allow for correct decryption");
    break;
#endif /* MBEDTLS_PKCS5_C */
#if defined(MBEDTLS_PKCS7_C)
  case 0x5300: // -(MBEDTLS_ERR_PKCS7_INVALID_FORMAT):
    highLevelError = StringFromLiteral("PKCS7 - The format is invalid, e.g. different type expected");
    break;
  case 0x5380: // -(MBEDTLS_ERR_PKCS7_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("PKCS7 - Unavailable feature, e.g. anything other than signed data");
    break;
  case 0x5400: // -(MBEDTLS_ERR_PKCS7_INVALID_VERSION):
    highLevelError = StringFromLiteral("PKCS7 - The PKCS #7 version element is invalid or cannot be parsed");
    break;
  case 0x5480: // -(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO):
    highLevelError = StringFromLiteral("PKCS7 - The PKCS #7 content info is invalid or cannot be parsed");
    break;
  case 0x5500: // -(MBEDTLS_ERR_PKCS7_INVALID_ALG):
    highLevelError = StringFromLiteral("PKCS7 - The algorithm tag or value is invalid or cannot be parsed");
    break;
  case 0x5580: // -(MBEDTLS_ERR_PKCS7_INVALID_CERT):
    highLevelError = StringFromLiteral("PKCS7 - The certificate tag or value is invalid or cannot be parsed");
    break;
  case 0x5600: // -(MBEDTLS_ERR_PKCS7_INVALID_SIGNATURE):
    highLevelError = StringFromLiteral("PKCS7 - Error parsing the signature");
    break;
  case 0x5680: // -(MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO):
    highLevelError = StringFromLiteral("PKCS7 - Error parsing the signer's info");
    break;
  case 0x5700: // -(MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("PKCS7 - Input invalid");
    break;
  case 0x5780: // -(MBEDTLS_ERR_PKCS7_ALLOC_FAILED):
    highLevelError = StringFromLiteral("PKCS7 - Allocation of memory failed");
    break;
  case 0x5800: // -(MBEDTLS_ERR_PKCS7_VERIFY_FAIL):
    highLevelError = StringFromLiteral("PKCS7 - Verification Failed");
    break;
  case 0x5880: // -(MBEDTLS_ERR_PKCS7_CERT_DATE_INVALID):
    highLevelError = StringFromLiteral("PKCS7 - The PKCS #7 date issued/expired dates are invalid");
    break;
#endif /* MBEDTLS_PKCS7_C */
#if defined(MBEDTLS_RSA_C)
  case 0x4080: // -(MBEDTLS_ERR_RSA_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("RSA - Bad input parameters to function");
    break;
  case 0x4100: // -(MBEDTLS_ERR_RSA_INVALID_PADDING):
    highLevelError = StringFromLiteral("RSA - Input data contains invalid padding and is rejected");
    break;
  case 0x4180: // -(MBEDTLS_ERR_RSA_KEY_GEN_FAILED):
    highLevelError = StringFromLiteral("RSA - Something failed during generation of a key");
    break;
  case 0x4200: // -(MBEDTLS_ERR_RSA_KEY_CHECK_FAILED):
    highLevelError = StringFromLiteral("RSA - Key failed to pass the validity check of the library");
    break;
  case 0x4280: // -(MBEDTLS_ERR_RSA_PUBLIC_FAILED):
    highLevelError = StringFromLiteral("RSA - The public key operation failed");
    break;
  case 0x4300: // -(MBEDTLS_ERR_RSA_PRIVATE_FAILED):
    highLevelError = StringFromLiteral("RSA - The private key operation failed");
    break;
  case 0x4380: // -(MBEDTLS_ERR_RSA_VERIFY_FAILED):
    highLevelError = StringFromLiteral("RSA - The PKCS#1 verification failed");
    break;
  case 0x4400: // -(MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE):
    highLevelError = StringFromLiteral("RSA - The output buffer for decryption is not large enough");
    break;
  case 0x4480: // -(MBEDTLS_ERR_RSA_RNG_FAILED):
    highLevelError = StringFromLiteral("RSA - The random generator failed to generate non-zeros");
    break;
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_SSL_TLS_C)
  case 0x7000: // -(MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS):
    highLevelError = StringFromLiteral("SSL - A cryptographic operation is in progress. Try again later");
    break;
  case 0x7080: // -(MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("SSL - The requested feature is not available");
    break;
  case 0x7100: // -(MBEDTLS_ERR_SSL_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("SSL - Bad input parameters to function");
    break;
  case 0x7180: // -(MBEDTLS_ERR_SSL_INVALID_MAC):
    highLevelError = StringFromLiteral("SSL - Verification of the message MAC failed");
    break;
  case 0x7200: // -(MBEDTLS_ERR_SSL_INVALID_RECORD):
    highLevelError = StringFromLiteral("SSL - An invalid SSL record was received");
    break;
  case 0x7280: // -(MBEDTLS_ERR_SSL_CONN_EOF):
    highLevelError = StringFromLiteral("SSL - The connection indicated an EOF");
    break;
  case 0x7300: // -(MBEDTLS_ERR_SSL_DECODE_ERROR):
    highLevelError = StringFromLiteral("SSL - A message could not be parsed due to a syntactic error");
    break;
  case 0x7400: // -(MBEDTLS_ERR_SSL_NO_RNG):
    highLevelError = StringFromLiteral("SSL - No RNG was provided to the SSL module");
    break;
  case 0x7480: // -(MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE):
    highLevelError = StringFromLiteral(
        "SSL - No client certification received from the client, but required by the authentication mode");
    break;
  case 0x7500: // -(MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION):
    highLevelError =
        StringFromLiteral("SSL - Client received an extended server hello containing an unsupported extension");
    break;
  case 0x7580: // -(MBEDTLS_ERR_SSL_NO_APPLICATION_PROTOCOL):
    highLevelError = StringFromLiteral("SSL - No ALPN protocols supported that the client advertises");
    break;
  case 0x7600: // -(MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED):
    highLevelError = StringFromLiteral("SSL - The own private key or pre-shared key is not set, but needed");
    break;
  case 0x7680: // -(MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED):
    highLevelError = StringFromLiteral("SSL - No CA Chain is set, but required to operate");
    break;
  case 0x7700: // -(MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE):
    highLevelError = StringFromLiteral("SSL - An unexpected message was received from our peer");
    break;
  case 0x7780: // -(MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE):
    highLevelError = StringFromLiteral("SSL - A fatal alert message was received from our peer");
    break;
  case 0x7800: // -(MBEDTLS_ERR_SSL_UNRECOGNIZED_NAME):
    highLevelError = StringFromLiteral("SSL - No server could be identified matching the client's SNI");
    break;
  case 0x7880: // -(MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY):
    highLevelError = StringFromLiteral("SSL - The peer notified us that the connection is going to be closed");
  case 0x7A00: // -(MBEDTLS_ERR_SSL_BAD_CERTIFICATE):
    highLevelError = StringFromLiteral("SSL - Processing of the Certificate handshake message failed");
    break;
  case 0x7B00: // -(MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET):
    highLevelError = StringFromLiteral("SSL - A TLS 1.3 NewSessionTicket message has been received");
    break;
  case 0x7B80: // -(MBEDTLS_ERR_SSL_CANNOT_READ_EARLY_DATA):
    highLevelError = StringFromLiteral("SSL - Not possible to read early data");
    break;
  case 0x7C00: // -(MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA):
    highLevelError = StringFromLiteral(
        "SSL - * Early data has been received as part of an on-going handshake. This error code can be returned only "
        "on server side if and only if early data has been enabled by means of the mbedtls_ssl_conf_early_data() API. "
        "This error code can then be returned by mbedtls_ssl_handshake(), mbedtls_ssl_handshake_step(), "
        "mbedtls_ssl_read() or mbedtls_ssl_write() if early data has been received as part of the handshake sequence "
        "they triggered. To read the early data, call mbedtls_ssl_read_early_data()");
    break;
  case 0x7C80: // -(MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA):
    highLevelError = StringFromLiteral("SSL - Not possible to write early data");
    break;
  case 0x7E80: // -(MBEDTLS_ERR_SSL_CACHE_ENTRY_NOT_FOUND):
    highLevelError = StringFromLiteral("SSL - Cache entry not found");
    break;
  case 0x7F00: // -(MBEDTLS_ERR_SSL_ALLOC_FAILED):
    highLevelError = StringFromLiteral("SSL - Memory allocation failed");
    break;
  case 0x7F80: // -(MBEDTLS_ERR_SSL_HW_ACCEL_FAILED):
    highLevelError = StringFromLiteral("SSL - Hardware acceleration function returned with error");
    break;
  case 0x6F80: // -(MBEDTLS_ERR_SSL_HW_ACCEL_FALLTHROUGH):
    highLevelError = StringFromLiteral("SSL - Hardware acceleration function skipped / left alone data");
    break;
  case 0x6E80: // -(MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION):
    highLevelError = StringFromLiteral("SSL - Handshake protocol not within min/max boundaries");
    break;
  case 0x6E00: // -(MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE):
    highLevelError = StringFromLiteral("SSL - The handshake negotiation failed");
    break;
  case 0x6D80: // -(MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED):
    highLevelError = StringFromLiteral("SSL - Session ticket has expired");
    break;
  case 0x6D00: // -(MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH):
    highLevelError =
        StringFromLiteral("SSL - Public key type mismatch (eg, asked for RSA key exchange and presented EC key)");
    break;
  case 0x6C80: // -(MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY):
    highLevelError = StringFromLiteral("SSL - Unknown identity received (eg, PSK identity)");
    break;
  case 0x6C00: // -(MBEDTLS_ERR_SSL_INTERNAL_ERROR):
    highLevelError = StringFromLiteral("SSL - Internal error (eg, unexpected failure in lower-level module)");
    break;
  case 0x6B80: // -(MBEDTLS_ERR_SSL_COUNTER_WRAPPING):
    highLevelError = StringFromLiteral("SSL - A counter would wrap (eg, too many messages exchanged)");
    break;
  case 0x6B00: // -(MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO):
    highLevelError = StringFromLiteral("SSL - Unexpected message at ServerHello in renegotiation");
    break;
  case 0x6A80: // -(MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED):
    highLevelError = StringFromLiteral("SSL - DTLS client must retry for hello verification");
    break;
  case 0x6A00: // -(MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("SSL - A buffer is too small to receive or write a message");
    break;
  case 0x6900: // -(MBEDTLS_ERR_SSL_WANT_READ):
    highLevelError = StringFromLiteral("SSL - No data of requested type currently available on underlying transport");
    break;
  case 0x6880: // -(MBEDTLS_ERR_SSL_WANT_WRITE):
    highLevelError = StringFromLiteral("SSL - Connection requires a write call");
    break;
  case 0x6800: // -(MBEDTLS_ERR_SSL_TIMEOUT):
    highLevelError = StringFromLiteral("SSL - The operation timed out");
    break;
  case 0x6780: // -(MBEDTLS_ERR_SSL_CLIENT_RECONNECT):
    highLevelError = StringFromLiteral("SSL - The client initiated a reconnect from the same port");
    break;
  case 0x6700: // -(MBEDTLS_ERR_SSL_UNEXPECTED_RECORD):
    highLevelError = StringFromLiteral("SSL - Record header looks valid but is not expected");
    break;
  case 0x6680: // -(MBEDTLS_ERR_SSL_NON_FATAL):
    highLevelError = StringFromLiteral("SSL - The alert message received indicates a non-fatal error");
    break;
  case 0x6600: // -(MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER):
    highLevelError = StringFromLiteral("SSL - A field in a message was incorrect or inconsistent with other fields");
    break;
  case 0x6580: // -(MBEDTLS_ERR_SSL_CONTINUE_PROCESSING):
    highLevelError =
        StringFromLiteral("SSL - Internal-only message signaling that further message-processing should be done");
    break;
  case 0x6500: // -(MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS):
    highLevelError = StringFromLiteral("SSL - The asynchronous operation is not completed yet");
    break;
  case 0x6480: // -(MBEDTLS_ERR_SSL_EARLY_MESSAGE):
    highLevelError = StringFromLiteral("SSL - Internal-only message signaling that a message arrived early");
    break;
  case 0x6000: // -(MBEDTLS_ERR_SSL_UNEXPECTED_CID):
    highLevelError = StringFromLiteral("SSL - An encrypted DTLS-frame with an unexpected CID was received");
    break;
  case 0x5F00: // -(MBEDTLS_ERR_SSL_VERSION_MISMATCH):
    highLevelError = StringFromLiteral("SSL - An operation failed due to an unexpected version or configuration");
    break;
  case 0x5E80: // -(MBEDTLS_ERR_SSL_BAD_CONFIG):
    highLevelError = StringFromLiteral("SSL - Invalid value in SSL config");
    break;
  case 0x5D80: // -(MBEDTLS_ERR_SSL_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME):
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
  case 0x2080: // -(MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE):
    highLevelError = StringFromLiteral("X509 - Unavailable feature, e.g. RSA hashing/encryption combination");
    break;
  case 0x2100: // -(MBEDTLS_ERR_X509_UNKNOWN_OID):
    highLevelError = StringFromLiteral("X509 - Requested OID is unknown");
    break;
  case 0x2180: // -(MBEDTLS_ERR_X509_INVALID_FORMAT):
    highLevelError = StringFromLiteral("X509 - The CRT/CRL/CSR format is invalid, e.g. different type expected");
    break;
  case 0x2200: // -(MBEDTLS_ERR_X509_INVALID_VERSION):
    highLevelError = StringFromLiteral("X509 - The CRT/CRL/CSR version element is invalid");
    break;
  case 0x2280: // -(MBEDTLS_ERR_X509_INVALID_SERIAL):
    highLevelError = StringFromLiteral("X509 - The serial tag or value is invalid");
    break;
  case 0x2300: // -(MBEDTLS_ERR_X509_INVALID_ALG):
    highLevelError = StringFromLiteral("X509 - The algorithm tag or value is invalid");
    break;
  case 0x2380: // -(MBEDTLS_ERR_X509_INVALID_NAME):
    highLevelError = StringFromLiteral("X509 - The name tag or value is invalid");
    break;
  case 0x2400: // -(MBEDTLS_ERR_X509_INVALID_DATE):
    highLevelError = StringFromLiteral("X509 - The date tag or value is invalid");
    break;
  case 0x2480: // -(MBEDTLS_ERR_X509_INVALID_SIGNATURE):
    highLevelError = StringFromLiteral("X509 - The signature tag or value invalid");
    break;
  case 0x2500: // -(MBEDTLS_ERR_X509_INVALID_EXTENSIONS):
    highLevelError = StringFromLiteral("X509 - The extension tag or value is invalid");
    break;
  case 0x2580: // -(MBEDTLS_ERR_X509_UNKNOWN_VERSION):
    highLevelError = StringFromLiteral("X509 - CRT/CRL/CSR has an unsupported version number");
    break;
  case 0x2600: // -(MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG):
    highLevelError = StringFromLiteral("X509 - Signature algorithm (oid) is unsupported");
    break;
  case 0x2680: // -(MBEDTLS_ERR_X509_SIG_MISMATCH):
    highLevelError =
        StringFromLiteral("X509 - Signature algorithms do not match. (see \\c ::mbedtls_x509_crt sig_oid)");
    break;
  case 0x2700: // -(MBEDTLS_ERR_X509_CERT_VERIFY_FAILED):
    highLevelError =
        StringFromLiteral("X509 - Certificate verification failed, e.g. CRL, CA or signature check failed");
    break;
  case 0x2780: // -(MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT):
    highLevelError = StringFromLiteral("X509 - Format not recognized as DER or PEM");
    break;
  case 0x2800: // -(MBEDTLS_ERR_X509_BAD_INPUT_DATA):
    highLevelError = StringFromLiteral("X509 - Input invalid");
    break;
  case 0x2880: // -(MBEDTLS_ERR_X509_ALLOC_FAILED):
    highLevelError = StringFromLiteral("X509 - Allocation of memory failed");
    break;
  case 0x2900: // -(MBEDTLS_ERR_X509_FILE_IO_ERROR):
    highLevelError = StringFromLiteral("X509 - Read/write of file failed");
    break;
  case 0x2980: // -(MBEDTLS_ERR_X509_BUFFER_TOO_SMALL):
    highLevelError = StringFromLiteral("X509 - Destination buffer is too small");
    break;
  case 0x3000: // -(MBEDTLS_ERR_X509_FATAL_ERROR):
    highLevelError =
        StringFromLiteral("X509 - A fatal error occurred, eg the chain is too long or the vrfy callback failed");
    break;
#endif /* MBEDTLS_X509_USE_C || MBEDTLS_X509_CREATE_C */
  }

  /* Extract the low-level part from the error code. */
  lowLevelErrorCode = errnum & ~0xFF80;

  lowLevelError = StringNull();
  switch (lowLevelErrorCode) {
#if defined(MBEDTLS_AES_C)
  case 0x0020: // -(MBEDTLS_ERR_AES_INVALID_KEY_LENGTH):
    lowLevelError = StringFromLiteral("AES - Invalid key length");
    break;
  case 0x0022: // -(MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("AES - Invalid data input length");
    break;
  case 0x0021: // -(MBEDTLS_ERR_AES_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("AES - Invalid input data");
    break;
#endif /* MBEDTLS_AES_C */
#if defined(MBEDTLS_ARIA_C)
  case 0x005C: // -(MBEDTLS_ERR_ARIA_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("ARIA - Bad input data");
    break;
  case 0x005E: // -(MBEDTLS_ERR_ARIA_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("ARIA - Invalid data input length");
    break;
#endif /* MBEDTLS_ARIA_C */
#if defined(MBEDTLS_ASN1_PARSE_C)
  case 0x0060: // -(MBEDTLS_ERR_ASN1_OUT_OF_DATA):
    lowLevelError = StringFromLiteral("ASN1 - Out of data when parsing an ASN1 data structure");
    break;
  case 0x0062: // -(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG):
    lowLevelError = StringFromLiteral("ASN1 - ASN1 tag was of an unexpected value");
    break;
  case 0x0064: // -(MBEDTLS_ERR_ASN1_INVALID_LENGTH):
    lowLevelError = StringFromLiteral("ASN1 - Error when trying to determine the length or invalid length");
    break;
  case 0x0066: // -(MBEDTLS_ERR_ASN1_LENGTH_MISMATCH):
    lowLevelError = StringFromLiteral("ASN1 - Actual length differs from expected length");
    break;
  case 0x0068: // -(MBEDTLS_ERR_ASN1_INVALID_DATA):
    lowLevelError = StringFromLiteral("ASN1 - Data is invalid");
    break;
  case 0x006A: // -(MBEDTLS_ERR_ASN1_ALLOC_FAILED):
    lowLevelError = StringFromLiteral("ASN1 - Memory allocation failed");
    break;
  case 0x006C: // -(MBEDTLS_ERR_ASN1_BUF_TOO_SMALL):
    lowLevelError = StringFromLiteral("ASN1 - Buffer too small when writing ASN.1 data structure");
    break;
#endif /* MBEDTLS_ASN1_PARSE_C */
#if defined(MBEDTLS_BASE64_C)
  case 0x002A: // -(MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("BASE64 - Output buffer too small");
    break;
  case 0x002C: // -(MBEDTLS_ERR_BASE64_INVALID_CHARACTER):
    lowLevelError = StringFromLiteral("BASE64 - Invalid character in input");
    break;
#endif /* MBEDTLS_BASE64_C */
#if defined(MBEDTLS_BIGNUM_C)
  case 0x0002: // -(MBEDTLS_ERR_MPI_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("BIGNUM - An error occurred while reading from or writing to a file");
    break;
  case 0x0004: // -(MBEDTLS_ERR_MPI_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("BIGNUM - Bad input parameters to function");
    break;
  case 0x0006: // -(MBEDTLS_ERR_MPI_INVALID_CHARACTER):
    lowLevelError = StringFromLiteral("BIGNUM - There is an invalid character in the digit string");
    break;
  case 0x0008: // -(MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("BIGNUM - The buffer is too small to write to");
    break;
  case 0x000A: // -(MBEDTLS_ERR_MPI_NEGATIVE_VALUE):
    lowLevelError = StringFromLiteral("BIGNUM - The input arguments are negative or result in illegal output");
    break;
  case 0x000C: // -(MBEDTLS_ERR_MPI_DIVISION_BY_ZERO):
    lowLevelError = StringFromLiteral("BIGNUM - The input argument for division is zero, which is not allowed");
    break;
  case 0x000E: // -(MBEDTLS_ERR_MPI_NOT_ACCEPTABLE):
    lowLevelError = StringFromLiteral("BIGNUM - The input arguments are not acceptable");
    break;
  case 0x0010: // -(MBEDTLS_ERR_MPI_ALLOC_FAILED):
    lowLevelError = StringFromLiteral("BIGNUM - Memory allocation failed");
    break;
#endif /* MBEDTLS_BIGNUM_C */
#if defined(MBEDTLS_CAMELLIA_C)
  case 0x0024: // -(MBEDTLS_ERR_CAMELLIA_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("CAMELLIA - Bad input data");
    break;
  case 0x0026: // -(MBEDTLS_ERR_CAMELLIA_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("CAMELLIA - Invalid data input length");
    break;
#endif /* MBEDTLS_CAMELLIA_C */
#if defined(MBEDTLS_CCM_C)
  case 0x000D: // -(MBEDTLS_ERR_CCM_BAD_INPUT):
    lowLevelError = StringFromLiteral("CCM - Bad input parameters to the function");
    break;
  case 0x000F: // -(MBEDTLS_ERR_CCM_AUTH_FAILED):
    lowLevelError = StringFromLiteral("CCM - Authenticated decryption failed");
    break;
#endif /* MBEDTLS_CCM_C */
#if defined(MBEDTLS_CHACHA20_C)
  case 0x0051: // -(MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("CHACHA20 - Invalid input parameter(s)");
    break;
#endif /* MBEDTLS_CHACHA20_C */
#if defined(MBEDTLS_CHACHAPOLY_C)
  case 0x0054: // -(MBEDTLS_ERR_CHACHAPOLY_BAD_STATE):
    lowLevelError = StringFromLiteral("CHACHAPOLY - The requested operation is not permitted in the current state");
    break;
  case 0x0056: // -(MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED):
    lowLevelError = StringFromLiteral("CHACHAPOLY - Authenticated decryption failed: data was not authentic");
    break;
#endif /* MBEDTLS_CHACHAPOLY_C */
#if defined(MBEDTLS_CTR_DRBG_C)
  case 0x0034: // -(MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED):
    lowLevelError = StringFromLiteral("CTR_DRBG - The entropy source failed");
    break;
  case 0x0036: // -(MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG):
    lowLevelError = StringFromLiteral("CTR_DRBG - The requested random buffer length is too big");
    break;
  case 0x0038: // -(MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG):
    lowLevelError = StringFromLiteral("CTR_DRBG - The input (entropy + additional data) is too large");
    break;
  case 0x003A: // -(MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("CTR_DRBG - Read or write error in file");
    break;
#endif /* MBEDTLS_CTR_DRBG_C */
#if defined(MBEDTLS_DES_C)
  case 0x0032: // -(MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH):
    lowLevelError = StringFromLiteral("DES - The data input has an invalid length");
    break;
#endif /* MBEDTLS_DES_C */
#if defined(MBEDTLS_ENTROPY_C)
  case 0x003C: // -(MBEDTLS_ERR_ENTROPY_SOURCE_FAILED):
    lowLevelError = StringFromLiteral("ENTROPY - Critical entropy source failure");
    break;
  case 0x003E: // -(MBEDTLS_ERR_ENTROPY_MAX_SOURCES):
    lowLevelError = StringFromLiteral("ENTROPY - No more sources can be added");
    break;
  case 0x0040: // -(MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED):
    lowLevelError = StringFromLiteral("ENTROPY - No sources have been added to poll");
    break;
  case 0x003D: // -(MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE):
    lowLevelError = StringFromLiteral("ENTROPY - No strong sources have been added to poll");
    break;
  case 0x003F: // -(MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("ENTROPY - Read/write error in file");
    break;
#endif /* MBEDTLS_ENTROPY_C */
#if defined(MBEDTLS_ERROR_C)
  case 0x0001: // -(MBEDTLS_ERR_ERROR_GENERIC_ERROR):
    lowLevelError = StringFromLiteral("ERROR - Generic error");
    break;
  case 0x006E: // -(MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED):
    lowLevelError = StringFromLiteral("ERROR - This is a bug in the library");
    break;
#endif /* MBEDTLS_ERROR_C */
#if defined(MBEDTLS_PLATFORM_C)
  case 0x0070: // -(MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED):
    lowLevelError = StringFromLiteral("PLATFORM - Hardware accelerator failed");
    break;
  case 0x0072: // -(MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED):
    lowLevelError = StringFromLiteral("PLATFORM - The requested feature is not supported by the platform");
    break;
#endif /* MBEDTLS_PLATFORM_C */
#if defined(MBEDTLS_GCM_C)
  case 0x0012: // -(MBEDTLS_ERR_GCM_AUTH_FAILED):
    lowLevelError = StringFromLiteral("GCM - Authenticated decryption failed");
    break;
  case 0x0014: // -(MBEDTLS_ERR_GCM_BAD_INPUT):
    lowLevelError = StringFromLiteral("GCM - Bad input parameters to function");
    break;
  case 0x0016: // -(MBEDTLS_ERR_GCM_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("GCM - An output buffer is too small");
    break;
#endif /* MBEDTLS_GCM_C */
#if defined(MBEDTLS_HKDF_C)
  case 0x5F80: // -(MBEDTLS_ERR_HKDF_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("HKDF - Bad input parameters to function");
    break;
#endif /* MBEDTLS_HKDF_C */
#if defined(MBEDTLS_HMAC_DRBG_C)
  case 0x0003: // -(MBEDTLS_ERR_HMAC_DRBG_REQUEST_TOO_BIG):
    lowLevelError = StringFromLiteral("HMAC_DRBG - Too many random requested in single call");
    break;
  case 0x0005: // -(MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG):
    lowLevelError = StringFromLiteral("HMAC_DRBG - Input too large (Entropy + additional)");
    break;
  case 0x0007: // -(MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR):
    lowLevelError = StringFromLiteral("HMAC_DRBG - Read/write error in file");
    break;
  case 0x0009: // -(MBEDTLS_ERR_HMAC_DRBG_ENTROPY_SOURCE_FAILED):
    lowLevelError = StringFromLiteral("HMAC_DRBG - The entropy source failed");
    break;
#endif /* MBEDTLS_HMAC_DRBG_C */
#if defined(MBEDTLS_LMS_C)
  case 0x0011: // -(MBEDTLS_ERR_LMS_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("LMS - Bad data has been input to an LMS function");
    break;
  case 0x0013: // -(MBEDTLS_ERR_LMS_OUT_OF_PRIVATE_KEYS):
    lowLevelError = StringFromLiteral("LMS - Specified LMS key has utilised all of its private keys");
    break;
  case 0x0015: // -(MBEDTLS_ERR_LMS_VERIFY_FAILED):
    lowLevelError = StringFromLiteral("LMS - LMS signature verification failed");
    break;
  case 0x0017: // -(MBEDTLS_ERR_LMS_ALLOC_FAILED):
    lowLevelError = StringFromLiteral("LMS - LMS failed to allocate space for a private key");
    break;
  case 0x0019: // -(MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("LMS - Input/output buffer is too small to contain requited data");
    break;
#endif /* MBEDTLS_LMS_C */
#if defined(MBEDTLS_NET_C)
  case 0x0042: // -(MBEDTLS_ERR_NET_SOCKET_FAILED):
    lowLevelError = StringFromLiteral("NET - Failed to open a socket");
    break;
  case 0x0044: // -(MBEDTLS_ERR_NET_CONNECT_FAILED):
    lowLevelError = StringFromLiteral("NET - The connection to the given server / port failed");
    break;
  case 0x0046: // -(MBEDTLS_ERR_NET_BIND_FAILED):
    lowLevelError = StringFromLiteral("NET - Binding of the socket failed");
    break;
  case 0x0048: // -(MBEDTLS_ERR_NET_LISTEN_FAILED):
    lowLevelError = StringFromLiteral("NET - Could not listen on the socket");
    break;
  case 0x004A: // -(MBEDTLS_ERR_NET_ACCEPT_FAILED):
    lowLevelError = StringFromLiteral("NET - Could not accept the incoming connection");
    break;
  case 0x004C: // -(MBEDTLS_ERR_NET_RECV_FAILED):
    lowLevelError = StringFromLiteral("NET - Reading information from the socket failed");
    break;
  case 0x004E: // -(MBEDTLS_ERR_NET_SEND_FAILED):
    lowLevelError = StringFromLiteral("NET - Sending information through the socket failed");
    break;
  case 0x0050: // -(MBEDTLS_ERR_NET_CONN_RESET):
    lowLevelError = StringFromLiteral("NET - Connection was reset by peer");
    break;
  case 0x0052: // -(MBEDTLS_ERR_NET_UNKNOWN_HOST):
    lowLevelError = StringFromLiteral("NET - Failed to get an IP address for the given hostname");
    break;
  case 0x0043: // -(MBEDTLS_ERR_NET_BUFFER_TOO_SMALL):
    lowLevelError = StringFromLiteral("NET - Buffer is too small to hold the data");
    break;
  case 0x0045: // -(MBEDTLS_ERR_NET_INVALID_CONTEXT):
    lowLevelError = StringFromLiteral("NET - The context is invalid, eg because it was free()ed");
    break;
  case 0x0047: // -(MBEDTLS_ERR_NET_POLL_FAILED):
    lowLevelError = StringFromLiteral("NET - Polling the net context failed");
    break;
  case 0x0049: // -(MBEDTLS_ERR_NET_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("NET - Input invalid");
    break;
#endif /* MBEDTLS_NET_C */
#if defined(MBEDTLS_OID_C)
  case 0x002E: // -(MBEDTLS_ERR_OID_NOT_FOUND):
    lowLevelError = StringFromLiteral("OID - OID is not found");
    break;
  case 0x000B: // -(MBEDTLS_ERR_OID_BUF_TOO_SMALL):
    lowLevelError = StringFromLiteral("OID - output buffer is too small");
    break;
#endif /* MBEDTLS_OID_C */
#if defined(MBEDTLS_POLY1305_C)
  case 0x0057: // -(MBEDTLS_ERR_POLY1305_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("POLY1305 - Invalid input parameter(s)");
    break;
#endif /* MBEDTLS_POLY1305_C */
#if defined(MBEDTLS_SHA1_C)
  case 0x0073: // -(MBEDTLS_ERR_SHA1_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA1 - SHA-1 input data was malformed");
    break;
#endif /* MBEDTLS_SHA1_C */
#if defined(MBEDTLS_SHA256_C)
  case 0x0074: // -(MBEDTLS_ERR_SHA256_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA256 - SHA-256 input data was malformed");
    break;
#endif /* MBEDTLS_SHA256_C */
#if defined(MBEDTLS_SHA3_C)
  case 0x0076: // -(MBEDTLS_ERR_SHA3_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA3 - SHA-3 input data was malformed");
    break;
#endif /* MBEDTLS_SHA3_C */
#if defined(MBEDTLS_SHA512_C)
  case 0x0075: // -(MBEDTLS_ERR_SHA512_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("SHA512 - SHA-512 input data was malformed");
    break;
#endif /* MBEDTLS_SHA512_C */
#if defined(MBEDTLS_THREADING_C)
  case 0x001C: // -(MBEDTLS_ERR_THREADING_BAD_INPUT_DATA):
    lowLevelError = StringFromLiteral("THREADING - Bad input parameters to function");
    break;
  case 0x001E: // -(MBEDTLS_ERR_THREADING_MUTEX_ERROR):
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
  /* see
   * - wolfssl-5.8.0/wolfcrypt/src/error.c:36
   *   const char* wc_GetErrorString(int error)
   *
   * - wolfssl-5.8.0/src/internal.c:26288
   *   const char* wolfSSL_ERR_reason_error_string(unsigned long e)
   */

  if (errnum > 0)
    errnum = -errnum;

  struct string message = StringFromLiteral("Unknown wolfSSL error");

  if ((errnum <= -97 && errnum >= -300) || (errnum <= -1000 && errnum >= -1999)
      /* (error <= WC_SPAN1_FIRST_E && error >= WC_SPAN1_MIN_CODE_E) ||
       * (error <= WC_SPAN2_FIRST_E && error >= WC_SPAN2_MIN_CODE_E))
       */
  ) {
    message = StringFromLiteral("Unkonwn error message");

    switch (errnum) {
    case -1: // WC_FAILURE:
      message = StringFromLiteral("wolfCrypt generic failure");
      break;

    case -97: // MP_MEM :
      message = StringFromLiteral("MP integer dynamic memory allocation failed");
      break;

    case -98: // MP_VAL :
      message = StringFromLiteral("MP integer invalid argument");
      break;

    case -99: // MP_WOULDBLOCK :
      message = StringFromLiteral("MP integer non-blocking operation would block");
      break;

    case -100: // MP_NOT_INF:
      message = StringFromLiteral("MP point not at infinity");
      break;

    case -101: // OPEN_RAN_E :
      message = StringFromLiteral("opening random device error");
      break;

    case -102: // READ_RAN_E :
      message = StringFromLiteral("reading random device error");
      break;

    case -103: // WINCRYPT_E :
      message = StringFromLiteral("windows crypt init error");
      break;

    case -104: // CRYPTGEN_E :
      message = StringFromLiteral("windows crypt generation error");
      break;

    case -105: // RAN_BLOCK_E :
      message = StringFromLiteral("random device read would block error");
      break;

    case -106: // BAD_MUTEX_E :
      message = StringFromLiteral("Bad mutex, operation failed");
      break;

    case -107: // WC_TIMEOUT_E:
      message = StringFromLiteral("Timeout error");
      break;

    case -108: // WC_PENDING_E:
      message = StringFromLiteral("wolfCrypt Operation Pending (would block / eagain) error");
      break;

    case -109: // WC_NO_PENDING_E:
      message = StringFromLiteral("wolfCrypt operation not pending error");
      break;

    case -110: // MP_INIT_E :
      message = StringFromLiteral("mp_init error state");
      break;

    case -111: // MP_READ_E :
      message = StringFromLiteral("mp_read error state");
      break;

    case -112: // MP_EXPTMOD_E :
      message = StringFromLiteral("mp_exptmod error state");
      break;

    case -113: // MP_TO_E :
      message = StringFromLiteral("mp_to_xxx error state, can't convert");
      break;

    case -114: // MP_SUB_E :
      message = StringFromLiteral("mp_sub error state, can't subtract");
      break;

    case -115: // MP_ADD_E :
      message = StringFromLiteral("mp_add error state, can't add");
      break;

    case -116: // MP_MUL_E :
      message = StringFromLiteral("mp_mul error state, can't multiply");
      break;

    case -117: // MP_MULMOD_E :
      message = StringFromLiteral("mp_mulmod error state, can't multiply mod");
      break;

    case -118: // MP_MOD_E :
      message = StringFromLiteral("mp_mod error state, can't mod");
      break;

    case -119: // MP_INVMOD_E :
      message = StringFromLiteral("mp_invmod error state, can't inv mod");
      break;

    case -120: // MP_CMP_E :
      message = StringFromLiteral("mp_cmp error state");
      break;

    case -121: // MP_ZERO_E :
      message = StringFromLiteral("mp zero result, not expected");
      break;

    case -125: // MEMORY_E :
      message = StringFromLiteral("out of memory error");
      break;

    case -126: // VAR_STATE_CHANGE_E :
      message = StringFromLiteral("Variable state modified by different thread");
      break;

    case -130: // RSA_WRONG_TYPE_E :
      message = StringFromLiteral("RSA wrong block type for RSA function");
      break;

    case -131: // RSA_BUFFER_E :
      message = StringFromLiteral("RSA buffer error, output too small or input too big");
      break;

    case -132: // BUFFER_E :
      message = StringFromLiteral("Buffer error, output too small or input too big");
      break;

    case -133: // ALGO_ID_E :
      message = StringFromLiteral("Setting Cert AlgoID error");
      break;

    case -134: // PUBLIC_KEY_E :
      message = StringFromLiteral("Setting Cert Public Key error");
      break;

    case -135: // DATE_E :
      message = StringFromLiteral("Setting Cert Date validity error");
      break;

    case -136: // SUBJECT_E :
      message = StringFromLiteral("Setting Cert Subject name error");
      break;

    case -137: // ISSUER_E :
      message = StringFromLiteral("Setting Cert Issuer name error");
      break;

    case -138: // CA_TRUE_E :
      message = StringFromLiteral("Setting basic constraint CA true error");
      break;

    case -139: // EXTENSIONS_E :
      message = StringFromLiteral("Setting extensions error");
      break;

    case -140: // ASN_PARSE_E :
      message = StringFromLiteral("ASN parsing error, invalid input");
      break;

    case -141: // ASN_VERSION_E :
      message = StringFromLiteral("ASN version error, invalid number");
      break;

    case -142: // ASN_GETINT_E :
      message = StringFromLiteral("ASN get big int error, invalid data");
      break;

    case -143: // ASN_RSA_KEY_E :
      message = StringFromLiteral("ASN key init error, invalid input");
      break;

    case -144: // ASN_OBJECT_ID_E :
      message = StringFromLiteral("ASN object id error, invalid id");
      break;

    case -145: // ASN_TAG_NULL_E :
      message = StringFromLiteral("ASN tag error, not null");
      break;

    case -146: // ASN_EXPECT_0_E :
      message = StringFromLiteral("ASN expect error, not zero");
      break;

    case -147: // ASN_BITSTR_E :
      message = StringFromLiteral("ASN bit string error, wrong id");
      break;

    case -148: // ASN_UNKNOWN_OID_E :
      message = StringFromLiteral("ASN oid error, unknown sum id");
      break;

    case -149: // ASN_DATE_SZ_E :
      message = StringFromLiteral("ASN date error, bad size");
      break;

    case -150: // ASN_BEFORE_DATE_E :
      message = StringFromLiteral("ASN date error, current date before");
      break;

    case -151: // ASN_AFTER_DATE_E :
      message = StringFromLiteral("ASN date error, current date after");
      break;

    case -152: // ASN_SIG_OID_E :
      message = StringFromLiteral("ASN signature error, mismatched oid");
      break;

    case -153: // ASN_TIME_E :
      message = StringFromLiteral("ASN time error, unknown time type");
      break;

    case -154: // ASN_INPUT_E :
      message = StringFromLiteral("ASN input error, not enough data");
      break;

    case -155: // ASN_SIG_CONFIRM_E :
      message = StringFromLiteral("ASN sig error, confirm failure");
      break;

    case -156: // ASN_SIG_HASH_E :
      message = StringFromLiteral("ASN sig error, unsupported hash type");
      break;

    case -157: // ASN_SIG_KEY_E :
      message = StringFromLiteral("ASN sig error, unsupported key type");
      break;

    case -158: // ASN_DH_KEY_E :
      message = StringFromLiteral("ASN key init error, invalid input");
      break;

    case -160: // ASN_CRIT_EXT_E:
      message = StringFromLiteral("X.509 Critical extension ignored or invalid");
      break;

    case -161: // ASN_ALT_NAME_E:
      message = StringFromLiteral("ASN alternate name error");
      break;

    case -170: // ECC_BAD_ARG_E :
      message = StringFromLiteral("ECC input argument wrong type, invalid input");
      break;

    case -171: // ASN_ECC_KEY_E :
      message = StringFromLiteral("ECC ASN1 bad key data, invalid input");
      break;

    case -172: // ECC_CURVE_OID_E :
      message = StringFromLiteral("ECC curve sum OID unsupported, invalid input");
      break;

    case -173: // BAD_FUNC_ARG :
      message = StringFromLiteral("Bad function argument");
      break;

    case -174: // NOT_COMPILED_IN :
      message = StringFromLiteral("Feature not compiled in");
      break;

    case -175: // UNICODE_SIZE_E :
      message = StringFromLiteral("Unicode password too big");
      break;

    case -176: // NO_PASSWORD :
      message = StringFromLiteral("No password provided by user");
      break;

    case -177: // ALT_NAME_E :
      message = StringFromLiteral("Alt Name problem, too big");
      break;

    case -180: // AES_GCM_AUTH_E:
      message = StringFromLiteral("AES-GCM Authentication check fail");
      break;

    case -181: // AES_CCM_AUTH_E:
      message = StringFromLiteral("AES-CCM Authentication check fail");
      break;

    case -289: // AES_SIV_AUTH_E:
      message = StringFromLiteral("AES-SIV authentication failure");
      break;

    case -182: // ASYNC_INIT_E:
      message = StringFromLiteral("Async Init error");
      break;

    case -183: // COMPRESS_INIT_E:
      message = StringFromLiteral("Compress Init error");
      break;

    case -184: // COMPRESS_E:
      message = StringFromLiteral("Compress error");
      break;

    case -185: // DECOMPRESS_INIT_E:
      message = StringFromLiteral("DeCompress Init error");
      break;

    case -186: // DECOMPRESS_E:
      message = StringFromLiteral("DeCompress error");
      break;

    case -187: // BAD_ALIGN_E:
      message = StringFromLiteral("Bad alignment error, no alloc help");
      break;

    case -188: // ASN_NO_SIGNER_E :
      message = StringFromLiteral("ASN no signer error to confirm failure");
      break;

    case -189: // ASN_CRL_CONFIRM_E :
      message = StringFromLiteral("ASN CRL sig error, confirm failure");
      break;

    case -190: // ASN_CRL_NO_SIGNER_E :
      message = StringFromLiteral("ASN CRL no signer error to confirm failure");
      break;

    case -179: // CRL_CERT_DATE_ERR:
      message = StringFromLiteral("CRL date error");
      break;

    case -191: // ASN_OCSP_CONFIRM_E :
      message = StringFromLiteral("ASN OCSP sig error, confirm failure");
      break;

    case -162: // ASN_NO_PEM_HEADER:
      message = StringFromLiteral("ASN no PEM Header Error");
      break;

    case -192: // BAD_STATE_E:
      message = StringFromLiteral("Bad state operation");
      break;

    case -193: // BAD_PADDING_E:
      message = StringFromLiteral("Bad padding, message wrong length");
      break;

    case -194: // REQ_ATTRIBUTE_E:
      message = StringFromLiteral("Setting cert request attributes error");
      break;

    case -195: // PKCS7_OID_E:
      message = StringFromLiteral("PKCS#7 error: mismatched OID value");
      break;

    case -196: // PKCS7_RECIP_E:
      message = StringFromLiteral("PKCS#7 error: no matching recipient found");
      break;

    case -270: // WC_PKCS7_WANT_READ_E:
      message = StringFromLiteral("PKCS#7 operations wants more input, call again");
      break;

    case -197: // FIPS_NOT_ALLOWED_E:
      message = StringFromLiteral("FIPS mode not allowed error");
      break;

    case -198: // ASN_NAME_INVALID_E:
      message = StringFromLiteral("Name Constraint error");
      break;

    case -199: // RNG_FAILURE_E:
      message = StringFromLiteral("Random Number Generator failed");
      break;

    case -200: // HMAC_MIN_KEYLEN_E:
      message = StringFromLiteral("FIPS Mode HMAC Minimum Key Length error");
      break;

    case -201: // RSA_PAD_E:
      message = StringFromLiteral("Rsa Padding error");
      break;

    case -202: // LENGTH_ONLY_E:
      message = StringFromLiteral("Output length only set, not for other use error");
      break;

    case -203: // IN_CORE_FIPS_E:
      message = StringFromLiteral("In Core Integrity check FIPS error");
      break;

    case -204: // AES_KAT_FIPS_E:
      message = StringFromLiteral("AES Known Answer Test check FIPS error");
      break;

    case -205: // DES3_KAT_FIPS_E:
      message = StringFromLiteral("DES3 Known Answer Test check FIPS error");
      break;

    case -206: // HMAC_KAT_FIPS_E:
      message = StringFromLiteral("HMAC Known Answer Test check FIPS error");
      break;

    case -207: // RSA_KAT_FIPS_E:
      message = StringFromLiteral("RSA Known Answer Test check FIPS error");
      break;

    case -208: // DRBG_KAT_FIPS_E:
      message = StringFromLiteral("DRBG Known Answer Test check FIPS error");
      break;

    case -209: // DRBG_CONT_FIPS_E:
      message = StringFromLiteral("DRBG Continuous Test FIPS error");
      break;

    case -210: // AESGCM_KAT_FIPS_E:
      message = StringFromLiteral("AESGCM Known Answer Test check FIPS error");
      break;

    case -211: // THREAD_STORE_KEY_E:
      message = StringFromLiteral("Thread Storage Key Create error");
      break;

    case -212: // THREAD_STORE_SET_E:
      message = StringFromLiteral("Thread Storage Set error");
      break;

    case -213: // MAC_CMP_FAILED_E:
      message = StringFromLiteral("MAC comparison failed");
      break;

    case -214: // IS_POINT_E:
      message = StringFromLiteral("ECC is point on curve failed");
      break;

    case -215: // ECC_INF_E:
      message = StringFromLiteral("ECC point at infinity error");
      break;

    case -217: // ECC_OUT_OF_RANGE_E:
      message = StringFromLiteral("ECC Qx or Qy out of range error");
      break;

    case -216: // ECC_PRIV_KEY_E:
      message = StringFromLiteral("ECC private key is not valid error");
      break;

    case -218: // SRP_CALL_ORDER_E:
      message = StringFromLiteral("SRP function called in the wrong order error");
      break;

    case -219: // SRP_VERIFY_E:
      message = StringFromLiteral("SRP proof verification error");
      break;

    case -220: // SRP_BAD_KEY_E:
      message = StringFromLiteral("SRP bad key values error");
      break;

    case -221: // ASN_NO_SKID:
      message = StringFromLiteral("ASN no Subject Key Identifier found error");
      break;

    case -222: // ASN_NO_AKID:
      message = StringFromLiteral("ASN no Authority Key Identifier found error");
      break;

    case -223: // ASN_NO_KEYUSAGE:
      message = StringFromLiteral("ASN no Key Usage found error");
      break;

    case -224: // SKID_E:
      message = StringFromLiteral("Setting Subject Key Identifier error");
      break;

    case -225: // AKID_E:
      message = StringFromLiteral("Setting Authority Key Identifier error");
      break;

    case -226: // KEYUSAGE_E:
      message = StringFromLiteral("Key Usage value error");
      break;

    case -247: // EXTKEYUSAGE_E:
      message = StringFromLiteral("Extended Key Usage value error");
      break;

    case -227: // CERTPOLICIES_E:
      message = StringFromLiteral("Setting Certificate Policies error");
      break;

    case -228: // WC_INIT_E:
      message = StringFromLiteral("wolfCrypt Initialize Failure error");
      break;

    case -229: // SIG_VERIFY_E:
      message = StringFromLiteral("Signature verify error");
      break;

    case -230: // BAD_COND_E:
      message = StringFromLiteral("Bad condition variable operation error");
      break;

    case -231: // SIG_TYPE_E:
      message = StringFromLiteral("Signature type not enabled/available");
      break;

    case -232: // HASH_TYPE_E:
      message = StringFromLiteral("Hash type not enabled/available");
      break;

    case -234: // WC_KEY_SIZE_E:
      message = StringFromLiteral("Key size error, either too small or large");
      break;

    case -235: // ASN_COUNTRY_SIZE_E:
      message = StringFromLiteral("Country code size error, either too small or large");
      break;

    case -236: // MISSING_RNG_E:
      message = StringFromLiteral("RNG required but not provided");
      break;

    case -237: // ASN_PATHLEN_SIZE_E:
      message = StringFromLiteral("ASN CA path length value too large error");
      break;

    case -238: // ASN_PATHLEN_INV_E:
      message = StringFromLiteral("ASN CA path length larger than signer error");
      break;

    case -239: // BAD_KEYWRAP_ALG_E:
      message = StringFromLiteral("Unsupported key wrap algorithm error");
      break;

    case -240: // BAD_KEYWRAP_IV_E:
      message = StringFromLiteral("Decrypted AES key wrap IV does not match expected");
      break;

    case -241: // WC_CLEANUP_E:
      message = StringFromLiteral("wolfcrypt cleanup failed");
      break;

    case -242: // ECC_CDH_KAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS ECC CDH Known Answer Test Failure");
      break;

    case -243: // DH_CHECK_PUB_E:
      message = StringFromLiteral("DH Check Public Key failure");
      break;

    case -244: // BAD_PATH_ERROR:
      message = StringFromLiteral("Bad path for opendir error");
      break;

    case -245: // ASYNC_OP_E:
      message = StringFromLiteral("Async operation error");
      break;

    case -178: // BAD_OCSP_RESPONDER:
      message = StringFromLiteral("Invalid OCSP Responder, missing specific key usage extensions");
      break;

    case -246: // ECC_PRIVATEONLY_E:
      message = StringFromLiteral("Invalid use of private only ECC key");
      break;

    case -248: // WC_HW_E:
      message = StringFromLiteral("Error with hardware crypto use");
      break;

    case -249: // WC_HW_WAIT_E:
      message = StringFromLiteral("Hardware waiting on resource");
      break;

    case -250: // PSS_SALTLEN_E:
      message = StringFromLiteral("PSS - Length of salt is too big for hash algorithm");
      break;

    case -251: // PRIME_GEN_E:
      message = StringFromLiteral("Unable to find a prime for RSA key");
      break;

    case -252: // BER_INDEF_E:
      message = StringFromLiteral("Unable to decode an indefinite length encoded message");
      break;

    case -253: // RSA_OUT_OF_RANGE_E:
      message = StringFromLiteral("Ciphertext to decrypt is out of range");
      break;

    case -254: // RSAPSS_PAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS RSA-PSS Pairwise Agreement Test Failure");
      break;

    case -255: // ECDSA_PAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS ECDSA Pairwise Agreement Test Failure");
      break;

    case -256: // DH_KAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS DH Known Answer Test Failure");
      break;

    case -257: // AESCCM_KAT_FIPS_E:
      message = StringFromLiteral("AESCCM Known Answer Test check FIPS error");
      break;

    case -258: // SHA3_KAT_FIPS_E:
      message = StringFromLiteral("SHA-3 Known Answer Test check FIPS error");
      break;

    case -259: // ECDHE_KAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS ECDHE Known Answer Test Failure");
      break;

    case -260: // AES_GCM_OVERFLOW_E:
      message = StringFromLiteral("AES-GCM invocation counter overflow");
      break;

    case -261: // AES_CCM_OVERFLOW_E:
      message = StringFromLiteral("AES-CCM invocation counter overflow");
      break;

    case -262: // RSA_KEY_PAIR_E:
      message = StringFromLiteral("RSA Key Pair-Wise Consistency check fail");
      break;

    case -263: // DH_CHECK_PRIV_E:
      message = StringFromLiteral("DH Check Private Key failure");
      break;

    case -264: // WC_AFALG_SOCK_E:
      message = StringFromLiteral("AF_ALG socket error");
      break;

    case -265: // WC_DEVCRYPTO_E:
      message = StringFromLiteral("Error with /dev/crypto");
      break;

    case -266: // ZLIB_INIT_ERROR:
      message = StringFromLiteral("zlib init error");
      break;

    case -267: // ZLIB_COMPRESS_ERROR:
      message = StringFromLiteral("zlib compress error");
      break;

    case -268: // ZLIB_DECOMPRESS_ERROR:
      message = StringFromLiteral("zlib decompress error");
      break;

    case -269: // PKCS7_NO_SIGNER_E:
      message = StringFromLiteral("No signer in PKCS#7 signed data");
      break;

    case -271: // CRYPTOCB_UNAVAILABLE:
      message = StringFromLiteral("Crypto callback unavailable");
      break;

    case -272: // PKCS7_SIGNEEDS_CHECK:
      message = StringFromLiteral("Signature found but no certificate to verify");
      break;

    case -273: // PSS_SALTLEN_RECOVER_E:
      message = StringFromLiteral("PSS - Salt length unable to be recovered");
      break;

    case -274: // CHACHA_POLY_OVERFLOW:
      message = StringFromLiteral("wolfcrypt - ChaCha20_Poly1305 limit overflow 4GB");
      break;

    case -275: // ASN_SELF_SIGNED_E:
      message = StringFromLiteral("ASN self-signed certificate error");
      break;

    case -276: // SAKKE_VERIFY_FAIL_E:
      message = StringFromLiteral("SAKKE derivation verification error");
      break;

    case -277: // MISSING_IV:
      message = StringFromLiteral("Required IV not set");
      break;

    case -278: // MISSING_KEY:
      message = StringFromLiteral("Required key not set");
      break;

    case -279: // BAD_LENGTH_E:
      message = StringFromLiteral("Value of length parameter is invalid.");
      break;

    case -280: // ECDSA_KAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS ECDSA Known Answer Test Failure");
      break;

    case -281: // RSA_PAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS RSA Pairwise Agreement Test Failure");
      break;

    case -282: // KDF_TLS12_KAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS TLSv1.2 KDF Known Answer Test Failure");
      break;

    case -283: // KDF_TLS13_KAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS TLSv1.3 KDF Known Answer Test Failure");
      break;

    case -284: // KDF_SSH_KAT_FIPS_E:
      message = StringFromLiteral("wolfcrypt FIPS SSH KDF Known Answer Test Failure");
      break;

    case -285: // DHE_PCT_E:
      message = StringFromLiteral("wolfcrypt DHE Pairwise Consistency Test Failure");
      break;

    case -286: // ECC_PCT_E:
      message = StringFromLiteral("wolfcrypt ECDHE Pairwise Consistency Test Failure");
      break;

    case -287: // FIPS_PRIVATE_KEY_LOCKED_E:
      message = StringFromLiteral("Cannot export private key, locked");
      break;

    case -288: // PROTOCOLCB_UNAVAILABLE:
      message = StringFromLiteral("Protocol callback unavailable");
      break;

    case -290: // NO_VALID_DEVID:
      message = StringFromLiteral("No valid device ID set");
      break;

    case -291: // IO_FAILED_E:
      message = StringFromLiteral("Input/output failure");
      break;

    case -292: // SYSLIB_FAILED_E:
      message = StringFromLiteral("System/library call failed");
      break;

    case -293: // USE_HW_PSK:
      message = StringFromLiteral("Callback indicates that HW has PSK");
      break;

    case -294: // ENTROPY_RT_E:
      message = StringFromLiteral("Entropy Repetition Test failed");
      break;

    case -295: // ENTROPY_APT_E:
      message = StringFromLiteral("Entropy Adaptive Proportion Test failed");
      break;

    case -296: // ASN_DEPTH_E:
      message = StringFromLiteral("Invalid ASN.1 - depth check");
      break;

    case -297: // ASN_LEN_E:
      message = StringFromLiteral("ASN.1 length invalid");
      break;

    case -298: // SM4_GCM_AUTH_E:
      message = StringFromLiteral("SM4-GCM Authentication check fail");
      break;

    case -299: // SM4_CCM_AUTH_E:
      message = StringFromLiteral("SM4-CCM Authentication check fail");
      break;

    case -127: // FIPS_DEGRADED_E:
      message = StringFromLiteral("FIPS module in DEGRADED mode");
      break;

    case -122: // AES_EAX_AUTH_E:
      message = StringFromLiteral("AES-EAX Authentication check fail");
      break;

    case -123: // KEY_EXHAUSTED_E:
      message = StringFromLiteral("Key no longer usable for operation");
      break;

    case -233: // FIPS_INVALID_VER_E:
      message = StringFromLiteral("Invalid FIPS version defined, check length");
      break;

    case -129: // FIPS_DATA_SZ_E:
      message = StringFromLiteral("FIPS Module Data too large adjust MAX_FIPS_DATA_SZ");
      break;

    case -128: // FIPS_CODE_SZ_E:
      message = StringFromLiteral("FIPS Module Code too large adjust MAX_FIPS_CODE_SZ");
      break;

    case -159: // KDF_SRTP_KAT_FIPS_E:
      message = StringFromLiteral("wolfCrypt FIPS SRTP-KDF Known Answer Test Failure");
      break;

    case -163: // ED25519_KAT_FIPS_E:
      message = StringFromLiteral("wolfCrypt FIPS Ed25519 Known Answer Test Failure");
      break;

    case -164: // ED448_KAT_FIPS_E:
      message = StringFromLiteral("wolfCrypt FIPS Ed448 Known Answer Test Failure");
      break;

    case -165: // PBKDF2_KAT_FIPS_E:
      message = StringFromLiteral("wolfCrypt FIPS PBKDF2 Known Answer Test Failure");
      break;

    case -166: // WC_KEY_MISMATCH_E:
      message = StringFromLiteral("key values mismatch");
      break;

    case -1000: // DEADLOCK_AVERTED_E:
      message = StringFromLiteral("Deadlock averted -- retry the call");
      break;

    case -1001: // ASCON_AUTH_E:
      message = StringFromLiteral("ASCON Authentication check fail");
      break;
    }
  } else if (errnum == 0) {
    message = StringFromLiteral("unknown error number");
  } else {
    switch (errnum) {
    case -500: // UNSUPPORTED_SUITE :
      message = StringFromLiteral("unsupported cipher suite");
      break;

    case -301: // INPUT_CASE_ERROR :
      message = StringFromLiteral("input state error");
      break;

    case -302: // PREFIX_ERROR :
      message = StringFromLiteral("bad index to key rounds");
      break;

    case -303: // MEMORY_ERROR :
      message = StringFromLiteral("out of memory");
      break;

    case -304: // VERIFY_FINISHED_ERROR :
      message = StringFromLiteral("verify problem on finished");
      break;

    case -305: // VERIFY_MAC_ERROR :
      message = StringFromLiteral("verify mac problem");
      break;

    case -306: // PARSE_ERROR :
      message = StringFromLiteral("parse error on header");
      break;

    case -344: // SIDE_ERROR :
      message = StringFromLiteral("wrong client/server type");
      break;

    case -345: // NO_PEER_CERT : /* OpenSSL compatibility expects this exact text */
      message = StringFromLiteral("peer did not return a certificate");
      break;

    case -307: // UNKNOWN_HANDSHAKE_TYPE :
      message = StringFromLiteral("weird handshake type");
      break;

    case -308: // SOCKET_ERROR_E :
      message = StringFromLiteral("error state on socket");
      break;

    case -309: // SOCKET_NODATA :
      message = StringFromLiteral("expected data, not there");
      break;

    case -310: // INCOMPLETE_DATA :
      message = StringFromLiteral("don't have enough data to complete task");
      break;

    case -311: // UNKNOWN_RECORD_TYPE :
      message = StringFromLiteral("unknown type in record hdr");
      break;

    case -312: // DECRYPT_ERROR :
      message = StringFromLiteral("error during decryption");
      break;

    case -313: // FATAL_ERROR :
      message = StringFromLiteral("received alert fatal error");
      break;

    case -314: // ENCRYPT_ERROR :
      message = StringFromLiteral("error during encryption");
      break;

    case -315: // FREAD_ERROR :
      message = StringFromLiteral("fread problem");
      break;

    case -316: // NO_PEER_KEY :
      message = StringFromLiteral("need peer's key");
      break;

    case -317: // NO_PRIVATE_KEY :
      message = StringFromLiteral("need the private key");
      break;

    case -319: // NO_DH_PARAMS :
      message = StringFromLiteral("server missing DH params");
      break;

    case -318: // RSA_PRIVATE_ERROR :
      message = StringFromLiteral("error during rsa priv op");
      break;

    case -501: // MATCH_SUITE_ERROR :
      message = StringFromLiteral("can't match cipher suite");
      break;

    case -502: // COMPRESSION_ERROR :
      message = StringFromLiteral("compression mismatch error");
      break;

    case -320: // BUILD_MSG_ERROR :
      message = StringFromLiteral("build message failure");
      break;

    case -321: // BAD_HELLO :
      message = StringFromLiteral("client hello malformed");
      break;

    case -322: // DOMAIN_NAME_MISMATCH :
      message = StringFromLiteral("peer subject name mismatch");
      break;

    case -325: // IPADDR_MISMATCH :
      message = StringFromLiteral("peer ip address mismatch");
      break;

    case -323: // WANT_READ :
    case -2:   // -WOLFSSL_ERROR_WANT_READ :
      message = StringFromLiteral("non-blocking socket wants data to be read");
      break;

    case -324: // NOT_READY_ERROR :
      message = StringFromLiteral("handshake layer not ready yet, complete first");
      break;

    case -326: // VERSION_ERROR :
      message = StringFromLiteral("record layer version error");
      break;

    case -327: // WANT_WRITE :
    case -3:   // -WOLFSSL_ERROR_WANT_WRITE :
      message = StringFromLiteral("non-blocking socket write buffer full");
      break;

    case -7: // -WOLFSSL_ERROR_WANT_CONNECT:
    case -8: // -WOLFSSL_ERROR_WANT_ACCEPT:
      message = StringFromLiteral("The underlying BIO was not yet connected");
      break;

    case -5: // -WOLFSSL_ERROR_SYSCALL:
      message = StringFromLiteral("fatal I/O error in TLS layer");
      break;

    case -4: // -WOLFSSL_ERROR_WANT_X509_LOOKUP:
      message = StringFromLiteral("application client cert callback asked to be called again");
      break;

    case -328: // BUFFER_ERROR :
      message = StringFromLiteral("malformed buffer input error");
      break;

    case -329: // VERIFY_CERT_ERROR :
      message = StringFromLiteral("verify problem on certificate");
      break;

    case -330: // VERIFY_SIGN_ERROR :
      message = StringFromLiteral("verify problem based on signature");
      break;

    case -331: // CLIENT_ID_ERROR :
      message = StringFromLiteral("psk client identity error");
      break;

    case -332: // SERVER_HINT_ERROR:
      message = StringFromLiteral("psk server hint error");
      break;

    case -333: // PSK_KEY_ERROR:
      message = StringFromLiteral("psk key callback error");
      break;

    case -337: // GETTIME_ERROR:
      message = StringFromLiteral("gettimeofday() error");
      break;

    case -338: // GETITIMER_ERROR:
      message = StringFromLiteral("getitimer() error");
      break;

    case -339: // SIGACT_ERROR:
      message = StringFromLiteral("sigaction() error");
      break;

    case -340: // SETITIMER_ERROR:
      message = StringFromLiteral("setitimer() error");
      break;

    case -341: // LENGTH_ERROR:
      message = StringFromLiteral("record layer length error");
      break;

    case -342: // PEER_KEY_ERROR:
      message = StringFromLiteral("can't decode peer key");
      break;
    case -343: // ZERO_RETURN:
    case -6:   // -WOLFSSL_ERROR_ZERO_RETURN:
      message = StringFromLiteral("peer sent close notify alert");
      break;

    case -350: // ECC_CURVETYPE_ERROR:
      message = StringFromLiteral("Bad ECC Curve Type or unsupported");
      break;

    case -351: // ECC_CURVE_ERROR:
      message = StringFromLiteral("Bad ECC Curve or unsupported");
      break;

    case -352: // ECC_PEERKEY_ERROR:
      message = StringFromLiteral("Bad ECC Peer Key");
      break;

    case -353: // ECC_MAKEKEY_ERROR:
      message = StringFromLiteral("ECC Make Key failure");
      break;

    case -354: // ECC_EXPORT_ERROR:
      message = StringFromLiteral("ECC Export Key failure");
      break;

    case -355: // ECC_SHARED_ERROR:
      message = StringFromLiteral("ECC DHE shared failure");
      break;

    case -357: // NOT_CA_ERROR:
      message = StringFromLiteral("Not a CA by basic constraint error");
      break;

    case -359: // BAD_CERT_MANAGER_ERROR:
      message = StringFromLiteral("Bad Cert Manager error");
      break;

    case -360: // OCSP_CERT_REVOKED:
      message = StringFromLiteral("OCSP Cert revoked");
      break;

    case -361: // CRL_CERT_REVOKED:
      message = StringFromLiteral("CRL Cert revoked");
      break;

    case -362: // CRL_MISSING:
      message = StringFromLiteral("CRL missing, not loaded");
      break;

    case -516: // CRYPTO_POLICY_FORBIDDEN:
      message = StringFromLiteral("Operation forbidden by system crypto-policy");
      break;

    case -363: // MONITOR_SETUP_E:
      message = StringFromLiteral("CRL monitor setup error");
      break;

    case -364: // THREAD_CREATE_E:
      message = StringFromLiteral("Thread creation problem");
      break;

    case -365: // OCSP_NEED_URL:
      message = StringFromLiteral("OCSP need URL");
      break;

    case -366: // OCSP_CERT_UNKNOWN:
      message = StringFromLiteral("OCSP Cert unknown");
      break;

    case -367: // OCSP_LOOKUP_FAIL:
      message = StringFromLiteral("OCSP Responder lookup fail");
      break;

    case -368: // MAX_CHAIN_ERROR:
      message = StringFromLiteral("Maximum Chain Depth Exceeded");
      break;

    case -372: // MAX_CERT_EXTENSIONS_ERR:
      message = StringFromLiteral("Maximum Cert Extension Exceeded");
      break;

    case -369: // COOKIE_ERROR:
      message = StringFromLiteral("DTLS Cookie Error");
      break;

    case -370: // SEQUENCE_ERROR:
      message = StringFromLiteral("DTLS Sequence Error");
      break;

    case -371: // SUITES_ERROR:
      message = StringFromLiteral("Suites Pointer Error");
      break;

    case -373: // OUT_OF_ORDER_E:
      message = StringFromLiteral("Out of order message, fatal");
      break;

    case -374: // BAD_KEA_TYPE_E:
      message = StringFromLiteral("Bad KEA type found");
      break;

    case -375: // SANITY_CIPHER_E:
      message = StringFromLiteral("Sanity check on ciphertext failed");
      break;

    case -376: // RECV_OVERFLOW_E:
      message = StringFromLiteral("Receive callback returned more than requested");
      break;

    case -377: // GEN_COOKIE_E:
      message = StringFromLiteral("Generate Cookie Error");
      break;

    case -378: // NO_PEER_VERIFY:
      message = StringFromLiteral("Need peer certificate verify Error");
      break;

    case -379: // FWRITE_ERROR:
      message = StringFromLiteral("fwrite Error");
      break;

    case -380: // CACHE_MATCH_ERROR:
      message = StringFromLiteral("Cache restore header match Error");
      break;

    case -381: // UNKNOWN_SNI_HOST_NAME_E:
      message = StringFromLiteral("Unrecognized host name Error");
      break;

    case -382: // UNKNOWN_MAX_FRAG_LEN_E:
      message = StringFromLiteral("Unrecognized max frag len Error");
      break;

    case -383: // KEYUSE_SIGNATURE_E:
      message = StringFromLiteral("Key Use digitalSignature not set Error");
      break;

    case -385: // KEYUSE_ENCIPHER_E:
      message = StringFromLiteral("Key Use keyEncipherment not set Error");
      break;

    case -386: // EXTKEYUSE_AUTH_E:
      message = StringFromLiteral("Ext Key Use server/client auth not set Error");
      break;

    case -387: // SEND_OOB_READ_E:
      message = StringFromLiteral("Send Callback Out of Bounds Read Error");
      break;

    case -388: // SECURE_RENEGOTIATION_E:
      message = StringFromLiteral("Invalid Renegotiation Error");
      break;

    case -389: // SESSION_TICKET_LEN_E:
      message = StringFromLiteral("Session Ticket Too Long Error");
      break;

    case -390: // SESSION_TICKET_EXPECT_E:
      message = StringFromLiteral("Session Ticket Error");
      break;

    case -391: // SCR_DIFFERENT_CERT_E:
      message = StringFromLiteral("SCR Different cert error");
      break;

    case -392: // SESSION_SECRET_CB_E:
      message = StringFromLiteral("Session Secret Callback Error");
      break;

    case -393: // NO_CHANGE_CIPHER_E:
      message = StringFromLiteral("Finished received from peer before Change Cipher Error");
      break;

    case -394: // SANITY_MSG_E:
      message = StringFromLiteral("Sanity Check on message order Error");
      break;

    case -395: // DUPLICATE_MSG_E:
      message = StringFromLiteral("Duplicate HandShake message Error");
      break;

    case -396: // SNI_UNSUPPORTED:
      message = StringFromLiteral("Protocol version does not support SNI Error");
      break;

    case -397: // SOCKET_PEER_CLOSED_E:
      message = StringFromLiteral("Peer closed underlying transport Error");
      break;

    case -398: // BAD_TICKET_KEY_CB_SZ:
      message = StringFromLiteral("Bad user session ticket key callback Size Error");
      break;

    case -399: // BAD_TICKET_MSG_SZ:
      message = StringFromLiteral("Bad session ticket message Size Error");
      break;

    case -400: // BAD_TICKET_ENCRYPT:
      message = StringFromLiteral("Bad user ticket callback encrypt Error");
      break;

    case -401: // DH_KEY_SIZE_E:
      message = StringFromLiteral("DH key too small Error");
      break;

    case -402: // SNI_ABSENT_ERROR:
      message = StringFromLiteral("No Server Name Indication extension Error");
      break;

    case -403: // RSA_SIGN_FAULT:
      message = StringFromLiteral("RSA Signature Fault Error");
      break;

    case -404: // HANDSHAKE_SIZE_ERROR:
      message = StringFromLiteral("Handshake message too large Error");
      break;

    case -405: // UNKNOWN_ALPN_PROTOCOL_NAME_E:
      message = StringFromLiteral("Unrecognized protocol name Error");
      break;

    case -406: // BAD_CERTIFICATE_STATUS_ERROR:
      message = StringFromLiteral("Bad Certificate Status Message Error");
      break;

    case -407: // OCSP_INVALID_STATUS:
      message = StringFromLiteral("Invalid OCSP Status Error");
      break;

    case -408: // OCSP_WANT_READ:
      message = StringFromLiteral("OCSP nonblock wants read");
      break;

    case -409: // RSA_KEY_SIZE_E:
      message = StringFromLiteral("RSA key too small");
      break;

    case -410: // ECC_KEY_SIZE_E:
      message = StringFromLiteral("ECC key too small");
      break;

    case -411: // DTLS_EXPORT_VER_E:
      message = StringFromLiteral("Version needs updated after code change or version mismatch");
      break;

    case -412: // INPUT_SIZE_E:
      message = StringFromLiteral("Input size too large Error");
      break;

    case -413: // CTX_INIT_MUTEX_E:
      message = StringFromLiteral("Initialize ctx mutex error");
      break;

    case -414: // EXT_MASTER_SECRET_NEEDED_E:
      message = StringFromLiteral("Extended Master Secret must be enabled to resume EMS session");
      break;

    case -415: // DTLS_POOL_SZ_E:
      message = StringFromLiteral("Maximum DTLS pool size exceeded");
      break;

    case -416: // DECODE_E:
      message = StringFromLiteral("Decode handshake message error");
      break;

    case -418: // WRITE_DUP_READ_E:
      message = StringFromLiteral("Write dup write side can't read error");
      break;

    case -419: // WRITE_DUP_WRITE_E:
      message = StringFromLiteral("Write dup read side can't write error");
      break;

    case -420: // INVALID_CERT_CTX_E:
      message = StringFromLiteral("Certificate context does not match request or not empty");
      break;

    case -421: // BAD_KEY_SHARE_DATA:
      message = StringFromLiteral("The Key Share data contains group that wasn't in Client Hello");
      break;

    case -422: // MISSING_HANDSHAKE_DATA:
      message = StringFromLiteral("The handshake message is missing required data");
      break;

    case -423: // BAD_BINDER: /* OpenSSL compatibility expects this exact text */
      message = StringFromLiteral("binder does not verify");
      break;

    case -424: // EXT_NOT_ALLOWED:
      message = StringFromLiteral("Extension type not allowed in handshake message type");
      break;

    case -425: // INVALID_PARAMETER:
      message = StringFromLiteral("The security parameter is invalid");
      break;

    case -429: // UNSUPPORTED_EXTENSION:
      message = StringFromLiteral("TLS Extension not requested by the client");
      break;

    case -430: // PRF_MISSING:
      message = StringFromLiteral("Pseudo-random function is not enabled");
      break;

    case -503: // KEY_SHARE_ERROR:
      message = StringFromLiteral("Key share extension did not contain a valid named group");
      break;

    case -504: // POST_HAND_AUTH_ERROR:
      message = StringFromLiteral("Client will not do post handshake authentication");
      break;

    case -505: // HRR_COOKIE_ERROR:
      message = StringFromLiteral("Cookie does not match one sent in HelloRetryRequest");
      break;

    case -426: // MCAST_HIGHWATER_CB_E:
      message = StringFromLiteral("Multicast highwater callback returned error");
      break;

    case -427: // ALERT_COUNT_E:
      message = StringFromLiteral("Alert Count exceeded error");
      break;

    case -428: // EXT_MISSING:
      message = StringFromLiteral("Required TLS extension missing");
      break;

    case -431: // DTLS_RETX_OVER_TX:
      message = StringFromLiteral("DTLS interrupting flight transmit with retransmit");
      break;

    case -432: // DH_PARAMS_NOT_FFDHE_E:
      message = StringFromLiteral("Server DH parameters were not from the FFDHE set as required");
      break;

    case -433: // TCA_INVALID_ID_TYPE:
      message = StringFromLiteral("TLS Extension Trusted CA ID type invalid");
      break;

    case -434: // TCA_ABSENT_ERROR:
      message = StringFromLiteral("TLS Extension Trusted CA ID response absent");
      break;

    case -435: // TSIP_MAC_DIGSZ_E:
      message = StringFromLiteral("TSIP MAC size invalid, must be sized for SHA-1 or SHA-256");
      break;

    case -436: // CLIENT_CERT_CB_ERROR:
      message = StringFromLiteral("Error importing client cert or key from callback");
      break;

    case -437: // SSL_SHUTDOWN_ALREADY_DONE_E:
      message = StringFromLiteral("Shutdown has already occurred");
      break;

    case -438: // TLS13_SECRET_CB_E:
      message = StringFromLiteral("TLS1.3 Secret Callback Error");
      break;

    case -439: // DTLS_SIZE_ERROR:
      message = StringFromLiteral("DTLS trying to send too much in single datagram error");
      break;

    case -440: // NO_CERT_ERROR:
      message = StringFromLiteral("TLS1.3 No Certificate Set Error");
      break;

    case -441: // APP_DATA_READY:
      message = StringFromLiteral("Application data is available for reading");
      break;

    case -442: // TOO_MUCH_EARLY_DATA:
      message = StringFromLiteral("Too much early data");
      break;

    case -443: // SOCKET_FILTERED_E:
      message = StringFromLiteral("Session stopped by network filter");
      break;

    case -506: // UNSUPPORTED_CERTIFICATE:
      message = StringFromLiteral("Unsupported certificate type");
      break;

    case -417: // HTTP_TIMEOUT:
      message = StringFromLiteral("HTTP timeout for OCSP or CRL req");
      break;

    case -444: // HTTP_RECV_ERR:
      message = StringFromLiteral("HTTP Receive error");
      break;

    case -445: // HTTP_HEADER_ERR:
      message = StringFromLiteral("HTTP Header error");
      break;

    case -446: // HTTP_PROTO_ERR:
      message = StringFromLiteral("HTTP Protocol error");
      break;

    case -447: // HTTP_STATUS_ERR:
      message = StringFromLiteral("HTTP Status error");
      break;

    case -448: // HTTP_VERSION_ERR:
      message = StringFromLiteral("HTTP Version error");
      break;

    case -449: // HTTP_APPSTR_ERR:
      message = StringFromLiteral("HTTP Application string error");
      break;

    case -450: // UNSUPPORTED_PROTO_VERSION:
      message = StringFromLiteral("bad/unsupported protocol version");
      break;

    case -451: // FALCON_KEY_SIZE_E:
      message = StringFromLiteral("Wrong key size for Falcon.");
      break;

    case -453: // DILITHIUM_KEY_SIZE_E:
      message = StringFromLiteral("Wrong key size for Dilithium.");
      break;

    case -452: // QUIC_TP_MISSING_E:
      message = StringFromLiteral("QUIC transport parameter not set");
      break;

    case -456: // QUIC_WRONG_ENC_LEVEL:
      message = StringFromLiteral("QUIC data received at wrong encryption level");
      break;

    case -454: // DTLS_CID_ERROR:
      message = StringFromLiteral("DTLS ConnectionID mismatch or missing");
      break;

    case -455: // DTLS_TOO_MANY_FRAGMENTS_E:
      message = StringFromLiteral("Received too many fragmented messages from peer error");
      break;

    case -457: // DUPLICATE_TLS_EXT_E:
      message = StringFromLiteral("Duplicate TLS extension in message.");
      break;

    case -458: // WOLFSSL_ALPN_NOT_FOUND:
      message = StringFromLiteral("TLS extension not found");
      break;

    case -459: // WOLFSSL_BAD_CERTTYPE:
      message = StringFromLiteral("Certificate type not supported");
      break;

    case -460: // WOLFSSL_BAD_STAT:
      message = StringFromLiteral("bad status");
      break;

    case -461: // WOLFSSL_BAD_PATH:
      message = StringFromLiteral("No certificates found at designated path");
      break;

    case -462: // WOLFSSL_BAD_FILETYPE:
      message = StringFromLiteral("Data format not supported");
      break;

    case -463: // WOLFSSL_BAD_FILE:
      message = StringFromLiteral("Input/output error on file");
      break;

    case -464: // WOLFSSL_NOT_IMPLEMENTED:
      message = StringFromLiteral("Function not implemented");
      break;

    case -465: // WOLFSSL_UNKNOWN:
      message = StringFromLiteral("Unknown algorithm (EVP)");
      break;

    case -1: // WOLFSSL_FATAL_ERROR:
      message = StringFromLiteral("fatal error");
      break;

    case -507: // WOLFSSL_PEM_R_NO_START_LINE_E:
      message = StringFromLiteral("No more matching objects found (PEM)");
      break;

    case -508: // WOLFSSL_PEM_R_PROBLEMS_GETTING_PASSWORD_E:
      message = StringFromLiteral("Error getting password (PEM)");
      break;

    case -509: // WOLFSSL_PEM_R_BAD_PASSWORD_READ_E:
      message = StringFromLiteral("Bad password (PEM)");
      break;

    case -510: // WOLFSSL_PEM_R_BAD_DECRYPT_E :
      message = StringFromLiteral("Decryption failed (PEM)");
      break;

    case -511: // WOLFSSL_ASN1_R_HEADER_TOO_LONG_E:
      message = StringFromLiteral("ASN header too long (compat)");
      break;

    case -512: // WOLFSSL_EVP_R_BAD_DECRYPT_E :
      message = StringFromLiteral("Decryption failed (EVP)");
      break;

    case -513: // WOLFSSL_EVP_R_BN_DECODE_ERROR:
      message = StringFromLiteral("Bignum decode error (EVP)");
      break;

    case -514: // WOLFSSL_EVP_R_DECODE_ERROR  :
      message = StringFromLiteral("Decode error (EVP)");
      break;

    case -515: // WOLFSSL_EVP_R_PRIVATE_KEY_DECODE_ERROR:
      message = StringFromLiteral("Private key decode error (EVP)");
      break;
    }
  }

  StringBuilderAppendString(sb, &message);
  StringBuilderAppendStringLiteral(sb, " (Errnum ");
  StringBuilderAppendS32(sb, errnum);
  StringBuilderAppendStringLiteral(sb, ")");
}
