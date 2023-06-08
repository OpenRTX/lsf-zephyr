/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT listenai_i2c_slavevdev

#include "i2c_slave_vdev.h"

#include <errno.h>
#include <string.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(slave_vdev, LOG_LEVEL_INF);

typedef struct _i2c_slave_vdev_data_t {
	struct i2c_slave_config config;
	uint32_t buffer_size;
	uint8_t *buffer;
	uint32_t buffer_idx;
	bool first_write;
} vdev_data_t;

typedef struct _i2c_slave_vdev_config {
	struct i2c_dt_spec bus;
	uint32_t buffer_size;
	uint8_t *buffer;
} vdev_config_t;

#define SLAVE_VDEV_STACK_SIZE 4096
K_THREAD_STACK_DEFINE(slave_vdev_task_stack_area, SLAVE_VDEV_STACK_SIZE);
static struct k_thread vdev_thread_data;
K_EVENT_DEFINE(slave_vdev_evt);
static struct k_event *slave_evt = &slave_vdev_evt;

static uint8_t m_handler_task_initialized = 0;
static slave_vdev_handler_t m_vdev_handler;
uint32_t m_event = 0;

static struct i2c_msg m_msg;

int slave_vdev_program(const struct device *dev, const uint8_t *src, unsigned int length)
{
	vdev_data_t *data = dev->data;

	if (length > data->buffer_size) {
		return -EINVAL;
	}

	memcpy(data->buffer, src, length);

	return 0;
}

int slave_vdev_read(const struct device *dev, uint8_t *dst, unsigned int length)
{
	vdev_data_t *data = dev->data;

	if (!data || length >= data->buffer_size) {
		return -EINVAL;
	}
	memcpy(dst, data->buffer, length);

	return 0;
}

int slave_vdev_read_byte(const struct device *dev, uint8_t *dst, unsigned int offset)
{
	vdev_data_t *data = dev->data;

	if (!data || offset >= data->buffer_size) {
		return -EINVAL;
	}

	*dst = data->buffer[offset];

	return 0;
}

int slave_vdev_event_handler_register(const struct device *dev, slave_vdev_handler_t handler)
{
	if (!handler) {
		LOG_WRN("Device handle(handler) is null");
		return 0;
	}
	if (!dev) {
		LOG_ERR("Illegal, device(dev) cannot be NULL");
		return -1;
	}

	m_vdev_handler = handler;
	return 0;
}

static void slave_vdev_thread(void *user_data, void *v2, void *v3)
{
	uint32_t evt = 0;
	LOG_DBG("Slave_vdev_thread start successful");
	for (;;) {
		evt = k_event_wait(slave_evt,
				   SLAVE_VDEV_EVENT_BUS_STOP | SLAVE_VDEV_EVENT_READ_DONE |
					   SLAVE_VDEV_EVENT_WRITE_DONE,
				   true, K_FOREVER);
		if (m_vdev_handler) {
			m_vdev_handler((struct device *)user_data, slave_evt->events, m_msg);
		}
	}
}

int start_slave_vdev_handler_task(const struct device *dev)
{
	if (m_handler_task_initialized) {
		LOG_WRN("The virtual slave processing task has been initialized");
		return 1;
	}
	vdev_data_t *data = dev->data;
	m_msg.buf = data->buffer;
	int pri = k_thread_priority_get(k_current_get());
	k_thread_create(&vdev_thread_data, slave_vdev_task_stack_area,
			K_THREAD_STACK_SIZEOF(slave_vdev_task_stack_area), slave_vdev_thread, (void *)dev,
			NULL, NULL, pri, 0, K_NO_WAIT);
	m_handler_task_initialized = 1;
	LOG_INF("start_slave_vdev_handler_task ok");
	return 0;
}

/* -------------------------------------------------------------------------- */
static int slave_vdev_write_requested(struct i2c_slave_config *config)
{
	vdev_data_t *data = CONTAINER_OF(config, vdev_data_t, config);
	LOG_DBG("slave_vdev: write req");

	data->first_write = true;
	m_msg.len = 0;

	m_event |= SLAVE_VDEV_EVENT_WRITE_REQ;
	return 0;
}

static int slave_vdev_read_requested(struct i2c_slave_config *config, uint8_t *val)
{
	vdev_data_t *data = CONTAINER_OF(config, vdev_data_t, config);
	LOG_DBG("slave_vdev: read req, val=0x%x", *val);

	*val = data->buffer[data->buffer_idx];
	m_msg.len = 0;
	
	m_event |= SLAVE_VDEV_EVENT_READ_REQ;
	/* Increment will be done in the read_processed callback */
	return 0;
}

