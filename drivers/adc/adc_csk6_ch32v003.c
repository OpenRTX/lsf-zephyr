/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_adc_ch32v003

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_adc_ch32v003, CONFIG_ADC_LOG_LEVEL);

#include "exmcu_comm.h"

#define CH32V003_ADC_RESOLUTION	   (10)

struct adc_ch32v003_config {
	const struct device *exmcu;
};

struct adc_ch32v003_data {
	struct k_sem lock;
	struct adc_context ctx;
	int16_t *buffer;
	int16_t *repeat_buffer;
	const struct device *device;
	struct k_sem acq_sem;
};

static int adc_ch32v003_wait_data_ready(const struct device *dev, uint8_t ch)
{
	uint8_t status = 0;
	int r;
	uint32_t time = k_uptime_get_32();
	const struct adc_ch32v003_config *config = dev->config;

	while (!(status & CH32V003_ADC_EOC)) {
		r = exmcu_register_read_byte(config->exmcu, CH32V003_ADC_STATUS_REG, &status);
		if (r) {
			return r;
		}
		k_msleep(100);

		if ((k_uptime_get_32() - time) > (2 * 1000)) {
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
	const struct adc_ch32v003_config *config = dev->config;

	r = exmcu_register_read_byte(config->exmcu, CH32V003_ADC_CH_DATA_H_REG(ch), buf + 1);
	if (r) {
		return r;
	}

	r = exmcu_register_read_byte(config->exmcu, CH32V003_ADC_CH_DATA_L_REG(ch), buf);
	if (r) {
		return r;
	}

	return 0;
}

static int adc_ch32v003_start_conversion(const struct device *dev, uint8_t ch_mask)
{
	int r;
	const struct adc_ch32v003_config *config = dev->config;

	r = exmcu_register_write_byte(config->exmcu, CH32V003_ADC_CONFIG_0,
				      (CH32V003_ADC_SAMPLING_CYCLE_30 << 1) | CH32V003_ADC_MODE_ONCE);
	if (r) {
		return r;
	}

	r = exmcu_register_write_byte(config->exmcu, CH32V003_ADC_CONFIG_1, ch_mask);

	return r;
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

		LOG_DBG("channel_mask:%02X", channel_mask);

		r = adc_ch32v003_wait_data_ready(dev, channel_mask);
		if (r) {
			adc_context_complete(&data->ctx, r);
			continue;
		}

		for (i = 0; i < 8; i++) {
			int16_t value = 0;
			if (channel_mask & (1 << i)) {
				r = adc_ch32v003_read_data(dev, i, &value);
				if (r) {
					adc_context_complete(&data->ctx, r);
					continue;
				}
				*data->buffer++ = value;
				LOG_DBG("value:%d", value);
			}
		}

		adc_context_on_sampling_done(&data->ctx, dev);
	}
}

int adc_ch32v003_init(const struct device *dev)
{
	const struct adc_ch32v003_config *config = dev->config;
	struct adc_ch32v003_data *data = dev->data;

	if (!device_is_ready(config->exmcu)) {
		LOG_ERR("device:%s is not ready", config->exmcu->name);
	}

	adc_context_unlock_unconditionally(&data->ctx);

	return 0;
}

static const struct adc_driver_api api_ch32v003_driver_api = {
	.channel_setup = adc_ch32v003_channel_setup,
	.read = adc_ch32v003_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = NULL,
#endif
	.ref_internal = CONFIG_ADC_CSK6_CH32V003_INTERNAL_VREF,
};

#define ADC_DEVICE_INSTANCE(inst)                                                                  \
	static const struct adc_ch32v003_config adc_ch32003_##inst##_cfg = {                       \
		.exmcu = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                      \
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
			      &adc_ch32003_##inst##_cfg, POST_KERNEL, CONFIG_ADC_CSK6_CH32V003_INIT_PRIO,                          \
			      &api_ch32v003_driver_api);                                           \
	K_THREAD_DEFINE(adc_ch32v003_thread_id_##inst, CONFIG_ADC_CSK6_CH32V003_THREAD_STACK_SIZE, \
			adc_ch32v003_thread, DEVICE_DT_INST_GET(inst),                             \
			CONFIG_ADC_CSK6_CH32V003_THREAD_PRIO, NULL, 0, 0, 0);

DT_INST_FOREACH_STATUS_OKAY(ADC_DEVICE_INSTANCE);
