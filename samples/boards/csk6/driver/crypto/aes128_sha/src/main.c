/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <zephyr.h>
#include <string.h>
#include "crypto/crypto.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(crypto), okay)
#define CRYPTO_DRV_NAME DT_NODELABEL(crypto)
#else
#error "can't find crypto"
#endif

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

void sha_mode(const struct device *dev)
{
	int ret;
	struct hash_ctx ctx;

	ctx.flags = CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS;

	ret = hash_begin_session(dev, &ctx, CRYPTO_HASH_ALGO_SHA256);
	zassert_true(ret == 0, "Failed to init sha256 session");

#define TEST_HASH(_i)                                                                              \
	do {                                                                                       \
		uint8_t out_buf[32] = { 0 };                                                       \
		struct hash_pkt pkt = {                                                            \
			.in_buf = test##_i,                                                        \
			.in_len = sizeof(test##_i),                                                \
			.out_buf = out_buf,                                                        \
		};                                                                                 \
		ret = hash_compute(&ctx, &pkt);                                                    \
	} while (0)

	TEST_HASH(1);
	TEST_HASH(2);
	TEST_HASH(3);
	TEST_HASH(4);
	TEST_HASH(5);
	TEST_HASH(6);
	TEST_HASH(7);

	hash_free_session(dev, &ctx);
}

void main(void)
{
	const struct device *dev = DEVICE_DT_GET(CRYPTO_DRV_NAME);

	if (!dev) {
		LOG_ERR("crypto device not found \n");
		return;
	}

	LOG_INF("SHA128 HASH Cipher Sample");

	sha_mode(dev);
}
