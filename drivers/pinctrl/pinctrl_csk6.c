/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pinmux.h>

static int csk6_pin_configure(const struct device *pinctrls, uint8_t pin, uint8_t alt_fun)
{
	pinmux_pin_set(pinctrls, pin, alt_fun);		

	return 0;		
}

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt,
			   uintptr_t reg)
{
	int ret = 0;

	ARG_UNUSED(reg);

	for (uint8_t i = 0U; i < pin_cnt; i++) {
		ret = csk6_pin_configure(pins[i].pinctrls, pins[i].pin, pins[i].alt_func);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}