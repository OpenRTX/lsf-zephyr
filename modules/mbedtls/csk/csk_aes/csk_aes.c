/**
 * 
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Additions Copyright (C) 2016-2017, Espressif Systems (Shanghai) PTE Ltd
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
/*
 *  The AES block cipher was designed by Vincent Rijmen and Joan Daemen.
 *
 *  http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
 *  http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
 */
#include <zephyr/device.h>
#include <zephyr/zephyr.h>
#include <zephyr/crypto/crypto.h>

#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "mbedtls/aes.h"
#include "mbedtls/platform_util.h"

#include "csk_aes.h"

#define AES_128_KEY_BYTES (128 / 8)
#define AES_192_KEY_BYTES (192 / 8)
#define AES_256_KEY_BYTES (256 / 8)

static uint32_t cap_flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS;

bool valid_key_length(const csk_aes_context *ctx)
{
	bool valid_len = (ctx->key_bytes == AES_128_KEY_BYTES);
	return valid_len;
}

void csk_aes_init(csk_aes_context *ctx)
{
	if (ctx == NULL) {
		return;
	}

	bzero(ctx, sizeof(csk_aes_context));
}

void csk_aes_free(csk_aes_context *ctx)
{
	if (ctx == NULL) {
		return;
	}

	bzero(ctx, sizeof(csk_aes_context));
}

/*
 * AES key schedule (same for encryption or decryption, as hardware handles schedule)
 *
 */
int csk_aes_setkey(csk_aes_context *ctx, const unsigned char *key, unsigned int keybits)
{
	if (keybits != 128) {
		if (keybits == 192 || keybits == 256) {
			return MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
		} else {
			return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
		}
	}
	ctx->key_bytes = keybits / 8;
	memcpy(ctx->key, key, ctx->key_bytes);
	return 0;
}

/*
 * AES-ECB block encryption
 */
int csk_internal_aes_encrypt(csk_aes_context *ctx, const unsigned char input[16],
			     unsigned char output[16])
{
	int ret = 0;
	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}

	struct cipher_ctx ini = {
		.keylen = ctx->key_bytes,
		.key.bit_stream = ctx->key,
		.flags = cap_flags,
	};
	struct cipher_pkt encrypt = {
		.in_buf = (uint8_t *)input,
		.in_len = 16,
		.out_buf = output,
		.out_buf_max = 16,
	};
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(crypto));
	ret = cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_ECB,
				   CRYPTO_CIPHER_OP_ENCRYPT);
	assert(ret == 0);
	ret = cipher_block_op(&ini, &encrypt);
	assert(ret == 0);
	ret = cipher_free_session(dev, &ini);
	assert(ret == 0);

	return 0;
}

/*
 * AES-ECB block decryption
 */

int csk_internal_aes_decrypt(csk_aes_context *ctx, const unsigned char input[16],
			     unsigned char output[16])
{
	int ret = 0;
	struct cipher_ctx ini = {
		.keylen = ctx->key_bytes,
		.key.bit_stream = ctx->key,
		.flags = cap_flags,
	};
	struct cipher_pkt decrypt = {
		.in_buf = (uint8_t *)input,
		.in_len = 16,
		.out_buf = output,
		.out_buf_max = 16,
	};
	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(crypto));
	ret = cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_ECB,
				   CRYPTO_CIPHER_OP_DECRYPT);
	assert(ret == 0);
	ret = cipher_block_op(&ini, &decrypt);
	assert(ret == 0);
	cipher_free_session(dev, &ini);
	assert(ret == 0);
	return 0;
}

/*
 * AES-ECB block encryption/decryption
 */
int csk_aes_crypt_ecb(csk_aes_context *ctx, int mode, const unsigned char input[16],
		      unsigned char output[16])
{
	int ret = 0;
	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}

	if (mode == CSK_AES_ENCRYPT) {
		ret = csk_internal_aes_encrypt(ctx, input, output);
	} else {
		ret = csk_internal_aes_decrypt(ctx, input, output);
	}
	assert(ret == 0);

	return 0;
}

/*
 * AES-CBC buffer encryption/decryption
 */
