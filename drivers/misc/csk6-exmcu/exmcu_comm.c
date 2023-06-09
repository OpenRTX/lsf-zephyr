/*
 * Copyright (c) 2023 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "exmcu_comm.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(csk6_exmcu_i2c);

K_MUTEX_DEFINE(exmcu_lock);

struct exmcu_config {
	struct i2c_dt_spec bus;
};

typedef struct exmcu_info {
	uint8_t id;
	const char *name;
} exmcu_info_t;

const static exmcu_info_t exmcu_infos[] = {
	{
		.id = 0x01,
		.name = "ch32v003",
	},
};

static void print_exmcu_info(uint8_t ver, uint8_t id)
{
	uint8_t i;
	uint8_t found = 0;

	const exmcu_info_t *info = &exmcu_infos[0];

	for (i = 0; i < sizeof(exmcu_infos) / sizeof(exmcu_infos[0]); i++) {
		if (info == NULL) {
			break;
		}

		if (info->id == id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		LOG_WRN("exmcu info not found, v%d.%d, id:%d", ver / 10, ver % 10, id);
	} else {
		LOG_INF("exmcu info, chip type:%s, ver:%d.%d", info->name, ver / 10, ver % 10);
		if (EXMCU_PROTOCOL_VERSION != ver) {
			LOG_WRN("exmcu software version(v%d.%d) is not same as exmcu hardware "
				"version(v%d.%d)",
				EXMCU_PROTOCOL_VERSION / 10, EXMCU_PROTOCOL_VERSION % 10, ver / 10,
				ver % 10);
		}
	}
}

#ifdef CONFIG_CSK6_CH32V003_NON_BLOCK_INIT
static K_EVENT_DEFINE(event_group);
#define INIT_BIT BIT(0)
static struct k_work_delayable init_work;
static const struct device *s_init_dev = NULL;

static int wait_init_done(void)
{
	int ret = k_event_wait(&event_group, INIT_BIT, false, K_FOREVER);
	if (!(ret & INIT_BIT)) {
		return -1;
	}

	return 0;
}

static void init_work_handler(struct k_work *item)
{
	ARG_UNUSED(item);
	uint8_t id = 0;
	uint8_t ver = 0;
	const struct device *dev = s_init_dev;
	int r;
	const struct exmcu_config *config = dev->config;

	/* Do not call exmcu_register_read_byte, otherwise a deadlock may occur. */
	r = i2c_reg_read_byte_dt(&config->bus, CH32V003_ID_REG, &id);
	if (r) {
		LOG_ERR("exmcu read id failed, r:%d\n", r);
	}

	r = i2c_reg_read_byte_dt(&config->bus, CH32V003_VER_REG, &ver);
	if (r) {
		LOG_ERR("exmcu read ver failed, r:%d\n", r);
	}

	print_exmcu_info(ver, id);

	k_event_set(&event_group, INIT_BIT);
}

#endif

int exmcu_register_read_byte(const struct device *dev, uint8_t reg, uint8_t *value)
{
	int r;
	const struct exmcu_config *config = dev->config;

#ifdef CONFIG_CSK6_CH32V003_NON_BLOCK_INIT
	wait_init_done();
#endif

	k_mutex_lock(&exmcu_lock, K_FOREVER);
	r = i2c_reg_read_byte_dt(&config->bus, reg, value);
	LOG_DBG("reg:0x%02X, value:0x%02X", reg, *value);
	k_mutex_unlock(&exmcu_lock);

	return r;
}

int exmcu_register_write_byte(const struct device *dev, uint8_t reg, uint8_t value)
{
	int r;
	uint8_t busy = 0x00;
	uint8_t cnt = 0;
	const struct exmcu_config *config = dev->config;

#ifdef CONFIG_CSK6_CH32V003_NON_BLOCK_INIT
	wait_init_done();
#endif

	k_mutex_lock(&exmcu_lock, K_FOREVER);

	r = 0;
	cnt = 0;
	while (1) {
		LOG_DBG("READ BUSY");
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

	LOG_DBG("reg:0x%02X, 0x%02X\n", reg, value);
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
	const struct exmcu_config *config = dev->config;

	LOG_INF("exmcu addr:0x%02X", config->bus.addr);

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("device: %s is not ready, ", config->bus.bus->name);
		return -EIO;
	}

	r = i2c_configure(config->bus.bus, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_MASTER);
	if (r) {
		LOG_ERR("exmcu i2c_configure failed, return value: %d", r);
		return r;
	}

	/* wait exmcu startup */
#ifdef CONFIG_CSK6_CH32V003_NON_BLOCK_INIT
	s_init_dev = dev;
	k_work_init_delayable(&init_work, init_work_handler);
	k_work_schedule(&init_work, K_MSEC(CONFIG_CSK6_CH32V003_INIT_DELAY_MS));
#else
	k_msleep(CONFIG_CSK6_CH32V003_INIT_DELAY_MS);
	uint8_t id = 0;
	uint8_t ver = 0;

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

	print_exmcu_info(ver, id);
#endif

	return 0;
}

#if DT_NODE_EXISTS(DT_ALIAS(csk6_exmcu))

const static struct exmcu_config exmcu_config = {
	.bus = I2C_DT_SPEC_GET(DT_ALIAS(csk6_exmcu)),
};

DEVICE_DT_DEFINE(DT_ALIAS(csk6_exmcu), exmcu_init, NULL, NULL, &exmcu_config, POST_KERNEL,
		 CONFIG_CSK6_EXMCU_INIT_PRIORITY, NULL);

#endif
