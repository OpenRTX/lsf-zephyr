/*
 * Copyright (c) 2023 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#include <logging/log.h>

#include "exmcu_comm.h"

LOG_MODULE_REGISTER(csk6_exmcu_i2c);


K_MUTEX_DEFINE(exmcu_lock);

struct exmcu_config {
	struct i2c_dt_spec bus;
};

int exmcu_register_read_byte(const struct device *dev, uint8_t reg, uint8_t *value)
{
	int r;
	const struct exmcu_config *config = dev->config;

	k_mutex_lock(&exmcu_lock, K_FOREVER);
	r = i2c_reg_read_byte_dt(&config->bus, reg, value);
	k_mutex_unlock(&exmcu_lock);

	return r;
}

int exmcu_register_write_byte(const struct device *dev, uint8_t reg, uint8_t value)
{
	int r;
	uint8_t busy = 0x00;
	uint8_t cnt = 0;
	const struct exmcu_config *config = dev->config;

	k_mutex_lock(&exmcu_lock, K_FOREVER);

	r = 0;

	while (1) {
		r = exmcu_register_read_byte(dev, CH32V003_STA_REG, &busy);
		if (r) {
			break;
			;
		}

		if (busy & CH32V003_STATUS_BUSY) {
			k_msleep(CH32V003_BUSY_DELAY_TIMEOUT_MS);
		} else {
			break;
		}
		cnt++;

		if (cnt >= CH32V003_BUSY_DELAY_CNT) {
			LOG_ERR("wait busy timeout");
			r = -EBUSY;
			break;
		}
	}

	if (r) {
		k_mutex_unlock(&exmcu_lock);
		return r;
	}

	r = i2c_reg_write_byte_dt(&config->bus, reg, value);
	k_mutex_unlock(&exmcu_lock);

	if (r) {
		return r;
	}

	return 0;
}

int exmcu_init(const struct device *dev)
{
	int r;
	uint8_t id = 0;
	uint8_t ver = 0;
	const struct exmcu_config *config = dev->config;

	printk("i2c addr:0x%02X", config->bus.addr);

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("device: %s is not ready, ", config->bus.bus->name);
		return -EIO;
	}

	r = i2c_configure(config->bus.bus, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_MASTER);
	if (r) {
		LOG_ERR("exmcu i2c_configure failed, return value: %d", r);
		return r;
	}

	r = exmcu_register_read_byte(dev, CH32V003_ID_REG, &id);
	if (r) {
		LOG_ERR("exmcu read id failed, r:%d\n", r);
		return r;
	}

	r = exmcu_register_read_byte(dev, CH32V003_VER_REG, &ver);
	if (r) {
		LOG_ERR("exmcu read ver failed, r:%d\n", r);
		return r;
	}

	LOG_INF("exmcu info, id:%d, ver:%d\n", id, ver);

	return 0;
}

#if DT_NODE_EXISTS(DT_ALIAS(csk6_exmcu))

const static struct exmcu_config exmcu_config = {
	.bus = I2C_DT_SPEC_GET(DT_ALIAS(csk6_exmcu)),
};

DEVICE_DT_DEFINE(DT_ALIAS(csk6_exmcu), exmcu_init, NULL, NULL, &exmcu_config, POST_KERNEL,
		 CONFIG_CSK6_EXMCU_INIT_PRIORITY, NULL);

#endif
