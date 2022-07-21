/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <zephyr.h>
#include <string.h>
#include <crypto/cipher.h>


#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);



#if DT_NODE_HAS_STATUS(DT_NODELABEL(crypto), okay)
#define CRYPTO_DRV_NAME DT_NODELABEL(crypto)
#else
#error "can't find crypto dev"
#endif

uint32_t cap_flags;

static void print_buffer_comparison(uint8_t *wanted_result,
				    uint8_t *result, size_t length)
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

void cbc_mode(const struct device *dev)
{
	
	uint8_t cbc_key[16] = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
	};

	uint8_t iv[16] = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
	};

	uint8_t  plaintext[32] = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
	};

	uint8_t cbc_ciphertext[32] = {
		0xC6, 0xA1, 0x3B, 0x37, 0x87, 0x8F, 0x5B, 0x82, 0x6F, 0x4F, 0x81, 0x62, 0xA1, 0xC8, 0xD8, 0x79, 
		0xB5, 0x8A, 0x10, 0x64, 0xD8, 0xAC, 0xA9, 0x9B, 0xD9, 0xB0, 0x40, 0x5B, 0x85, 0x45, 0xF5, 0xBB 
	};

	uint8_t encrypted[32] = {0};
	uint8_t decrypted[32] = {0};

	struct cipher_ctx ini = {
		.keylen = sizeof(cbc_key),
		.key.bit_stream = cbc_key,
		.flags = cap_flags,
	};
	struct cipher_pkt encrypt = {
		.in_buf = plaintext,
		.in_len = sizeof(plaintext),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypt.out_buf,
		.in_len = sizeof(encrypted),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};


	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CBC,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return;
	}

	if (cipher_cbc_op(&ini, &encrypt, iv)) {
		LOG_ERR("CBC mode ENCRYPT - Failed");
		goto out;
	}

	LOG_INF("Output length (encryption): %d", encrypt.out_len);

	if (memcmp(encrypt.out_buf, cbc_ciphertext, sizeof(cbc_ciphertext))) {
		LOG_ERR("CBC mode ENCRYPT - Mismatch between expected and "
			    "returned cipher text");
		print_buffer_comparison(cbc_ciphertext, encrypt.out_buf,
					sizeof(cbc_ciphertext));
		goto out;
	}

	LOG_INF("CBC mode ENCRYPT - Match");
	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CBC,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return;
	}


	if (cipher_cbc_op(&ini, &decrypt, iv)) {
		LOG_ERR("CBC mode DECRYPT - Failed");
		goto out;
	}

	LOG_INF("Output length (decryption): %d", decrypt.out_len);

	if (memcmp(decrypt.out_buf, plaintext, sizeof(plaintext))) {
		LOG_ERR("CBC mode DECRYPT - Mismatch between plaintext and "
			    "decrypted cipher text");
		print_buffer_comparison(plaintext, decrypt.out_buf,
					sizeof(plaintext));
		goto out;
	}

	LOG_INF("CBC mode DECRYPT - Match");
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

	LOG_INF("AES128 CBC Cipher Sample");

	cbc_mode(dev);

}