static int slave_vdev_write_received(struct i2c_slave_config *config, uint8_t val)
{
	vdev_data_t *data = CONTAINER_OF(config, vdev_data_t, config);

	LOG_DBG("slave_vdev: write done, val=0x%x", val);

	/* In case slave_vdev wants to be R/O, return !0 here could trigger
	 * a NACK to the I2C controller, support depends on the
	 * I2C controller support
	 */

	if (data->first_write) {
		if (val > data->buffer_size) {
			LOG_WRN("slave_vdev: out of buffer size, val=0x%x, buffer_size=%d", val, data->buffer_size);
			return 0;
		}
		data->buffer_idx = val;
		m_msg.buf = &(data->buffer[data->buffer_idx]);
		data->first_write = false;
	} else {
		data->buffer[data->buffer_idx++] = val;
		m_msg.len++;
	}

	data->buffer_idx = data->buffer_idx % data->buffer_size;

	m_msg.flags = I2C_MSG_READ;
	m_event |= SLAVE_VDEV_EVENT_WRITE_DONE;
	return 0;
}

static int slave_vdev_read_processed(struct i2c_slave_config *config, uint8_t *val)
{
	/**
	 * @note: Since the Hal library does not support I2C Slave multi-byte
	 * transmission, this API is not currently supported.
	 * 	(由于 Hal 库暂不支持 I2C Slave 多字节传输，故暂不支持该 API)
	 *
	 */
#if 0
	vdev_data_t *data = CONTAINER_OF(config, vdev_data_t, config);
	LOG_DBG("slave_vdev: read done, val=0x%x", *val);

	/* Increment here */
	data->buffer_idx = (data->buffer_idx + 1) % data->buffer_size;

	*val = data->buffer[data->buffer_idx];

	m_event = 0;
	m_event |= SLAVE_VDEV_EVENT_READ_DONE;
	m_msg.len++;
	m_msg.flags = I2C_MSG_WRITE;
	k_event_post(slave_evt, m_event);
	

	/* Increment will be done in the next read_processed callback
	 * In case of STOP, the byte won't be taken in account
	 */
#endif
	return 0;
}

static int slave_vdev_stop(struct i2c_slave_config *config)
{
	vdev_data_t *data = CONTAINER_OF(config, vdev_data_t, config);
	LOG_DBG("slave_vdev: stop");

	data->first_write = true;

	m_event |= SLAVE_VDEV_EVENT_BUS_STOP;
	k_event_post(slave_evt, m_event);
	m_event = 0;
	return 0;
}

static int slave_vdev_register(const struct device *dev)
{
	const vdev_config_t *cfg = dev->config;
	vdev_data_t *data = dev->data;

	return i2c_slave_register(cfg->bus.bus, &data->config);
}

static int slave_vdev_unregister(const struct device *dev)
{
	const vdev_config_t *cfg = dev->config;
	vdev_data_t *data = dev->data;

	return i2c_slave_unregister(cfg->bus.bus, &data->config);
}

static const struct i2c_slave_driver_api api_funcs = {
	.driver_register = slave_vdev_register,
	.driver_unregister = slave_vdev_unregister,
};

static const struct i2c_slave_callbacks slave_vdev_callbacks = {
	.write_requested = slave_vdev_write_requested,
	.read_requested = slave_vdev_read_requested,
	.write_received = slave_vdev_write_received,
	.read_processed = slave_vdev_read_processed,
	.stop = slave_vdev_stop,
};

static int i2c_slave_vdev_init(const struct device *dev)
{
	const vdev_config_t *cfg = dev->config;
	vdev_data_t *data = dev->data;

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C controller device not ready");
		return -ENODEV;
	}

	data->buffer_size = cfg->buffer_size;
	data->buffer = cfg->buffer;
	data->config.address = cfg->bus.addr;
	data->config.callbacks = &slave_vdev_callbacks;

	return 0;
}

#define I2C_SLAVEDEV_INIT(inst)                                                                    \
	static vdev_data_t i2c_slave_vdev_##inst##_dev_data;                                       \
                                                                                                   \
	static uint8_t i2c_slave_vdev_##inst##_buffer[(DT_INST_PROP(inst, size))];                 \
                                                                                                   \
	static vdev_config_t i2c_slave_vdev_##inst##_cfg = {                                       \
		.bus = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.buffer_size = DT_INST_PROP(inst, size),                                           \
		.buffer = i2c_slave_vdev_##inst##_buffer};                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, &i2c_slave_vdev_init, NULL, &i2c_slave_vdev_##inst##_dev_data, \
			      &i2c_slave_vdev_##inst##_cfg, POST_KERNEL,                           \
			      CONFIG_I2C_SLAVE_INIT_PRIORITY, &api_funcs);

DT_INST_FOREACH_STATUS_OKAY(I2C_SLAVEDEV_INIT)
