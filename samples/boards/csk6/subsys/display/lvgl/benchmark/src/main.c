/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>

#include <device.h>
#include <drivers/display.h>
#include <lvgl.h>
#include <zephyr.h>
#include <drivers/kscan.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

static void k_callback(const struct device *dev, uint32_t row, uint32_t col, bool pressed)
{
	ARG_UNUSED(dev);
	printk("row = %u col = %u, pressed:%s\n", row, col, pressed ? "TRUE" : "FLASE");
}

void kscan_init(void)
{
	const struct device *kscan_dev = device_get_binding("SPD2010");

	if (!device_is_ready(kscan_dev)) {
		LOG_ERR("kscan device %s not ready", kscan_dev->name);
		return;
	}

	kscan_config(kscan_dev, k_callback);
	kscan_enable_callback(kscan_dev);
}

void main(void)
{
	const struct device *display_dev;

	display_dev = device_get_binding(CONFIG_LV_Z_DISPLAY_DEV_NAME);
	if (display_dev == NULL) {
		LOG_ERR("Device not ready, aborting test");
		return;
	}

	// kscan_init();

	display_blanking_off(display_dev);
	// lv_disp_set_rotation(NULL, LV_DISP_ROT_270);

	extern void lv_demo_benchmark(void);
	lv_demo_benchmark();
	while (1) {
		lv_task_handler();
		k_sleep(K_MSEC(10));
	}
}
