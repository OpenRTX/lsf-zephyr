/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crypto/crypto.h"

#include <string.h>

#include <device.h>
#include <logging/log.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(crypto), okay)
#define CRYPTO_DRV_NAME DT_NODELABEL(crypto)
#else
#error "can't find crypto"
#endif

uint32_t cap_flags;

static void print_buffer_comparison(uint8_t *wanted_result, uint8_t *result, size_t length)
{
	int i, j;

	printk("Was waiting for: \n");

	for (i = 0, j = 1; i < length; i++, j++) {
		printk("0x%02x ", wanted_result[i]);

		if (j == 10) {
			printk("\n");
			j = 0;
		}
	}

	printk("\n But got:\n");

	for (i = 0, j = 1; i < length; i++, j++) {
		printk("0x%02x ", result[i]);

		if (j == 10) {
			printk("\n");
			j = 0;
		}
	}

	printk("\n");
}

void ecb_mode(const struct device *dev)
{
	/* from FIPS-197 test vectors */
	uint8_t ecb_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			       0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
	uint8_t ecb_plaintext[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
				     0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	uint8_t ecb_ciphertext[16] = {0x69, 0xC4, 0xE0, 0xD8, 0x6A, 0x7B, 0x04, 0x30,
				      0xD8, 0xCD, 0xB7, 0x80, 0x70, 0xB4, 0xC5, 0x5A};

	uint8_t encrypted[16] = {0};
	uint8_t decrypted[16] = {0};
	struct cipher_ctx ini = {
		.keylen = sizeof(ecb_key),
		.key.bit_stream = ecb_key,
		.flags = cap_flags,
	};
	struct cipher_pkt encrypt = {
		.in_buf = ecb_plaintext,
		.in_len = sizeof(ecb_plaintext),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypt.out_buf,
		.in_len = sizeof(encrypted),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_ECB,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return;
	}

	if (cipher_block_op(&ini, &encrypt)) {
		LOG_ERR("ECB mode ENCRYPT - Failed");
		goto out;
	}

	LOG_INF("Output length (encryption): %d", encrypt.out_len);

	if (memcmp(encrypt.out_buf, ecb_ciphertext, sizeof(ecb_ciphertext))) {
		LOG_ERR("ECB mode ENCRYPT - Mismatch between expected and "
			"returned cipher text");
		print_buffer_comparison(ecb_ciphertext, encrypt.out_buf, sizeof(ecb_ciphertext));
		goto out;
	}

	LOG_INF("ECB mode ENCRYPT - Match");
	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_ECB,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return;
	}

	if (cipher_block_op(&ini, &decrypt)) {
		LOG_ERR("ECB mode DECRYPT - Failed");
		goto out;
	}

	LOG_INF("Output length (decryption): %d", decrypt.out_len);

	if (memcmp(decrypt.out_buf, ecb_plaintext, sizeof(ecb_plaintext))) {
		LOG_ERR("ECB mode DECRYPT - Mismatch between plaintext and "
			"decrypted cipher text");
		print_buffer_comparison(ecb_plaintext, decrypt.out_buf, sizeof(ecb_plaintext));
		goto out;
	}

	LOG_INF("ECB mode DECRYPT - Match");
out:
	cipher_free_session(dev, &ini);
}

void main(void)
{
	const struct device *dev = DEVICE_DT_GET(CRYPTO_DRV_NAME);

	if (!dev) {
		LOG_ERR("crypto device not found \n");
		return;
	}

	LOG_INF("AES128 ECB Cipher Sample");

	ecb_mode(dev);
}
