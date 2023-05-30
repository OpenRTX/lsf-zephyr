/*
 * Copyright (c) 2021 Abel Sensors
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include "gpio_utils.h"

#define DT_DRV_COMPAT listenai_csk_gpio_ch32003_port

LOG_MODULE_REGISTER(gpio_ch32003, CONFIG_GPIO_LOG_LEVEL);

#define CH32V003_DIR_REG_BASE	    0x10
#define CH32V003_OUT_REG_BASE	    0x11
#define CH32V003_OD_REG_BASE	    0x12
#define CH32V003_IN_REG_BASE	    0x13
#define CH32V003_PULL_UP_REG_BASE   0x14
#define CH32V003_PULL_DWON_REG_BASE 0x15
#define CH32V003_INT_STA_REG_BASE   0x16
#define CH32V003_INT_EN_REG_BASE    0x17
#define CH32V003_INT_CTL_REG_BASE   0x18

#define CH32V003_DIR_REG(index)	      ((CH32V003_DIR_REG_BASE) + (index * 0x10))
#define CH32V003_OUT_REG(index)	      ((CH32V003_OUT_REG_BASE) + (index * 0x10))
#define CH32V003_OD_REG(index)	      ((CH32V003_OD_REG_BASE) + (index * 0x10))
#define CH32V003_IN_REG(index)	      ((CH32V003_IN_REG_BASE) + (index * 0x10))
#define CH32V003_PULL_UP_REG(index)   ((CH32V003_PULL_UP_REG_BASE) + (index * 0x10))
#define CH32V003_PULL_DWON_REG(index) ((CH32V003_PULL_DWON_REG_BASE) + (index * 0x10))
#define CH32V003_INT_STA_REG(index)   ((CH32V003_INT_STA_REG_BASE) + (index * 0x10))
#define CH32V003_INT_EN_REG(index)    ((CH32V003_INT_EN_REG_BASE) + (index * 0x10))
#define CH32V003_INT_CTL_REG(index)   ((CH32V003_INT_CTL_REG_BASE) + (index * 0x10))

#define CH32V003_ID_REG	  0x01
#define CH32V003_VER_REG  0x02
#define CH32V003_STA_REG  0x03
#define CH32V003_PAGE_REG 0x03

enum {
	CH32V003_STATUS_INVALID = 0,
	CH32V003_STATUS_OK = 1,

	CH32V003_STATUS_BUSY = 0x80,
};

#define CH32V003_BUSY_DELAY_TIMEOUT_MS (10)
#define CH32V003_BUSY_DELAY_CNT	       (3)

/* Register definitions */
#define REG_DEVICE_ID_CTRL	0x01
#define REG_DIRECTION		0x03
#define REG_OUTPUT		0x05
#define REG_OUTPUT_HIGH_Z	0x07
#define REG_INPUT_DEFAULT_STATE 0x09
#define REG_PUD_EN		0x0B
#define REG_PUD_SEL		0x0D
#define REG_INPUT_VALUE		0x0F
#define REG_INT_MASK		0x11
#define REG_INT_STATUS		0x13

#define SUPPORTED_FLAGS                                                                            \
	(GPIO_INPUT | GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW | GPIO_OUTPUT_INIT_HIGH |                 \
	 GPIO_PULL_DOWN | GPIO_PULL_UP | GPIO_ACTIVE_HIGH | GPIO_ACTIVE_LOW)

struct gpio_ch32003_config {
	struct gpio_driver_config common;
	struct i2c_dt_spec bus;
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

static inline int gpio_ch32v003_register_read(const struct device *dev, uint8_t reg, uint8_t *value)
{
	const struct gpio_ch32003_config *const config = dev->config;

	return i2c_reg_read_byte_dt(&config->bus, reg, value);
}

#ifdef CONFIG_GPIO_CSK6_CH32V003_NON_BLOCK_INIT
static K_EVENT_DEFINE(event_group);
#define INIT_BIT 	BIT(0)
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
	const struct device *dev = s_init_dev;
	const struct gpio_ch32003_config *const config = dev->config;

	LOG_DBG("addr:%02X", config->bus.addr);
	int r = i2c_configure(config->bus.bus, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_MASTER);
	if (r) {
		printk("i2c_configure failed, ret:%d\n", r);
		return;
	}

	uint8_t id = 0;
	r = gpio_ch32v003_register_read(dev, CH32V003_ID_REG, &id);
	if (r) {
		LOG_ERR("read id failed, r:%d\n", r);
		return;
	}

	LOG_DBG("chip id: %u\n", id);
	k_event_set(&event_group, INIT_BIT);
}
#else
#define	wait_init_done()
#endif //ifdef CONFIG_GPIO_CSK6_CH32V003_NON_BLOCK_INIT

