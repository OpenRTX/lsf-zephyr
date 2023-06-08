/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_adc_ch32v003

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_adc_ch32v003, CONFIG_ADC_LOG_LEVEL);

#define CH32V003_ID_REG	  0x01
#define CH32V003_VER_REG  0x02
#define CH32V003_STA_REG  0x03
#define CH32V003_PAGE_REG 0x03

#define CH32V003_ADC_CONFIG_0	0x50
#define CH32V003_ADC_CONFIG_1	0x51
#define CH32V003_ADC_EN_REG		0x52
#define CH32V003_ADC_DATA_H_REG	0x53
#define CH32V003_ADC_DATA_L_REG	0x54
#define CH32V003_ADC_STATUS_REG	0x55

#define CH32V003_ADC_CH_DATA_H_REG(ch) ((CH32V003_ADC_DATA_H_REG) + (ch)*16)
#define CH32V003_ADC_CH_DATA_L_REG(ch) ((CH32V003_ADC_DATA_L_REG) + (ch)*16)

enum {
	CH32V003_STATUS_INVALID = 0,
	CH32V003_STATUS_OK = 1,

	CH32V003_STATUS_BUSY = 0x80,
};

#define CH32V003_ADC_VREF_INTERNAL (3300)
#define CH32V003_ADC_RESOLUTION (10)

#define CH32V003_BUSY_DELAY_TIMEOUT_MS (10)
#define CH32V003_BUSY_DELAY_CNT	       (3)

struct adc_ch32v003_config {
	struct i2c_dt_spec bus;
};

struct adc_ch32v003_data {
	struct k_sem lock;
	struct adc_context ctx;
	int16_t *buffer;
	int16_t *repeat_buffer;
	const struct device *device;
	struct k_sem acq_sem;
};

static inline int adc_ch32v003_register_read(const struct device *dev, uint8_t reg, uint8_t *value)
{
	const struct adc_ch32v003_config *const config = dev->config;
	int r = i2c_reg_read_byte_dt(&config->bus, reg, value);
	LOG_DBG("adc_ch32v003_register_read, name:%s, reg=0x%02x, value=%d", dev->name, reg, *value);
	return r;
}

