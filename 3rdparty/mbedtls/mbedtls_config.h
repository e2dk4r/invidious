/* TLS 1.3 core */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_PROTO_TLS1_3
#define MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_KEY_EXCHANGE_WITH_CERT_ENABLED

/* X25519 for ECDHE */
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED // x25519

/* ECDSA-P256 signature support */
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED // NIST P-256
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C

/* AES-GCM */
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C

/* ChaCha20-Poly1305  */
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C

/* Hash */
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C

#define MBEDTLS_CIPHER_SUITES                                                                                          \
  MBEDTLS_TLS1_3_AES_256_GCM_SHA384                                                                                    \
  MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256

/* TLS extras */
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_SSL_SERVER_NAME_INDICATION

/* PSA crypto (required for TLS 1.3) */
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_PSA_CRYPTO_CLIENT
#define MBEDTLS_PSA_CRYPTO_CONFIG
#define MBEDTLS_BASE64_C

/* platform */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_NET_C
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_SSE2
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_AESNI_C

#if IS_PLATFORM_LINUX
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#include <sys/random.h>
static int
mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
  (void)data; // unused
  ssize_t bytesWritten = getrandom(output, len, 0);
  if (bytesWritten < 0)
    return -1;

  *olen = (size_t)bytesWritten;
  return 0;
}
#endif

// Enable for troubleshooting
// #define MBEDTLS_DEBUG_C
