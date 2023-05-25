/*
 * Copyright (c) 2023 listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_ch32v003_pwm

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(pwm_csk_ch32v003, CONFIG_PWM_LOG_LEVEL);

struct pwm_csk_ch32v003_config {
	struct i2c_dt_spec bus;
	uint32_t clock_frequency;
	uint16_t clock_prescaler;
};

#define CH32V003_ID_REG	  0x01
#define CH32V003_VER_REG  0x02
#define CH32V003_STA_REG  0x03
#define CH32V003_PAGE_REG 0x03

#define CH32V003_BUSY_DELAY_TIMEOUT_MS (5)
#define CH32V003_BUSY_DELAY_CNT	       (3)

#define CH32V003_PWM_CTRL_REG  0x70
#define CH32V003_PWM_ARR_REG_H 0x71
#define CH32V003_PWM_ARR_REG_L 0x72
#define CH32V003_PWM_PSC_REG_H 0x73
#define CH32V003_PWM_PSC_REG_L 0x74
#define CH32V003_PWM_CCP_REG_H 0x75
#define CH32V003_PWM_CCP_REG_L 0x76
#define CH32V003_PWM_DTS_REG   0x77

enum {
	CH32V003_STATUS_INVALID = 0,
	CH32V003_STATUS_BUSY = 1 << 0,
	CH32V003_STATUS_DONE = 1 << 1,
};

static inline int pwm_csk_ch32v003_register_read(const struct device *dev, uint8_t reg,
						 uint8_t *value)
{
	const struct pwm_csk_ch32v003_config *const config = dev->config;
	int r = i2c_reg_read_byte_dt(&config->bus, reg, value);
	LOG_DBG("pwm_csk_ch32v003_register_read, name:%s, reg=0x%02x, value=%d", dev->name, reg,
		*value);

	return r;
}

static inline int pwm_csk_ch32v003_register_write(const struct device *dev, uint8_t reg,
						  uint8_t value)
{
	const struct pwm_csk_ch32v003_config *const config = dev->config;
	int r;
	uint8_t busy = 0x00;
	uint8_t cnt = 0;

	LOG_DBG("pwm_csk_ch32v003_register_write, reg=0x%02x, value=0x%02x", reg, value);

	while (1) {
		r = pwm_csk_ch32v003_register_read(dev, CH32V003_STA_REG, &busy);
		if (r) {
			return r;
		}

		if (busy & CH32V003_STATUS_BUSY) {
			k_msleep(CH32V003_BUSY_DELAY_TIMEOUT_MS);
		} else {
			break;
		}
		cnt++;

		if (cnt >= CH32V003_BUSY_DELAY_CNT) {
			LOG_ERR("wait busy timeout");
			return -EBUSY;
		}
	}

	r = i2c_reg_write_byte_dt(&config->bus, reg, value);
	if (r) {
		return r;
	}

	return 0;
}

static int pwm_csk_ch32v003_set_cycles(const struct device *dev, uint32_t channel,
				       uint32_t period_cycles, uint32_t pulse_cycles,
				       pwm_flags_t flags)
{
    const struct pwm_csk_ch32v003_config *const config = dev->config;

	uint16_t reg_arr = period_cycles;
	uint16_t reg_psc = config->clock_prescaler;
	uint16_t reg_ccp = pulse_cycles;
	int r;

	LOG_INF("reg_arr:%d, reg_psc:%d, reg_ccp:%d\n", reg_arr, reg_psc, reg_ccp);

	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_ARR_REG_H, reg_arr >> 8);
	if (r) {
		return r;
	}
	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_ARR_REG_L, reg_arr);
	if (r) {
		return r;
	}
	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_PSC_REG_H, reg_psc >> 8);
	if (r) {
		return r;
	}
	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_PSC_REG_L, reg_psc);
	if (r) {
		return r;
	}
	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_CCP_REG_H, reg_ccp >> 8);
	if (r) {
		return r;
	}
	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_CCP_REG_L, reg_ccp);
	if (r) {
		return r;
	}

	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_DTS_REG, CONFIG_PWM_CSK6_CH32V003_DTG);
	if (r) {
		return r;
	}
	r = pwm_csk_ch32v003_register_write(dev, CH32V003_PWM_CTRL_REG, 0x01);
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
	int r;

	LOG_INF("pwm csk ch32v003 init, freq:%d, pres:%d", config->clock_frequency,
		config->clock_prescaler);

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("device:%s is not ready", config->bus.bus->name);
	}

	r = i2c_configure(config->bus.bus, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER);
	if (r) {
		printk("i2c_configure failed, ret:%d\n", r);
		return r;
	}

	uint8_t id = 0;
	r = pwm_csk_ch32v003_register_read(dev, CH32V003_ID_REG, &id);
	if (r) {
		LOG_ERR("read id failed, r:%d\n", r);
		return r;
	}

	return 0;
}

#define PWM_DEVICE_INIT(index)                                                                     \
                                                                                                   \
	static const struct pwm_csk_ch32v003_config pwm_csk_ch32v003_config##index = {             \
		.clock_prescaler = DT_INST_PROP(index, clock_prescaler),                           \
		.clock_frequency = DT_INST_PROP(index, clock_frequency),                           \
		.bus = I2C_DT_SPEC_GET(DT_INST_PARENT(index)),                                     \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(index, &pwm_csk_ch32v003_init, NULL, NULL,                           \
			      &pwm_csk_ch32v003_config##index, POST_KERNEL,                        \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &pwm_csk_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_DEVICE_INIT)
