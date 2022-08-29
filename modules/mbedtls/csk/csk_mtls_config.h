/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CSK_CONFIG_H
#define CSK_CONFIG_H

/* \} name SECTION: System support */

/**
 * \name SECTION: mbed TLS feature support
 *
 * This section sets support for features that are or are not needed
 * within the modules that are enabled.
 * \{
 */

/* The following units have CSK hardware support,
   uncommenting each _ALT macro will use the
   hardware-accelerated implementation. */
#ifdef CONFIG_CSK_MBEDTLS_HARDWARE_AES
#define MBEDTLS_AES_ALT
#else
#undef MBEDTLS_AES_ALT
#endif

/**
 * \def MBEDTLS_CIPHER_MODE_CBC
 *
 * Enable Cipher Block Chaining mode (CBC) for symmetric ciphers.
 */
#define MBEDTLS_CIPHER_MODE_CBC

/**
 * \def MBEDTLS_CIPHER_MODE_CFB
 *
 * Enable Cipher Feedback mode (CFB) for symmetric ciphers.
 */
#define MBEDTLS_CIPHER_MODE_CFB

/**
 * \def MBEDTLS_CIPHER_MODE_CTR
 *
 * Enable Counter Block Cipher mode (CTR) for symmetric ciphers.
 */
#define MBEDTLS_CIPHER_MODE_CTR

/**
 * \def MBEDTLS_CIPHER_MODE_OFB
 *
 * Enable Output Feedback mode (OFB) for symmetric ciphers.
 */
#define MBEDTLS_CIPHER_MODE_OFB

/**
 * \def MBEDTLS_CIPHER_MODE_XTS
 *
 * Enable Xor-encrypt-xor with ciphertext stealing mode (XTS) for AES.
 */
#define MBEDTLS_CIPHER_MODE_XTS

#endif /* MBEDTLS_CONFIG_H */
