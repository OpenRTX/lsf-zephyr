/*
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "mbedtls/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSK_AES_ENCRYPT     1
#define CSK_AES_DECRYPT     0


#define ERR_CSK_AES_INVALID_KEY_LENGTH                -0x0020  /**< Invalid key length. */
#define ERR_CSK_AES_INVALID_INPUT_LENGTH              -0x0022  /**< Invalid data input length. */

/**
 * \brief          AES context structure
 *
 */
typedef struct {
    uint8_t key_bytes;
    uint8_t key[32];
} csk_aes_context;

/**
 * \brief The AES XTS context-type definition.
 */
typedef struct
{
    csk_aes_context crypt; /*!< The AES context to use for AES block
                                        encryption or decryption. */
    csk_aes_context tweak; /*!< The AES context used for tweak
                                        computation. */
} csk_aes_xts_context;




/**
 * \brief Lock access to AES hardware unit
 *
 * AES hardware unit can only be used by one
 * consumer at a time.
 *
 * csk_aes_xxx API calls automatically manage locking & unlocking of
 * hardware, this function is only needed if you want to call
 * ets_aes_xxx functions directly.
 */
void csk_aes_acquire_hardware( void );

/**
 * \brief Unlock access to AES hardware unit
 *
 * csk_aes_xxx API calls automatically manage locking & unlocking of
 * hardware, this function is only needed if you want to call
 * ets_aes_xxx functions directly.
 */
void csk_aes_release_hardware( void );

/**
 * \brief          Initialize AES context
 *
 * \param ctx      AES context to be initialized
 */
void csk_aes_init( csk_aes_context *ctx );

/**
 * \brief          Clear AES context
 *
 * \param ctx      AES context to be cleared
 */
void csk_aes_free( csk_aes_context *ctx );

/**
 * \brief          This function initializes the specified AES XTS context.
 *
 *                 It must be the first API called before using
 *                 the context.
 *
 * \param ctx      The AES XTS context to initialize.
 */
void csk_aes_xts_init( csk_aes_xts_context *ctx );

/**
 * \brief          This function releases and clears the specified AES XTS context.
 *
 * \param ctx      The AES XTS context to clear.
 */
void csk_aes_xts_free( csk_aes_xts_context *ctx );

/**
 * \brief          AES set key schedule (encryption or decryption)
 *
 * \param ctx      AES context to be initialized
 * \param key      encryption key
 * \param keybits  must be 128, 192 or 256
 *
 * \return         0 if successful, or ERR_AES_INVALID_KEY_LENGTH
 */
int csk_aes_setkey( csk_aes_context *ctx, const unsigned char *key, unsigned int keybits );

/**
 * \brief          AES-ECB block encryption/decryption
 *
 * \param ctx      AES context
 * \param mode     AES_ENCRYPT or AES_DECRYPT
 * \param input    16-byte input block
 * \param output   16-byte output block
 *
 * \return         0 if successful
 */
int csk_aes_crypt_ecb( csk_aes_context *ctx, int mode, const unsigned char input[16], unsigned char output[16] );

/**
 * \brief          AES-CBC buffer encryption/decryption
 *                 Length should be a multiple of the block
 *                 size (16 bytes)
 *
 * \note           Upon exit, the content of the IV is updated so that you can
 *                 call the function same function again on the following
 *                 block(s) of data and get the same result as if it was
 *                 encrypted in one call. This allows a "streaming" usage.
 *                 If on the other hand you need to retain the contents of the
 *                 IV, you should either save it manually or use the cipher
 *                 module instead.
 *
 * \param ctx      AES context
 * \param mode     AES_ENCRYPT or AES_DECRYPT
 * \param length   length of the input data
 * \param iv       initialization vector (updated after use)
 * \param input    buffer holding the input data
 * \param output   buffer holding the output data
 *
 * \return         0 if successful, or ERR_AES_INVALID_INPUT_LENGTH
 */
int csk_aes_crypt_cbc( csk_aes_context *ctx,
                       int mode,
                       size_t length,
                       unsigned char iv[16],
                       const unsigned char *input,
                       unsigned char *output );


/**
 * \brief          AES-CFB128 buffer encryption/decryption.
 *
 * Note: Due to the nature of CFB you should use the same key schedule for
 * both encryption and decryption. So a context initialized with
 * csk_aes_setkey_enc() for both AES_ENCRYPT and AES_DECRYPT.
 *
 * \note           Upon exit, the content of the IV is updated so that you can
 *                 call the function same function again on the following
 *                 block(s) of data and get the same result as if it was
 *                 encrypted in one call. This allows a "streaming" usage.
 *                 If on the other hand you need to retain the contents of the
 *                 IV, you should either save it manually or use the cipher
 *                 module instead.
 *
 * \param ctx      AES context
 * \param mode     AES_ENCRYPT or AES_DECRYPT
 * \param length   length of the input data
 * \param iv_off   offset in IV (updated after use)
 * \param iv       initialization vector (updated after use)
 * \param input    buffer holding the input data
 * \param output   buffer holding the output data
 *
 * \return         0 if successful
 */
