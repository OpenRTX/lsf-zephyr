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

#ifdef CONFIG_COUNTER_CSK6_GPTCHANNEL
	#define DEVICE_NAME  DT_LABEL(DT_NODELABEL(gptchannel0))
#elif CONFIG_COUNTER_CSK6_DUAL
	#define DEVICE_NAME "DUL_TIM"
#endif

#define DELAY_US 1000000


uint32_t g_counter = 0;
static void test_counter_interrupt_fn(const struct device *counter_dev, void *user_data)
{
	printk("g_counter: %d\n",g_counter);
	if(g_counter>=10){
		counter_stop(counter_dev);
	}
	g_counter++;
}


void main(void)
{
	const struct device *counter_dev;
	int err;
	struct counter_top_cfg top_cfg;
	printk("Counter alarm sample\n\n");
	counter_dev = device_get_binding(DEVICE_NAME);
	if (counter_dev == NULL) {
		printk("counter device not found\n");
		return;
	}
	top_cfg.flags = 0;
	top_cfg.ticks = counter_us_to_ticks(counter_dev, DELAY_US);
	top_cfg.callback = test_counter_interrupt_fn;
	top_cfg.user_data = NULL;
	err = counter_set_top_value(counter_dev, &top_cfg);
	if (err < 0) {
		printk("counter_set_channel_alarm err %d\n",err);
	}
	while (1)
	{
		g_counter = 0;
		counter_start(counter_dev);
		k_msleep(12000);
		printk("restart\n");
	}	
}
