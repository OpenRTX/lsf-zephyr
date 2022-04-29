/*
 * Copyright (c) 2022 listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/watchdog.h>
#include <sys/printk.h>
#include <stdbool.h>

#define WDT_FEED_TRIES 10

#ifndef WDT_MAX_WINDOW
#define WDT_MAX_WINDOW 16000
#endif

#define WDT_ALLOW_CALLBACK 1

#if WDT_ALLOW_CALLBACK
static void wdt_callback(const struct device *wdt_dev, int channel_id)
{	
	static bool is_handled = false;

	if (!is_handled) {
		is_handled = !is_handled;
		wdt_feed(wdt_dev, channel_id);
		printk("Handled watchdog interrupt and feed the dog.System will reset after 16s.\n");
	} else {
		printk("Handled watchdog interrupt again, wait for reset.\n");
	}
}
#endif /* WDT_ALLOW_CALLBACK */

void main(void)
{
	int err;
	int wdt_channel_id;
	const struct device *wdt;

	printk("Watchdog sample application, %s %s\n", __DATE__, __TIME__);

	wdt = DEVICE_DT_GET(DT_NODELABEL(wdt));
	if (!wdt) {
		printk("Cannot get WDT device\n");
		return;
	}

	struct wdt_timeout_cfg wdt_config = {
		.flags = WDT_FLAG_RESET_SOC,
		.window.min = 0,
		.window.max = WDT_MAX_WINDOW,
	};

#if WDT_ALLOW_CALLBACK
	/* Set up watchdog callback. */
	wdt_config.callback = wdt_callback;
	printk("Attempting to test pre-reset callback, %p\n", wdt_config.callback);
#else /* WDT_ALLOW_CALLBACK */
	printk("Callback in RESET_SOC disabled for this platform\n");
#endif /* WDT_ALLOW_CALLBACK */

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	printk("wdt_channel_id=%d.\n", wdt_channel_id);
	if (wdt_channel_id < 0) {
		printk("Watchdog install error\n");
		return;
	}

	err = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (err < 0) {
		printk("Watchdog setup error\n");
		return;
	}

	/* Feeding watchdog. */
	uint32_t count = 0;
	printk("Feeding watchdog %d times\n", WDT_FEED_TRIES);
	for (int i = 0; i < WDT_FEED_TRIES; ++i) {
		k_msleep(1000);
		printk("Waiting for feed watchdog, %ds...\n", ++count);
	}
	wdt_feed(wdt, wdt_channel_id);
	/* Waiting for the SoC reset. */
	count = 0;
	while (1) {
		k_msleep(1000);
		printk("Waiting for reset, %ds...\n", ++count);
	}
}
