/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <string.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/counter.h>
/*
 * Copyright (c) 2019 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #include <zephyr/zephyr.h>

// #include <zephyr/device.h>
// #include <zephyr/drivers/counter.h>
// #include <zephyr/sys/printk.h>

#define DELAY 2000000
#define ALARM_CHANNEL_ID 0

struct counter_alarm_cfg alarm_cfg;

#define TIMER DT_LABEL(DT_NODELABEL(dultim))
bool level_1 = 0;
static void test_counter_interrupt_fn(const struct device *counter_dev, uint8_t chan_id,
				      uint32_t ticks, void *user_data)
{
	struct counter_alarm_cfg *config = user_data;
	uint32_t now_tick;
	int err;

	err = counter_get_value(counter_dev, &now_ticks);
	if (err) {
		printk("Failed to read counter value (err %d)", err);
		return;
	}
	level_1 = !level_1;
	GPIO_PinWrite(GPIOA(), (1 << 8), level_1);
	// printk("!!! Alarm !!!\n");
	// last_data = now_ticks;
	/* Set a new alarm with a double length duration */
	// config->ticks = config->ticks * 2U;

	// err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID,
	// 				user_data);
	if (err != 0) {
		printk("Alarm could not be set\n");
	}
	
	
}

void main(void)
{
	const struct device *counter_dev;
	int err;
	printk("Counter alarm sample\n\n");
	counter_dev = device_get_binding(TIMER);
	if (counter_dev == NULL) {
		printk("Device not found\n");
		return;
	}
	GPIO_SetDir(GPIOA(), (1 << 8), 1);
	GPIO_PinWrite(GPIOA(), (1 << 8), 0);
	// counter_start(counter_dev);

	alarm_cfg.flags = 0;
	alarm_cfg.ticks = 16000;
	alarm_cfg.callback = test_counter_interrupt_fn;
	alarm_cfg.user_data = &alarm_cfg;

	err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg);
	if (err < 0) {
		printk("counter_set_channel_alarm err \n");
	}
	counter_start(counter_dev);

	printk("over\n");
	while (1) {
		k_sleep(K_FOREVER);
	}
}

