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

#define RTC_STACK_SIZE 4096
struct k_thread rtc_thread_data;
K_THREAD_STACK_DEFINE(rtc_stack_area, RTC_STACK_SIZE);

static void sec_counter_callback(const struct device *dev,
				 uint8_t id,
				 uint32_t ticks,
				 void *ud)
{
	printk("Counter alarm callback at %u ms, id %d, ticks %u, ud %p\n",
	       k_uptime_get_32(), id, ticks, ud);
}

void rtc_thread(void *v1, void *v2, void *v3){
	const struct device *rtc = NULL;
	uint32_t ticks = 0;
	printk("======RTC device sample start======\n");

    rtc = DEVICE_DT_GET(DT_NODELABEL(rtc1));
	printk("RTC name: %s\n", rtc->name);

	if (rtc == NULL) {
		printk("RTC: Device is not found.\n");
	}

	if (!device_is_ready(rtc)) {
		printk("RTC: Device is not ready.\n");
	}

	uint32_t rtc_top_value = counter_get_top_value(rtc);
	printk("RTC top value is %ds\n", rtc_top_value);

	counter_start(rtc);			// start counter
	k_msleep(1000);
	counter_get_value(rtc, &ticks);
	printk("RTC value: %ds\n", ticks);

	printk("======RTC device alarm sample start======\n");
	uint32_t now;
	struct counter_alarm_cfg alarm_cfg;
	alarm_cfg.flags = 0;
	alarm_cfg.callback = sec_counter_callback;
	counter_get_value(rtc,&now);
	alarm_cfg.ticks = now + 2;  				// After 2s alarm
	alarm_cfg.user_data = &alarm_cfg;
	counter_set_channel_alarm(rtc, 0, &alarm_cfg);
	k_msleep(3000);
	counter_get_value(rtc, &ticks);
	printk("RTC value: %ds\n", ticks);
	printk("======RTC device alarm sample end======\n");

	printk("======RTC device sample end======\n");
}

void main(void)
{
	int pri = k_thread_priority_get(k_current_get());
	k_thread_create(&rtc_thread_data, rtc_stack_area,
					K_THREAD_STACK_SIZEOF(rtc_stack_area),
					rtc_thread, NULL, NULL, NULL, pri, 0, K_NO_WAIT);
	while (1) {
		k_msleep(50);
	}
}
