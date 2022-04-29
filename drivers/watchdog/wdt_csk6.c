/*
 * Copyright (c) 2022 listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/watchdog.h>
#include "hal/wdt.h"

#define DT_DRV_COMPAT listenai_csk6_watchdog

static void wdt_csk6_irq_handler(void *param);

enum {
	WDT_CSK6_MODE_INT,
	WDT_CSK6_MODE_RST,
};

typedef struct wdt_csk6_data {
	wdt_callback_t cb;
} wdt_csk6_data_t;

#define WDT_CSK_CONFIG(dev) ((WDT_RESOURCES *)((dev)->config))

#define WDT_CSK_DATA(dev) ((wdt_csk6_data_t *)((dev)->data))

static WDT_INFO wdt0_info = { 0 };
static const WDT_RESOURCES wdt0_resources = { VENUS_WDT0, IRQ_WDT_VECTOR, NULL, &wdt0_info };
static wdt_csk6_data_t wdt_csk6_data = { 0 };

// 1 / 32000 * 1000
#define WDT_CSK6_CLK_PERIOD_US (31)

static void wdt_csk6_irq_handler(void *param)
{
	const struct device *dev = (const struct device *)param;
	const wdt_csk6_data_t *data = WDT_CSK_DATA(dev);

	if (data != NULL && data->cb != NULL) {
		data->cb(dev, 0);
	}
}

static int wdt_csk6_init(const struct device *dev)
{
	WDT_RESOURCES *wdt = WDT_CSK_CONFIG(dev);
	uint32_t irq_sta = irq_lock();

	WDT_Initialize((void *)wdt);
	WDT_Disable((void *)wdt);

	IRQ_CONNECT(DT_IRQ(DT_NODELABEL(wdt), irq), DT_IRQ(DT_NODELABEL(wdt), priority),
		    wdt_csk6_irq_handler, DEVICE_DT_GET(DT_NODELABEL(wdt)), 0);
	irq_enable(DT_IRQ(DT_NODELABEL(wdt), irq));
	irq_unlock(irq_sta);

	return 0;
}

static int wdt_csk6_setup(const struct device *dev, uint8_t options)
{
	return 0;
}

static int wdt_csk6_disable(const struct device *dev)
{
	return WDT_Disable((void *)WDT_CSK_CONFIG(dev)) == CSK_DRIVER_OK ? 0 : -1;
}

static uint8_t wdt_csk6_timeout_to_shift(uint32_t timeout, uint8_t mode, uint32_t *out)
{
	uint8_t left;
	uint8_t right;
	uint8_t mid;
	uint32_t res;

	// See at Driver_WDT.h
	const uint8_t inttime_table[16] = { 6,	8,  10, 11, 12, 13, 14, 15,
					    17, 19, 21, 23, 25, 27, 29, 31 };
	const uint8_t rsttime_table[8] = { 7, 8, 9, 10, 11, 12, 13, 14 };

	mid = left = 0;
	right = mode == WDT_CSK6_MODE_INT ? 15 : 7;

	// convert to microseconds to avoid floating point arithmetic
	timeout *= 1000;

	// match a suitable value
	while (left <= right) {
		mid = (right - left) / 2 + left;
		uint8_t val =
			mode == WDT_CSK6_MODE_INT ? inttime_table[mid] : rsttime_table[mid];
		res = (1 << val) * WDT_CSK6_CLK_PERIOD_US;

		if (res >= timeout) {
			if (mid == 0) {
				break;
			}
			val = mode == WDT_CSK6_MODE_INT ? inttime_table[mid - 1] :
								   rsttime_table[mid - 1];
			if (((1 << val) * WDT_CSK6_CLK_PERIOD_US) < timeout) {
				break;
			}
			right = mid - 1;
		} else {
			left = mid + 1;
		}
	}

	*out = res;

	return mid;
}

static int wdt_csk6_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg)
{
	uint32_t ctrl_val = 0U;
	uint32_t compound_mask = 0U;
	uint32_t inttime = 0;
	uint32_t res = 0;

	WDT_RESOURCES *wdt = WDT_CSK_CONFIG(dev);
	wdt_csk6_data_t *data = WDT_CSK_DATA(dev);

	if (dev == NULL || cfg == NULL) {
		return -EINVAL;
	}

	WDT_PowerControl((void *)wdt, CSK_POWER_FULL);

	data->cb = cfg->callback;

	if (cfg->flags != WDT_FLAG_RESET_NONE) {
		// rst en
		ctrl_val |= (0x1 << 3);
		// set a fix value here. 0.5s reset
		ctrl_val |= (14 << CSK_WDT_RSTTIME_Pos);
		// int en
		ctrl_val |= cfg->callback != NULL ? (0x1 << 2) : 0;
		// time set
		inttime = wdt_csk6_timeout_to_shift(cfg->window.max, WDT_CSK6_MODE_INT, &res);
		ctrl_val |= (inttime << CSK_WDT_INTTIME_Pos);
	} else {
		if (cfg->callback == NULL) {
			return -EINVAL;
		}
		// int en
		ctrl_val |= (0x1 << 2);
		// time set
		inttime = wdt_csk6_timeout_to_shift(cfg->window.max, WDT_CSK6_MODE_INT, &res);
		ctrl_val |= (inttime << CSK_WDT_INTTIME_Pos);
	}

	//en
	ctrl_val |= 0x1;

	compound_mask = BITSET_0X010_RSTEN | BITSET_0X010_INTEN | BITSET_0X010_EN;
	// enable write and write control register
	wdt->reg->REG_WREN.all = WDT_MAGIC_WRITE_PROTECTION;
	wdt->reg->REG_CTRL.all = (wdt->reg->REG_CTRL.all & (~compound_mask)) | ctrl_val;

	return res / 1000;
}

static int wdt_csk6_feed(const struct device *dev, int channel_id)
{
	return WDT_Refresh((void *)WDT_CSK_CONFIG(dev)) == CSK_DRIVER_OK ? 0 : -1;
}

static const struct wdt_driver_api wdt_csk6_api = {
	.setup = wdt_csk6_setup,
	.disable = wdt_csk6_disable,
	.install_timeout = wdt_csk6_install_timeout,
	.feed = wdt_csk6_feed,
};

DEVICE_DT_INST_DEFINE(0, wdt_csk6_init, NULL, &wdt_csk6_data, &wdt0_resources, PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &wdt_csk6_api);