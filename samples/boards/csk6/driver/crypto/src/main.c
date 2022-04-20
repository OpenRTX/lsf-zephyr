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
#error "can't find crypto"
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

int validate_hw_compatibility(const struct device *dev)
{
	uint32_t flags = 0U;

	flags = cipher_query_hwcaps(dev);
	if ((flags & CAP_RAW_KEY) == 0U) {
		LOG_INF("Please provision the key separately "
			"as the module doesnt support a raw key");
		return -1;
	}

	if ((flags & CAP_SYNC_OPS) == 0U) {
		LOG_ERR("The app assumes sync semantics. "
		  "Please rewrite the app accordingly before proceeding");
		return -1;
	}

	if ((flags & CAP_SEPARATE_IO_BUFS) == 0U) {
		LOG_ERR("The app assumes distinct IO buffers. "
		"Please rewrite the app accordingly before proceeding");
		return -1;
	}

	cap_flags = flags;

	return 0;

}

void ecb_mode(const struct device *dev)
{
	/* from FIPS-197 test vectors */
	uint8_t ecb_key[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
	};
	uint8_t ecb_plaintext[16] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	};
	uint8_t ecb_ciphertext[16] = {
		0x69, 0xC4, 0xE0, 0xD8, 0x6A, 0x7B, 0x04, 0x30,
		0xD8, 0xCD, 0xB7, 0x80, 0x70, 0xB4, 0xC5, 0x5A
	};

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

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_ECB,
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
		print_buffer_comparison(ecb_ciphertext, encrypt.out_buf,
					sizeof(ecb_ciphertext));
		goto out;
	}

	LOG_INF("ECB mode ENCRYPT - Match");
	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_ECB,
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
		print_buffer_comparison(ecb_plaintext, decrypt.out_buf,
					sizeof(ecb_plaintext));
		goto out;
	}

	LOG_INF("ECB mode DECRYPT - Match");
out:
	cipher_free_session(dev, &ini);
}

void cbc_mode(const struct device *dev)
{
	
	uint8_t cbc_key[16] = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
	};

	uint8_t iv[16] = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x006,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
	};

	uint8_t  plaintext[32] = {
		0x00,0x01,0x02,0x03,0x04,0x05,0x006,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
		0x00,0x01,0x02,0x03,0x04,0x05,0x006,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
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


struct mode_test {
	const char *mode;
	void (*mode_func)(const struct device *dev);
};

void main(void)
{
	const struct device *dev = DEVICE_DT_GET(CRYPTO_DRV_NAME);
	// const struct device *dev;
	const struct mode_test modes[] = {
		{ .mode = "ECB Mode", .mode_func = ecb_mode },
		{ .mode = "CBC Mode", .mode_func = cbc_mode },
		{}
	};

	if (!dev) {
		LOG_ERR("crypto device not found \n");
		return;
	}

	if (validate_hw_compatibility(dev)) {
		LOG_ERR("Incompatible h/w");
		return;
	}

	LOG_INF("Cipher Sample");

	for (uint8_t i = 0; modes[i].mode; i++) {
		LOG_INF("%s", modes[i].mode);
		modes[i].mode_func(dev);
	}

}
