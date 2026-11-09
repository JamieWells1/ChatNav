#ifndef CHATNAV_MBEDTLS_CONFIG_H
#define CHATNAV_MBEDTLS_CONFIG_H

/* Minimal TLS 1.2 client: ECDHE-ECDSA with P-256 and AES-GCM. The server cert is
 * self-signed and pinned by fingerprint, so there is no CA store, no PEM, no clock and
 * no RSA. Plan section 5.5 budget: 50-70 KB for mbedTLS. */

#define MBEDTLS_ALLOW_PRIVATE_ACCESS /* lwIP altcp reads ssl internals */
#define MBEDTLS_HAVE_TIME            /* session start stamp only, no cert date checks */
#define MBEDTLS_PLATFORM_MS_TIME_ALT /* net/mbedtls_time.c */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_NO_UDBL_DIVISION
#define MBEDTLS_ECP_NIST_OPTIM

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_OID_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET

/* Records: the server may send 16 KB; we send at most one audio chunk (2 KB) at a time. */
#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096

#endif
