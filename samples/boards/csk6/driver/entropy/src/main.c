/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/entropy.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(sample);

#define ENTROPY_LEN_MAX 256
uint8_t entropy_buf[ENTROPY_LEN_MAX];

static void dump_entropy(const char *entropy_buf, uint32_t len)
{
	printk("entropy buf dump: \n");

	for (uint32_t cnt = 0; cnt < len; cnt++) {

		printk("%.2x  ", entropy_buf[cnt]);
		if (0 == ((cnt + 1) % 16)) {
			printk("\n");
		}
	}

	printk("\n");
}

void main(void)
{
	const struct device *dev;
	int ret = 0;

	dev = device_get_binding(DT_LABEL(DT_CHOSEN(zephyr_entropy)));
	if (!dev) {
		printk("error: no random device %s\r\n", DT_LABEL(DT_CHOSEN(zephyr_entropy)));
		return;
	}

	printk("start get entropy sample \r\n");

	ret = entropy_get_entropy(dev, entropy_buf, ENTROPY_LEN_MAX);
	if (ret != 0) {
		printk("error: get entropy failed %d\r\n", ret);
		return;
	}

	dump_entropy(entropy_buf, ENTROPY_LEN_MAX);

	printk("entropy sample done \r\n");

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}
