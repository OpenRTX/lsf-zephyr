/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/display.h>
#include <drivers/pwm.h>
#include <drivers/pinmux.h>
#include <lvgl.h>
// #include "demos/lv_demos.h"
//#include "demos/benchmark/lv_demo_benchmark.h"
#include <stdio.h>
#include <string.h>
#include <zephyr.h>

#include <init.h>
#include <console/console.h>
#include <hal/PSRAMManager.h>

#include "lvgl_driver.h"

#include "devicetree.h"
#include <drivers/gpio.h>
#include <logging/log.h>

// #define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
// #include <logging/log.h>

// LOG_MODULE_REGISTER(app);

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define GPIO_A_01 1
#define GPIO_A_01_FUNC_SWDIO 0
#define GPIO_A_01_FUNC_PWM1 11

#if LV_USE_FLEX

static void sw_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *sw = lv_event_get_target(e);

	if (code == LV_EVENT_VALUE_CHANGED) {
		lv_obj_t *list = lv_event_get_user_data(e);

		if (lv_obj_has_state(sw, LV_STATE_CHECKED))
			lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLL_ONE);
		else
			lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_ONE);
	}
}

/**
 * Show an example to scroll snap
 */
void lv_example_scroll_2(void)
{
	lv_obj_t *panel = lv_obj_create(lv_scr_act());
	lv_obj_set_size(panel, 280, 120);
	lv_obj_set_scroll_snap_x(panel, LV_SCROLL_SNAP_CENTER);
	lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
	lv_obj_align(panel, LV_ALIGN_CENTER, 0, 20);

	uint32_t i;
	for (i = 0; i < 10; i++) {
		lv_obj_t *btn = lv_btn_create(panel);
		lv_obj_set_size(btn, 150, lv_pct(100));

		lv_obj_t *label = lv_label_create(btn);
		if (i == 3) {
			lv_label_set_text_fmt(label, "Panel %" LV_PRIu32 "\nno snap", i);
			lv_obj_clear_flag(btn, LV_OBJ_FLAG_SNAPPABLE);
		} else {
			lv_label_set_text_fmt(label, "Panel %" LV_PRIu32, i);
		}

		lv_obj_center(label);
	}
	lv_obj_update_snap(panel, LV_ANIM_ON);

#if LV_USE_SWITCH
	/*Switch between "One scroll" and "Normal scroll" mode*/
	lv_obj_t *sw = lv_switch_create(lv_scr_act());
	lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -20, 10);
	lv_obj_add_event_cb(sw, sw_event_cb, LV_EVENT_ALL, panel);
	lv_obj_t *label = lv_label_create(lv_scr_act());
	lv_label_set_text(label, "One scroll");
	lv_obj_align_to(label, sw, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
#endif
}

#endif

#ifdef LV_USE_BTN

static void btn_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *btn = lv_event_get_target(e);
	if (code == LV_EVENT_CLICKED) {
		static uint8_t cnt = 0;
		cnt++;

		/*Get the first child of the button which is the label and change its text*/
		lv_obj_t *label = lv_obj_get_child(btn, 0);
		lv_label_set_text_fmt(label, "Button: %d", cnt);
	}
}

/**
 * Create a button with a label and react on click event.
 */
void lv_example_get_started_1(void)
{
	lv_obj_t *btn = lv_btn_create(lv_scr_act()); /*Add a button the current screen*/
	lv_obj_set_pos(btn, 10, 10); /*Set its position*/
	lv_obj_set_size(btn, 120, 50); /*Set its size*/
	lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL,
			    NULL); /*Assign a callback to the button*/

	lv_obj_t *label = lv_label_create(btn); /*Add a label to the button*/
	lv_label_set_text(label, "Button"); /*Set the labels text*/
	lv_obj_center(label);
}

#endif

static int PSRAM_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	PSRAM_Initialize();
	return 0;
}

SYS_INIT(PSRAM_init, PRE_KERNEL_2, 0);

void main(void)
{
	uint32_t count = 0U;
	const struct device *display_dev;

	console_init();
	lvgl_init(NULL);

	display_dev = device_get_binding(CONFIG_LV_Z_DISPLAY_DEV_NAME);

	if (display_dev == NULL) {
		printk("device not found.  Aborting test.");
		return;
	}

	//display_blanking_on(display_dev);
	lv_example_scroll_2();

	lv_task_handler();

	display_blanking_off(display_dev);

	const struct device *pinmuxa = DEVICE_DT_GET(DT_NODELABEL(pinmuxa));
	const struct device *pwmlcd = DEVICE_DT_GET(DT_NODELABEL(pwmlcd));
	pinmux_pin_set(pinmuxa, GPIO_A_01, GPIO_A_01_FUNC_PWM1);
	pwm_pin_set_nsec(pwmlcd, GPIO_A_01, 20000, 200, 1);

	while (1) {
		lv_task_handler();
		k_sleep(K_MSEC(10));
		++count;
	}
}
