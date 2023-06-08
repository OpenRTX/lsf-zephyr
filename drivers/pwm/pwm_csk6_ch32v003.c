/*
 * Copyright (c) 2023 listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_ch32v003_pwm

#include "exmcu_comm.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(pwm_csk_ch32v003, CONFIG_PWM_LOG_LEVEL);

struct pwm_csk_ch32v003_config {
	const struct device *exmcu;
	uint32_t clock_frequency;
	uint16_t clock_prescaler;
};

static int pwm_csk_ch32v003_set_cycles(const struct device *dev, uint32_t channel,
				       uint32_t period_cycles, uint32_t pulse_cycles,
				       pwm_flags_t flags)
{
	const struct pwm_csk_ch32v003_config *config = dev->config;

	uint16_t reg_arr = period_cycles;
	uint16_t reg_psc = config->clock_prescaler;
	uint16_t reg_ccp = pulse_cycles;
	int r;

	LOG_INF("reg_arr:%d, reg_psc:%d, reg_ccp:%d\n", reg_arr, reg_psc, reg_ccp);

	r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_ARR_REG_H, reg_arr >> 8);
	if (r) {
		return r;
	}
	r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_ARR_REG_L, reg_arr);
	if (r) {
		return r;
	}
	r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_PSC_REG_H, reg_psc >> 8);
	if (r) {
		return r;
	}
	r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_PSC_REG_L, reg_psc);
	if (r) {
		return r;
	}
	r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_CCP_REG_H, reg_ccp >> 8);
	if (r) {
		return r;
	}
	r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_CCP_REG_L, reg_ccp);
	if (r) {
		return r;
	}

	if (CONFIG_PWM_CSK6_CH32V003_DTG > 0) {
		r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_DTS_REG,
						CONFIG_PWM_CSK6_CH32V003_DTG);
		LOG_INF("PWM DTG: %02X", CONFIG_PWM_CSK6_CH32V003_DTG);
		if (r) {
			return r;
		}
	}

#if CONFIG_PWM_CSK6_CH32V003_BRAKE && CONFIG_PWM_CSK6_CH32V003_DTG
	r = exmcu_register_write_byte(config->exmcu, CH32V003_PWM_BRAKE_REG, CH32V003_PWM_BRAKE_EN);
	if (r) {
		return r;
	}
#endif

	r = exmcu_register_write_byte(
		config->exmcu, CH32V003_PWM_CTRL_REG,
		CH32V003_PWM_EN |
			((CONFIG_PWM_CSK6_CH32V003_DTG) ? CH32V003_PWM_DEADTIME_EN : 0x00));
	if (r) {
		return r;
	}

	return 0;
}

static int pwm_csk_ch32v003_get_cycles_per_sec(const struct device *dev, uint32_t channel,
					       uint64_t *cycles)
{
	const struct pwm_csk_ch32v003_config *config = dev->config;

	*cycles = config->clock_frequency / config->clock_prescaler;

	return 0;
}

static const struct pwm_driver_api pwm_csk_driver_api = {
	.set_cycles = pwm_csk_ch32v003_set_cycles,
	.get_cycles_per_sec = pwm_csk_ch32v003_get_cycles_per_sec,
};

static int pwm_csk_ch32v003_init(const struct device *dev)
{
	const struct pwm_csk_ch32v003_config *config = dev->config;

	LOG_INF("pwm csk ch32v003 init, freq:%d, pres:%d", config->clock_frequency,
		config->clock_prescaler);

	if (!device_is_ready(config->exmcu)) {
		LOG_ERR("device:%s is not ready", config->exmcu->name);
		return -EIO;
	}

	return 0;
}

#define PWM_DEVICE_INIT(index)                                                                     \
                                                                                                   \
	const static struct pwm_csk_ch32v003_config pwm_csk_ch32v003_config##index = {             \
		.clock_prescaler = DT_INST_PROP(index, clock_prescaler),                           \
		.clock_frequency = DT_INST_PROP(index, clock_frequency),                           \
		.exmcu = DEVICE_DT_GET(DT_INST_PARENT(index)),                                     \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(index, &pwm_csk_ch32v003_init, NULL, NULL,                           \
			      &pwm_csk_ch32v003_config##index, POST_KERNEL,                        \
			      CONFIG_PWM_CSK6_CH32V003_INIT_PRIORITY, &pwm_csk_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_DEVICE_INIT)
