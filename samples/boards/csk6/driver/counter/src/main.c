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

#define TICKS 16000
#define ALARM_CHANNEL_ID 0
#define ALARM_CHANNEL_ID_1 1
struct counter_alarm_cfg alarm_cfg;

#define TIMER DT_LABEL(DT_NODELABEL(dual_timer))
static void test_counter_interrupt_fn(const struct device *counter_dev, uint8_t chan_id,
				      uint32_t ticks, void *user_data)
{
	uint32_t now_tick;
	int err;
	if(chan_id == 0){
		err = counter_get_value(counter_dev, &now_tick);
		printk("Dul Channel_0 value: %d\n", now_tick);
		if (err) {
			printk("Failed to read counter value (err %d)", err);
			return;
		}
	}
	else if(chan_id == 1){
		err = counter_get_value(counter_dev, &now_tick);
		printk("Dul Channel_1 value: %d\n", now_tick);
		if (err) {
			printk("Failed to read counter value (err %d)", err);
			return;
		}
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

	alarm_cfg.flags = 0;
	//16000*31.25 = 500ms
	alarm_cfg.ticks = TICKS;
	alarm_cfg.callback = test_counter_interrupt_fn;
	alarm_cfg.user_data = &alarm_cfg;
	counter_start(counter_dev);
	err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg);
	if (err < 0) {
		printk("counter_set_channel_alarm err \n");
	}
	err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID_1, &alarm_cfg);
	if (err < 0) {
		printk("counter_set_channel_alarm err \n");
	}

	while (1) {
		k_sleep(K_FOREVER);
	}
}