static inline int gpio_ch32v003_register_write(const struct device *dev, uint8_t reg, uint8_t value)
{
	const struct gpio_ch32003_config *const config = dev->config;
	int r;
	uint8_t busy = 0x00;
	uint8_t cnt = 0;

	LOG_DBG("gpio_ch32v003_register_write, reg=0x%02x, value=0x%02x", reg, value);

	while (1) {
		r = gpio_ch32v003_register_read(dev, CH32V003_STA_REG, &busy);
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

static inline int ch32v003_dir_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;

	int r = gpio_ch32v003_register_write(dev, CH32V003_DIR_REG(config->id), flag);

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

	int r = gpio_ch32v003_register_read(dev, CH32V003_DIR_REG(config->id), flag);
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

	int r = gpio_ch32v003_register_write(dev, CH32V003_OD_REG(config->id), flag);
	if (r == 0) {
		drv_data->reg_cache.od = flag;
	}
	return r;
}

static inline int ch32v003_od_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return gpio_ch32v003_register_read(dev, CH32V003_OD_REG(config->id), flag);
}

static inline int ch32v003_out_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;
	int r = gpio_ch32v003_register_write(dev, CH32V003_OUT_REG(config->id), flag);
	if (r == 0) {
		drv_data->reg_cache.output = flag;
	}
	return r;
}

static inline int ch32v003_out_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return gpio_ch32v003_register_read(dev, CH32V003_OUT_REG(config->id), flag);
}

static inline int ch32v003_pull_mode_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return gpio_ch32v003_register_write(dev, CH32V003_PULL_UP_REG(config->id), flag);
}

static inline int ch32v003_pull_mode_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return gpio_ch32v003_register_read(dev, CH32V003_PULL_UP_REG(config->id), flag);
}

static inline int ch32v003_pull_en_set(const struct device *dev, uint8_t flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return gpio_ch32v003_register_write(dev, CH32V003_PULL_DWON_REG(config->id), flag);
}

static inline int ch32v003_pull_en_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	return gpio_ch32v003_register_read(dev, CH32V003_PULL_DWON_REG(config->id), flag);
}

static inline int ch32v003_in_get(const struct device *dev, uint8_t *flag)
{
	const struct gpio_ch32003_config *config = dev->config;
	struct gpio_ch32003_drv_data *const drv_data =
		(struct gpio_ch32003_drv_data *const)dev->data;
	int r = gpio_ch32v003_register_read(dev, CH32V003_IN_REG(config->id), flag);

	if (r == 0) {
		drv_data->reg_cache.input = *flag;
	}

	return r;
}

int gpio_ch32003_init(const struct device *dev)
{
	static uint8_t inited = 0;
	const struct gpio_ch32003_config *const config = dev->config;

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("%s is not ready", config->bus.bus->name);
		return -ENODEV;
	}

	if (inited) {
		return 0;
	}

#ifdef CONFIG_GPIO_CSK6_CH32V003_NON_BLOCK_INIT
	s_init_dev = dev;
	k_work_init_delayable(&init_work, init_work_handler);
	k_work_schedule(&init_work, K_MSEC(CONFIG_GPIO_CSK6_CH32V003_INIT_DELAY_MS));
#else
	int r = 0;
	/* TODO: 保证ch32v003初始化完成*/
	k_msleep(CONFIG_GPIO_CSK6_CH32V003_INIT_DELAY_MS);

	LOG_DBG("addr:%02X", config->bus.addr);
	r = i2c_configure(config->bus.bus, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_MASTER);
	if (r) {
		printk("i2c_configure failed, ret:%d\n", r);
		return r;
	}

	uint8_t id = 0;
	r = gpio_ch32v003_register_read(dev, CH32V003_ID_REG, &id);
	if (r) {
		LOG_ERR("read id failed, r:%d\n", r);
		return r;
	}

#endif
	inited = 1;

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

	wait_init_done();

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
			pull_en &= ~BIT(pin);
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
		r = ch32v003_pull_mode_set(dev, pull_mode);
		if (r != 0) {
			k_sem_give(&data->lock);
			return r;
		}

		// /* TODO: 固件还未实现 */
		// r = ch32v003_pull_en_set(dev, pull_en);
		// if (r != 0) {
		// 	k_sem_give(&data->lock);
		// 	return r;
		// }
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

	wait_init_done();
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

	wait_init_done();
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

	wait_init_done();
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

	wait_init_done();
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
	wait_init_done();
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
		.bus = I2C_DT_SPEC_GET(DT_INST_PARENT(inst)),                                      \
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
