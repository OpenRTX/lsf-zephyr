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

#define DELAY_US 1000000
#define ALARM_CHANNEL_ID_0 0
#define ALARM_CHANNEL_ID_1 1
struct counter_alarm_cfg alarm_cfg;
#ifdef CONFIG_COUNTER_CSK6_GPT
	#define TIMER DT_LABEL(DT_NODELABEL(gpt0))
#elif CONFIG_COUNTER_CSK6_DUAL
	#define TIMER DT_LABEL(DT_NODELABEL(dual_timer))
#endif

static void test_counter_interrupt_fn(const struct device *counter_dev, uint8_t chan_id,
				      uint32_t ticks, void *user_data)
{
	printk("counter Channel is: %d\n", chan_id);

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
	alarm_cfg.ticks = counter_us_to_ticks(counter_dev, DELAY_US);
	alarm_cfg.callback = test_counter_interrupt_fn;
	alarm_cfg.user_data = &alarm_cfg;
	counter_start(counter_dev);
	err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID_0, &alarm_cfg);
	if (err < 0) {
		printk("counter_set_channel_alarm err \n");
	}
	err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID_1, &alarm_cfg);
	if (err < 0) {
		printk("counter_set_channel_alarm err \n");
	}

}
