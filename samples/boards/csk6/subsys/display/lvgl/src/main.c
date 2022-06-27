/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr.h>
#include <drivers/pwm.h>
#include <drivers/pinmux.h>
#include <drivers/gpio.h>


#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(app);



/* The devicetree node identifier for the "LCD0" alias. */
#define LCD0_NODE DT_ALIAS(lcdbacklight0)

#if DT_NODE_HAS_STATUS(LCD0_NODE, okay)
#define LCD0	DT_GPIO_LABEL(LCD0_NODE, gpios)
#define PIN	DT_GPIO_PIN(LCD0_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LCD0_NODE, gpios)
#else
/* A build error here means your board isn't set up to blink an LCD Backlight. */
#error "Unsupported board: LCD0 devicetree alias is not defined"
#define LCD0	""
#define PIN	0
#define FLAGS	0
#endif

void lcd_blanking_on()
{
	const struct device *dev;
	bool lcd_is_on = true;
	int ret;

	dev = device_get_binding(LCD0);
	if (dev == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (ret < 0) {
		return;
	}
	// LCD背光低電平有效
	gpio_pin_set(dev, PIN, !(int)lcd_is_on);
}


void main(void)
{
	uint32_t count = 0U;
	char count_str[11] = {0};
	const struct device *display_dev;
	lv_obj_t *hello_world_label;
	lv_obj_t *count_label;

	#if DT_NODE_HAS_STATUS(LCD0_NODE, okay)
	lcd_blanking_on();
	#endif

	display_dev = device_get_binding(CONFIG_LVGL_DISPLAY_DEV_NAME);

	if (display_dev == NULL) {
		LOG_ERR("device not found.  Aborting test.");
		return;
	}

	if (IS_ENABLED(CONFIG_LVGL_POINTER_KSCAN)) {
		lv_obj_t *hello_world_button;

		hello_world_button = lv_btn_create(lv_scr_act(), NULL);
		lv_obj_align(hello_world_button, NULL, LV_ALIGN_CENTER, 0, 0);
		lv_btn_set_fit(hello_world_button, LV_FIT_TIGHT);
		hello_world_label = lv_label_create(hello_world_button, NULL);
	} else {
		hello_world_label = lv_label_create(lv_scr_act(), NULL);
	}

	lv_label_set_text(hello_world_label, "Hello world!");
	lv_obj_align(hello_world_label, NULL, LV_ALIGN_CENTER, 0, 0);

	count_label = lv_label_create(lv_scr_act(), NULL);
	lv_obj_align(count_label, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

	lv_task_handler();
	display_blanking_off(display_dev);

	while (1) {
		if ((count % 100) == 0U) {
			sprintf(count_str, "%d", count/100U);
			lv_label_set_text(count_label, count_str);
		}
		lv_task_handler();
		k_sleep(K_MSEC(10));
		++count;
	}
}
