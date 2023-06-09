/*
 * Copyright (c) 2021 Abel Sensors
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include "gpio_utils.h"
#include "exmcu_comm.h"

#define DT_DRV_COMPAT listenai_csk_gpio_ch32003_port

LOG_MODULE_REGISTER(gpio_ch32003, CONFIG_GPIO_LOG_LEVEL);


#define SUPPORTED_FLAGS                                                                            \
	(GPIO_INPUT | GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW | GPIO_OUTPUT_INIT_HIGH |                 \
	 GPIO_PULL_DOWN | GPIO_PULL_UP | GPIO_ACTIVE_HIGH | GPIO_ACTIVE_LOW)

struct gpio_ch32003_config {
	struct gpio_driver_config common;
	const struct device *exmcu;
	uint8_t id;
};

struct gpio_ch32003_drv_data {
	struct gpio_driver_data common;
	struct {
		uint8_t input;
		uint8_t output;
		uint8_t dir;
		uint8_t pull_mode;
		uint8_t pull_en;
		uint8_t od;
	} reg_cache;

	struct k_sem lock;
};

static inline int ch32v003_dir_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;

	int r = exmcu_register_write_byte(config->exmcu, CH32V003_DIR_REG(config->id), flag);

	if (r == 0) {
		drv_data->reg_cache.dir = flag;
	}

	return r;
}

static inline int ch32v003_dir_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;

	int r = exmcu_register_read_byte(config->exmcu, CH32V003_DIR_REG(config->id), flag);
	if (r == 0) {
		drv_data->reg_cache.dir = *flag;
	}

	return r;
}

static inline int ch32v003_od_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;

	int r = exmcu_register_write_byte(config->exmcu, CH32V003_OD_REG(config->id), flag);
	if (r == 0) {
		drv_data->reg_cache.od = flag;
	}
	return r;
}

static inline int ch32v003_od_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return exmcu_register_read_byte(config->exmcu, CH32V003_OD_REG(config->id), flag);
}

static inline int ch32v003_out_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;
	int r = exmcu_register_write_byte(config->exmcu, CH32V003_OUT_REG(config->id), flag);
	if (r == 0) {
		drv_data->reg_cache.output = flag;
	}
	return r;
}

static inline int ch32v003_out_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return exmcu_register_read_byte(config->exmcu, CH32V003_OUT_REG(config->id), flag);
}

static inline int ch32v003_pull_mode_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return exmcu_register_write_byte(config->exmcu, CH32V003_PULL_CTRL_REG(config->id), flag);
}

static inline int ch32v003_pull_mode_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return exmcu_register_read_byte(config->exmcu, CH32V003_PULL_CTRL_REG(config->id), flag);
}

static inline int ch32v003_pull_en_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return exmcu_register_write_byte(config->exmcu, CH32V003_PULL_EN_REG(config->id), flag);
}

static inline int ch32v003_pull_en_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return exmcu_register_read_byte(config->exmcu, CH32V003_PULL_EN_REG(config->id), flag);
}

static inline int ch32v003_in_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;
	int r = exmcu_register_read_byte(config->exmcu, CH32V003_IN_REG(config->id), flag);

	if (r == 0) {
		drv_data->reg_cache.input = *flag;
	}

	return r;
}

int gpio_ch32003_init(const struct device *dev)
{
	const struct gpio_ch32003_config *const config = dev->config;

	if (!device_is_ready(config->exmcu)) {
		LOG_ERR("%s is not ready", config->exmcu->name);
		return -ENODEV;
	}

	return 0;
}

static int ch32v003_pin_config(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
	struct gpio_ch32003_drv_data *data = dev->data;
	uint8_t dir = data->reg_cache.dir;
	uint8_t output = data->reg_cache.output;
	uint8_t pull_mode = data->reg_cache.pull_mode;
	uint8_t pull_en = data->reg_cache.pull_en;
	uint8_t dir_update = false;
	uint8_t out_update = false;
	uint8_t pull_update = false;
	int r;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&data->lock, K_FOREVER);
	LOG_DBG("ch32v003_pin_config");
	if (flags & GPIO_OUTPUT) {
		dir |= BIT(pin);
		dir_update = true;
		if (flags & GPIO_OUTPUT_INIT_HIGH) {
			output &= ~BIT(pin);
			out_update = true;
		} else if (flags & GPIO_OUTPUT_INIT_LOW) {
			output |= BIT(pin);
			out_update = true;
		}
	} else if (flags & GPIO_INPUT) {
		dir_update = true;
		dir &= ~BIT(pin);
	}

	if ((flags & GPIO_PULL_UP) || (flags & GPIO_PULL_DOWN)) {
		pull_update = true;
		if (flags & GPIO_PULL_UP) {
			pull_mode |= BIT(pin);
			pull_en |= BIT(pin);
		} else if (flags & GPIO_PULL_DOWN) {
			pull_mode &= ~BIT(pin);
			pull_en |= BIT(pin);
		}
	}

	if (dir_update) {
		r = ch32v003_dir_set(dev, dir);
		if (r != 0) {
			k_sem_give(&data->lock);
			return r;
		}
	}

	if (out_update) {
		r = ch32v003_od_set(dev, 0x00);
		if (r != 0) {
			k_sem_give(&data->lock);
			return r;
		}

		r = ch32v003_out_set(dev, output);
		if (r != 0) {
			k_sem_give(&data->lock);
			return r;
		}
	}

	if (pull_update) {
		r = ch32v003_pull_en_set(dev, pull_en);
		if (r != 0) {
			k_sem_give(&data->lock);
			return r;
		}

		r = ch32v003_pull_mode_set(dev, pull_mode);
		if (r != 0) {
			k_sem_give(&data->lock);
			return r;
		}
	}

	k_sem_give(&data->lock);

	return r;
}

static int ch32v003_set_masked_raw(const struct device *dev, gpio_port_pins_t mask,
				   gpio_port_value_t value)
{
	struct gpio_ch32003_drv_data *data = dev->data;
	uint8_t output;
	int r;

	k_sem_take(&data->lock, K_FOREVER);
	output = data->reg_cache.output;
	mask &= 0xFF;
	value &= 0xFF;
	output = (output & ~mask) | (value & mask);

	r = ch32v003_out_set(dev, output);
	LOG_DBG("ch32v003_set_masked_raw, output:%02X", output);
	k_sem_give(&data->lock);

	return r;
}

static int ch32v003_set_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	struct gpio_ch32003_drv_data *data = dev->data;
	uint8_t output;
	int r;

	k_sem_take(&data->lock, K_FOREVER);
	output = data->reg_cache.output;
	output |= (pins & 0xFF);

	r = ch32v003_out_set(dev, output);
	LOG_DBG("ch32v003_set_bits_raw, output:%02X", output);
	k_sem_give(&data->lock);

	return r;
}

static int ch32v003_clear_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	struct gpio_ch32003_drv_data *data = dev->data;
	uint8_t output;
	int r;

	k_sem_take(&data->lock, K_FOREVER);
	output = data->reg_cache.output;
	output &= ~(pins & 0xFF);
	LOG_DBG("ch32v003_clear_bits_raw, output:%02X", output);
	r = ch32v003_out_set(dev, output);
	k_sem_give(&data->lock);

	return r;
}

static int ch32v003_toggle_bits(const struct device *dev, gpio_port_pins_t pins)
{
	struct gpio_ch32003_drv_data *data = dev->data;
	uint8_t output;
	int r;

	k_sem_take(&data->lock, K_FOREVER);
	output = data->reg_cache.output;
	LOG_DBG("ch32v003_toggle_bits, cache output:%02X", output);
	output ^= (pins & 0xFF);
	LOG_DBG("ch32v003_toggle_bits, output:%02X", output);
	r = ch32v003_out_set(dev, output);
	k_sem_give(&data->lock);

	return r;
}

static int ch32v003_get_raw(const struct device *dev, uint32_t *value)
{
	struct gpio_ch32003_drv_data *data = dev->data;
	int r;

	k_sem_take(&data->lock, K_FOREVER);
	uint8_t sta;
	r = ch32v003_in_get(dev, &sta);
	*value = sta;
	LOG_DBG("ch32v003_get_raw, sta:%02X, value:%02X", sta, *value);
	k_sem_give(&data->lock);

	return r;
}

static const struct gpio_driver_api gpio_ch32003_driver = {
	.pin_configure = ch32v003_pin_config,
	.port_get_raw = ch32v003_get_raw,
	.port_set_masked_raw = ch32v003_set_masked_raw,
	.port_set_bits_raw = ch32v003_set_bits_raw,
	.port_clear_bits_raw = ch32v003_clear_bits_raw,
	.port_toggle_bits = ch32v003_toggle_bits,
	.pin_interrupt_configure = NULL,
};

#define GPIO_DEVICE_INSTANCE(inst)                                                                 \
	static const struct gpio_ch32003_config gpio_ch32003_##inst##_cfg = {                      \
		.common =                                                                          \
			{                                                                          \
				.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(inst),               \
			},                                                                         \
		.exmcu = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                     \
		.id = inst,                                                                        \
	};                                                                                         \
                                                                                                   \
	static struct gpio_ch32003_drv_data gpio_ch32003_##inst##_drvdata = {                      \
		.reg_cache = {.input = 0x0,                                                        \
			      .output = 0x00,                                                      \
			      .dir = 0x0,                                                          \
			      .pull_mode = 0xFF,                                                   \
			      .od = 0x00,                                                          \
			      .pull_en = 0x0},                                                     \
		.lock = Z_SEM_INITIALIZER(gpio_ch32003_##inst##_drvdata.lock, 1, 1),               \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, gpio_ch32003_init, NULL, &gpio_ch32003_##inst##_drvdata,       \
			      &gpio_ch32003_##inst##_cfg, POST_KERNEL, CONFIG_GPIO_CSK6_CH32V003_INIT_PRIORITY, &gpio_ch32003_driver);

DT_INST_FOREACH_STATUS_OKAY(GPIO_DEVICE_INSTANCE);
