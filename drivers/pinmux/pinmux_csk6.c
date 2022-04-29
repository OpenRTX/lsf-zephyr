/*
 * Copyright (c) 2021 listenai Systems (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_pinmux

#include <drivers/pinmux.h>
#include <hal/IOMuxManager.h>

#define DEV_CFG(dev) ((const struct pinmux_csk_config *const)(dev)->config)

struct pinmux_csk_config {
	/* pinmux port control registe */
	uintptr_t reg_addr;
};

static int pinmux_set(const struct device *dev, uint32_t pin_num, uint32_t func)
{
	ARG_UNUSED(dev);
	uint8_t pad;
	const struct pinmux_csk_config *config = DEV_CFG(dev);
	if (IOMuxManager_GetPad(config->reg_addr, &pad)) {
		return -EINVAL;
	}
	if (IOMuxManager_PinConfigure(pad, pin_num, func)) {
		return -EINVAL;
	}

	return 0;
}

static int pinmux_get(const struct device *dev, uint32_t pin_num, uint32_t *func)
{
	ARG_UNUSED(dev);
	uint8_t pad;
	const struct pinmux_csk_config *config = DEV_CFG(dev);

	if (IOMuxManager_GetPad(config->reg_addr, &pad)) {
		return -EINVAL;
	}
	if (IOMuxManager_GetConfig(pad, pin_num, func)) {
		return -EINVAL;
	}

	return 0;
}

static int pinmux_pullup(const struct device *dev, uint32_t pin_num, uint8_t func)
{
	ARG_UNUSED(dev);
	uint8_t pad;
	const struct pinmux_csk_config *config = DEV_CFG(dev);

	if (IOMuxManager_GetPad(config->reg_addr, &pad)) {
		return -EINVAL;
	}

	//If necessary, can add PULLDOWN_MODE 
	switch (func) {
	case PINMUX_PULLUP_DISABLE:
		return IOMuxManager_ModeConfigure(pad, pin_num, HAL_IOMUX_NONE_MODE);
		break;
	case PINMUX_PULLUP_ENABLE:
		return IOMuxManager_ModeConfigure(pad, pin_num, HAL_IOMUX_PULLUP_MODE);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int pinmux_input(const struct device *dev, uint32_t pin_num, uint8_t func)
{
	/* no support */
	return -ENOTSUP;
}

static struct pinmux_driver_api csk_pinmux_drv_api = {
	.set = pinmux_set,
	.get = pinmux_get,
	.pullup = pinmux_pullup,
	.input = pinmux_input,
};

static int csk_pinmux_initialize(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

/* Initialize using PRE_KERNEL_1 priority so that GPIO can use the pin_num
 * mux driver.
 */
#define PINMUX_CSK_INIT(index)                                                                     \
	static const struct pinmux_csk_config pinmux_csk_config##index = {                         \
		.reg_addr = DT_INST_REG_ADDR(index),                                               \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(index, &csk_pinmux_initialize, NULL, NULL,                           \
			      &pinmux_csk_config##index, PRE_KERNEL_1,                             \
			      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &csk_pinmux_drv_api);

DT_INST_FOREACH_STATUS_OKAY(PINMUX_CSK_INIT)