int csk_aes_crypt_cfb128( csk_aes_context *ctx,
                          int mode,
                          size_t length,
                          size_t *iv_off,
                          unsigned char iv[16],
                          const unsigned char *input,
                          unsigned char *output );

/**
 * \brief          AES-CFB8 buffer encryption/decryption.
 *
 * Note: Due to the nature of CFB you should use the same key schedule for
 * both encryption and decryption. So a context initialized with
 * csk_aes_setkey_enc() for both AES_ENCRYPT and AES_DECRYPT.
 *
 * \note           Upon exit, the content of the IV is updated so that you can
 *                 call the function same function again on the following
 *                 block(s) of data and get the same result as if it was
 *                 encrypted in one call. This allows a "streaming" usage.
 *                 If on the other hand you need to retain the contents of the
 *                 IV, you should either save it manually or use the cipher
 *                 module instead.
 *
 * \param ctx      AES context
 * \param mode     AES_ENCRYPT or AES_DECRYPT
 * \param length   length of the input data
 * \param iv       initialization vector (updated after use)
 * \param input    buffer holding the input data
 * \param output   buffer holding the output data
 *
 * \return         0 if successful
 */
int csk_aes_crypt_cfb8( csk_aes_context *ctx,
                        int mode,
                        size_t length,
                        unsigned char iv[16],
                        const unsigned char *input,
                        unsigned char *output );

/**
 * \brief               AES-CTR buffer encryption/decryption
 *
 * Warning: You have to keep the maximum use of your counter in mind!
 *
 * Note: Due to the nature of CTR you should use the same key schedule for
 * both encryption and decryption. So a context initialized with
 * csk_aes_setkey_enc() for both AES_ENCRYPT and AES_DECRYPT.
 *
 * \param ctx           AES context
 * \param length        The length of the data
 * \param nc_off        The offset in the current stream_block (for resuming
 *                      within current cipher stream). The offset pointer to
 *                      should be 0 at the start of a stream.
 * \param nonce_counter The 128-bit nonce and counter.
 * \param stream_block  The saved stream-block for resuming. Is overwritten
 *                      by the function.
 * \param input         The input data stream
 * \param output        The output data stream
 *
 * \return         0 if successful
 */
int csk_aes_crypt_ctr( csk_aes_context *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output );

/**
 * \brief       This function performs an AES-OFB (Output Feedback Mode)
 *              encryption or decryption operation.
 *
 * \param ctx      The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * \param length   The length of the input data.
 * \param iv_off   The offset in IV (updated after use).
 *                 It must point to a valid \c size_t.
 * \param iv       The initialization vector (updated after use).
 *                 It must be a readable and writeable buffer of \c 16 Bytes.
 * \param input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * \param output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * \return         \c 0 on success.
 */
int csk_aes_crypt_ofb( csk_aes_context *ctx,
                       size_t length,
                       size_t *iv_off,
                       unsigned char iv[16],
                       const unsigned char *input,
                       unsigned char *output );

/**
 * \brief          This function prepares an XTS context for encryption and
 *                 sets the encryption key.
 *
 * \param ctx      The AES XTS context to which the key should be bound.
 * \param key      The encryption key. This is comprised of the XTS key1
 *                 concatenated with the XTS key2.
 * \param keybits  The size of \p key passed in bits. Valid options are:
 *                 <ul><li>256 bits (each of key1 and key2 is a 128-bit key)</li>
 *                 <li>512 bits (each of key1 and key2 is a 256-bit key)</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
int csk_aes_xts_setkey_enc( csk_aes_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits );

/**
 * \brief          This function prepares an XTS context for decryption and
 *                 sets the decryption key.
 *
 * \param ctx      The AES XTS context to which the key should be bound.
 * \param key      The decryption key. This is comprised of the XTS key1
 *                 concatenated with the XTS key2.
 * \param keybits  The size of \p key passed in bits. Valid options are:
 *                 <ul><li>256 bits (each of key1 and key2 is a 128-bit key)</li>
 *                 <li>512 bits (each of key1 and key2 is a 256-bit key)</li></ul>
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_KEY_LENGTH on failure.
 */
int csk_aes_xts_setkey_dec( csk_aes_xts_context *ctx,
                                const unsigned char *key,
                                unsigned int keybits );


/**
 * \brief           Internal AES block encryption function
 *                  (Only exposed to allow overriding it,
 *                  see AES_ENCRYPT_ALT)
 *
 * \param ctx       AES context
 * \param input     Plaintext block
 * \param output    Output (ciphertext) block
 */
int csk_internal_aes_encrypt( csk_aes_context *ctx, const unsigned char input[16], unsigned char output[16] );

/**
 * \brief           Internal AES block decryption function
 *                  (Only exposed to allow overriding it,
 *                  see AES_DECRYPT_ALT)
 *
 * \param ctx       AES context
 * \param input     Ciphertext block
 * \param output    Output (plaintext) block
 */
int csk_internal_aes_decrypt( csk_aes_context *ctx, const unsigned char input[16], unsigned char output[16] );

/** AES-XTS buffer encryption/decryption */
int csk_aes_crypt_xts( csk_aes_xts_context *ctx, int mode, size_t length, const unsigned char data_unit[16], const unsigned char *input, unsigned char *output );

#ifdef __cplusplus
}
#endif