static inline int adc_ch32v003_register_write(const struct device *dev, uint8_t reg, uint8_t value)
{
	const struct adc_ch32v003_config *const config = dev->config;
	int r;
	uint8_t busy = 0x00;
	uint8_t cnt = 0;

	LOG_DBG("adc_ch32v003_register_write, reg=0x%02x, value=0x%02x", reg, value);

	while (1) {
		r = adc_ch32v003_register_read(dev, CH32V003_STA_REG, &busy);
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

static int adc_ch32v003_wait_data_ready(const struct device *dev, uint8_t ch)
{
	uint8_t status = 0;
	int r;
	uint32_t time = k_uptime_get_32();

	while (!(status & (1 << ch))) {
		r = adc_ch32v003_register_read(dev, 0x55 + ch * 16, &status);
		if (r) {
			return r;
		}
		k_usleep(100);

		if ((k_uptime_get_32() - time) > 2 * 1000) {
			LOG_ERR("wait ch %d ready timeout", ch);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int adc_ch32v003_read_data(const struct device *dev, uint8_t ch, uint16_t *out)
{
	int r;
	uint8_t *buf = (uint8_t *)out;

	r = adc_ch32v003_register_read(dev, CH32V003_ADC_CH_DATA_H_REG(ch), buf + 1);
	if (r) {
		return r;
	}

	r = adc_ch32v003_register_read(dev, CH32V003_ADC_CH_DATA_L_REG(ch), buf);
	if (r) {
		return r;
	}

	return 0;
}

static int adc_ch32v003_start_conversion(const struct device *dev, uint8_t ch)
{
	return adc_ch32v003_register_write(dev, 0x52, ch);
}

static int adc_ch32v003_channel_setup(const struct device *dev,
				      const struct adc_channel_cfg *channel_cfg)
{
	return 0;
}

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct adc_ch32v003_data *data = CONTAINER_OF(ctx, struct adc_ch32v003_data, ctx);
	const struct device *dev = data->device;
	int r;

	uint8_t channel_mask = ctx->sequence.channels;
	LOG_DBG("adc_context_start_sampling, ch:%02X", channel_mask);

	r = adc_ch32v003_start_conversion(dev, channel_mask);
	if (r) {
		adc_context_complete(&data->ctx, r);
	}

	k_sem_give(&data->acq_sem);
}

static int adc_ch32v003_start_read(const struct device *dev, const struct adc_sequence *sequence)
{
	struct adc_ch32v003_data *data = dev->data;

	if (sequence->resolution != CH32V003_ADC_RESOLUTION) {
		LOG_ERR("ADC resolution must be %d", CH32V003_ADC_RESOLUTION);
		return -EINVAL;
	}

	adc_context_start_read(&data->ctx, sequence);

	return adc_context_wait_for_completion(&data->ctx);
}

static int adc_ch32v003_read(const struct device *dev, const struct adc_sequence *sequence)
{
	struct adc_ch32v003_data *data = dev->data;
	int error = 0;

	data->buffer = sequence->buffer;

	adc_context_lock(&data->ctx, false, NULL);
	error = adc_ch32v003_start_read(dev, sequence);
	adc_context_release(&data->ctx, error);

	return error;
}

static void adc_context_update_buffer_pointer(struct adc_context *ctx, bool repeat_sampling)
{
	struct adc_ch32v003_data *data = CONTAINER_OF(ctx, struct adc_ch32v003_data, ctx);

	if (repeat_sampling) {
		data->buffer = data->repeat_buffer;
	}
}

static void adc_ch32v003_thread(void *p1, void *p2, void *p3)
{
	const struct device *dev = p1;
	struct adc_ch32v003_data *data = dev->data;
	int r;

	if (!device_is_ready(dev)) {
		LOG_ERR("adc_ch32v003_thread, device not ready");
	}

	LOG_DBG("dev:%p, name:%s", dev, dev->name);
	while (1) {
		k_sem_take(&data->acq_sem, K_FOREVER);
		uint8_t channel_mask = data->ctx.sequence.channels;
		uint8_t i;
		uint8_t err_happend = 0;

		LOG_DBG("channel_mask:%02X", channel_mask);

		/* FIXME:状态寄存器未实现 */
		// for (i = 0; i < 8; i++) {
		// 	if (channel_mask & (1 << i)) {
		// 		LOG_DBG("wait ch %d ready", i);
		// 		r = adc_ch32v003_wait_data_ready(dev, i);
		// 		if (r) {
		// 			adc_context_complete(&data->ctx, r);
		// 			err_happend = 1;
		// 			break;
		// 		}
		// 	}
		// }
		(void)adc_ch32v003_wait_data_ready;
		k_msleep(100);
		if (!err_happend) {
			for (i = 0; i < 8; i++) {
				int16_t value = 0;
				if (channel_mask & (1 << i)) {
					r = adc_ch32v003_read_data(dev, i, &value);
					if (r) {
						adc_context_complete(&data->ctx, r);
						err_happend = 1;
						break;
					}
					*data->buffer++ = value;
					LOG_DBG("value:%d", value);
				}
			}
		}

		if (!err_happend) {
			adc_context_on_sampling_done(&data->ctx, dev);
		}
	}
}

int adc_ch32v003_init(const struct device *dev)
{
	const struct adc_ch32v003_config *config = dev->config;
	struct adc_ch32v003_data *data = dev->data;
	int r;

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("device:%s is not ready", config->bus.bus->name);
	}

	LOG_DBG("adc_ch32v003_init, addr:%02X", config->bus.addr);

	r = i2c_configure(config->bus.bus, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_MASTER);
	if (r) {
		printk("i2c_configure failed, ret:%d\n", r);
		return r;
	}

	uint8_t id = 0;
	r = adc_ch32v003_register_read(dev, CH32V003_ID_REG, &id);
	if (r) {
		LOG_ERR("read id failed, r:%d\n", r);
		return r;
	}

	LOG_DBG("chip id: %u\n", id);
	adc_context_unlock_unconditionally(&data->ctx);

	return 0;
}

static const struct adc_driver_api api_ch32v003_driver_api = {
	.channel_setup = adc_ch32v003_channel_setup,
	.read = adc_ch32v003_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = NULL,
#endif
	.ref_internal = CH32V003_ADC_VREF_INTERNAL,
};

#define ADC_DEVICE_INSTANCE(inst)                                                                  \
	static const struct adc_ch32v003_config adc_ch32003_##inst##_cfg = {                       \
		.bus = I2C_DT_SPEC_GET(DT_INST_PARENT(inst)),                                      \
	};                                                                                         \
                                                                                                   \
	static struct adc_ch32v003_data adc_ch32003_##inst##_data = {                              \
		ADC_CONTEXT_INIT_TIMER(adc_ch32003_##inst##_data, ctx),                            \
		ADC_CONTEXT_INIT_LOCK(adc_ch32003_##inst##_data, ctx),                             \
		ADC_CONTEXT_INIT_SYNC(adc_ch32003_##inst##_data, ctx),                             \
		.acq_sem = Z_SEM_INITIALIZER(adc_ch32003_##inst##_data.acq_sem, 0, 1),             \
		.device = DEVICE_DT_INST_GET(inst),                                                \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, adc_ch32v003_init, NULL, &adc_ch32003_##inst##_data,           \
			      &adc_ch32003_##inst##_cfg, POST_KERNEL, 90,                          \
			      &api_ch32v003_driver_api);                                           \
	K_THREAD_DEFINE(adc_ch32v003_thread_id_##inst, 2048, adc_ch32v003_thread,                  \
			DEVICE_DT_INST_GET(inst), NULL, NULL, 0, 0, 0);

DT_INST_FOREACH_STATUS_OKAY(ADC_DEVICE_INSTANCE);
