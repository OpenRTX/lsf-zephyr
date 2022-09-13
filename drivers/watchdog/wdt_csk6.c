/*
 * Copyright (c) 2022 listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal/Driver_AON_WDT.h"
#include "hal/aon_timer.h"

#include <drivers/watchdog.h>

#define DT_DRV_COMPAT listenai_csk6_watchdog

static void wdt_csk6_irq_handler(void *param);

typedef struct wdt_csk6_data {
	void *wdt;
	wdt_callback_t cb;
} wdt_csk6_data_t;

#define WDT_CSK_DATA(dev) ((wdt_csk6_data_t *)((dev)->data))

static wdt_csk6_data_t wdt_csk6_data = { 0 };

static void wdt_csk6_signal_event(void *p)
{
	if (p != NULL) {
		const struct device *dev = (const struct device *)p;
		const wdt_csk6_data_t *data = WDT_CSK_DATA(dev);

		if (data != NULL && data->cb != NULL) {
			data->cb(dev, 0);
		}
	}
}

static void wdt_csk6_irq_handler(void *param)
{
	extern void AON_TIMER_WDT_SHARE_Handler(void);

	AON_TIMER_WDT_SHARE_Handler();
}

static int wdt_csk6_init(const struct device *dev)
{
	wdt_csk6_data_t *data = WDT_CSK_DATA(dev);
	data->wdt = AON_WDT();
	AON_WDT_Initialize(data->wdt, wdt_csk6_signal_event, (void *)dev);
	AON_WDT_PowerControl(data->wdt, CSK_POWER_FULL);

	uint32_t irq_sta = irq_lock();
	IRQ_CONNECT(DT_IRQ(DT_NODELABEL(wdt), irq), DT_IRQ(DT_NODELABEL(wdt), priority),
		    wdt_csk6_irq_handler, DEVICE_DT_GET(DT_NODELABEL(wdt)), 0);
	irq_enable(DT_IRQ(DT_NODELABEL(wdt), irq));
	irq_unlock(irq_sta);

	return 0;
}

static int wdt_csk6_setup(const struct device *dev, uint8_t options)
{
	wdt_csk6_data_t *data = WDT_CSK_DATA(dev);
	AON_WDT_Enable(data->wdt);
	return 0;
}

static int wdt_csk6_disable(const struct device *dev)
{
	return AON_WDT_Disable(WDT_CSK_DATA(dev)->wdt) == CSK_DRIVER_OK ? 0 : -1;
}

static int wdt_csk6_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg)
{
	wdt_csk6_data_t *data = WDT_CSK_DATA(dev);
	uint32_t aon_wdt_val = 0;
	uint32_t aon_wdt_max_val = AON_WDT_GetMaxValue();

	if (cfg == NULL) {
		return -EINVAL;
	}

	aon_wdt_val = cfg->window.max * CONFIG_WDT_CSK_SOURCE_NUM_PER_MS;

	if ((aon_wdt_val == 0U) || (aon_wdt_val > aon_wdt_max_val)) {
		return -EINVAL;
	}

	AON_WDT_Control(data->wdt, HAL_AON_WDT_TIME_CFG, aon_wdt_val);
	AON_WDT_Control(data->wdt, HAL_AON_WDT_INTERRUPT_EN, 1);
	AON_WDT_Control(data->wdt, HAL_AON_WDT_RESET_PMU_CTRL, reset_pmu_ctrl);

	data->cb = cfg->callback;

	return 0;
}

static int wdt_csk6_feed(const struct device *dev, int channel_id)
{
	return AON_WDT_Refresh(WDT_CSK_DATA(dev)->wdt) == CSK_DRIVER_OK ? 0 : -1;
}

static const struct wdt_driver_api wdt_csk6_api = {
	.setup = wdt_csk6_setup,
	.disable = wdt_csk6_disable,
	.install_timeout = wdt_csk6_install_timeout,
	.feed = wdt_csk6_feed,
};

DEVICE_DT_INST_DEFINE(0, wdt_csk6_init, NULL, &wdt_csk6_data, NULL, PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &wdt_csk6_api);
