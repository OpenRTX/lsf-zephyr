/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/gpio.h>
#include <zephyr.h>

#if defined(__ZEPHYR__) && (DT_NODE_HAS_STATUS(DT_ALIAS(avf_audio_pwr_amp), okay))

#define AVF_AUDIO_PWR_AMP	DT_GPIO_LABEL(DT_ALIAS(avf_audio_pwr_amp), enable_gpios)
#define AVF_AUDIO_PWR_AMP_PIN	DT_GPIO_PIN(DT_ALIAS(avf_audio_pwr_amp), enable_gpios)
#define AVF_AUDIO_PWR_AMP_FLAGS DT_GPIO_FLAGS(DT_ALIAS(avf_audio_pwr_amp), enable_gpios)

static inline void audio_pwr_amp_enbale(void)
{

	const struct device *dev;

	dev = device_get_binding(AVF_AUDIO_PWR_AMP);
	if (dev == NULL) {
		return;
	}
	gpio_pin_set(dev, AVF_AUDIO_PWR_AMP_PIN, 1);
}

static inline void audio_pwr_amp_disable(void)
{

	const struct device *dev;

	dev = device_get_binding(AVF_AUDIO_PWR_AMP);
	if (dev == NULL) {
		return;
	}
	gpio_pin_set(dev, AVF_AUDIO_PWR_AMP_PIN, 0);
}

static inline void audio_pwr_amp_init(void)
{
	int ret;
	const struct device *dev;

	dev = device_get_binding(AVF_AUDIO_PWR_AMP);
	if (dev == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev, AVF_AUDIO_PWR_AMP_PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}
}

#else
#warning "The amplifier pins are not defined"

static inline void audio_pwr_amp_enbale(void)
{
}
static inline void audio_pwr_amp_disable(void)
{
}
static inline void audio_pwr_amp_init(void)
{
}

#endif
