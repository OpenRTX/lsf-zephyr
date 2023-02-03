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

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

void main(void)
{
	const struct device *display_dev;

	display_dev = device_get_binding(CONFIG_LV_Z_DISPLAY_DEV_NAME);
	if (display_dev == NULL) {
		LOG_ERR("Device not ready, aborting test");
		return;
	}

	display_blanking_off(display_dev);

	extern void lv_demo_benchmark(void);
	lv_demo_benchmark();

	while (1) {
		lv_task_handler();
		k_sleep(K_MSEC(10));
	}
}