int csk_aes_crypt_cbc(csk_aes_context *ctx, int mode, size_t length, unsigned char iv[16],
		      const unsigned char *input, unsigned char *output)
{
	int i;
	int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
	unsigned char temp[16];

	if (length % 16) {
		return (ERR_CSK_AES_INVALID_INPUT_LENGTH);
	}

	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}

	if (mode == CSK_AES_DECRYPT) {
		while (length > 0) {
			memcpy(temp, input, 16);
			ret = csk_aes_crypt_ecb(ctx, mode, input, output);
			if (ret != 0)
				goto exit;

			for (i = 0; i < 16; i++)
				output[i] = (unsigned char)(output[i] ^ iv[i]);

			memcpy(iv, temp, 16);

			input += 16;
			output += 16;
			length -= 16;
		}
	} else {
		while (length > 0) {
			for (i = 0; i < 16; i++)
				output[i] = (unsigned char)(input[i] ^ iv[i]);

			ret = csk_aes_crypt_ecb(ctx, mode, output, output);
			if (ret != 0)
				goto exit;
			memcpy(iv, output, 16);

			input += 16;
			output += 16;
			length -= 16;
		}
	}

	ret = 0;
exit:
	return (ret);
}

/*
 * AES-CFB128 buffer encryption/decryption
 */
int csk_aes_crypt_cfb128(csk_aes_context *ctx, int mode, size_t length, size_t *iv_off,
			 unsigned char iv[16], const unsigned char *input, unsigned char *output)
{
	int c;
	size_t n = *iv_off;

	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}

	if (mode == CSK_AES_DECRYPT) {
		while (length--) {
			if (n == 0) {
				csk_aes_crypt_ecb(ctx, CSK_AES_ENCRYPT, iv, iv);
			}

			c = *input++;
			*output++ = (unsigned char)(c ^ iv[n]);
			iv[n] = (unsigned char)c;

			n = (n + 1) & 0x0F;
		}
	} else {
		while (length--) {
			if (n == 0) {
				csk_aes_crypt_ecb(ctx, CSK_AES_ENCRYPT, iv, iv);
			}

			iv[n] = *output++ = (unsigned char)(iv[n] ^ *input++);

			n = (n + 1) & 0x0F;
		}
	}

	*iv_off = n;

	return 0;
}

/*
 * AES-CFB8 buffer encryption/decryption
 */
int csk_aes_crypt_cfb8(csk_aes_context *ctx, int mode, size_t length, unsigned char iv[16],
		       const unsigned char *input, unsigned char *output)
{
	unsigned char c;
	unsigned char ov[17];

	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}

	while (length--) {
		memcpy(ov, iv, 16);
		csk_aes_crypt_ecb(ctx, CSK_AES_ENCRYPT, iv, iv);

		if (mode == CSK_AES_DECRYPT) {
			ov[16] = *input;
		}

		c = *output++ = (unsigned char)(iv[0] ^ *input++);

		if (mode == CSK_AES_ENCRYPT) {
			ov[16] = c;
		}

		memcpy(iv, ov + 1, 16);
	}

	return 0;
}

/*
 * AES-OFB (Output Feedback Mode) buffer encryption/decryption
 */
int csk_aes_crypt_ofb(csk_aes_context *ctx, size_t length, size_t *iv_off, unsigned char iv[16],
		      const unsigned char *input, unsigned char *output)
{
	int ret = 0;
	size_t n;

	if (ctx == NULL || iv_off == NULL || iv == NULL || input == NULL || output == NULL) {
		return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
	}

	n = *iv_off;

	if (n > 15) {
		return (MBEDTLS_ERR_AES_BAD_INPUT_DATA);
	}

	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}

	while (length--) {
		if (n == 0) {
			csk_aes_crypt_ecb(ctx, CSK_AES_ENCRYPT, iv, iv);
		}
		*output++ = *input++ ^ iv[n];

		n = (n + 1) & 0x0F;
	}

	*iv_off = n;

	return (ret);
}

/*
 * AES-CTR buffer encryption/decryption
 */
int csk_aes_crypt_ctr(csk_aes_context *ctx, size_t length, size_t *nc_off,
		      unsigned char nonce_counter[16], unsigned char stream_block[16],
		      const unsigned char *input, unsigned char *output)
{
	int c, i;
	size_t n = *nc_off;

	if (!valid_key_length(ctx)) {
		return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
	}

	while (length--) {
		if (n == 0) {
			csk_aes_crypt_ecb(ctx, CSK_AES_ENCRYPT, nonce_counter, stream_block);

			for (i = 16; i > 0; i--) {
				if (++nonce_counter[i - 1] != 0) {
					break;
				}
			}
		}
		c = *input++;
		*output++ = (unsigned char)(c ^ stream_block[n]);

		n = (n + 1) & 0x0F;
	}

	*nc_off = n;

	return 0;
}
