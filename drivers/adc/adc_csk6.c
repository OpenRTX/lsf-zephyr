/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_adc

#include <assert.h>
#include <drivers/adc.h>
#include <kernel.h>
#include <soc.h>
#include <soc_dt.h>
#include <drivers/pinmux.h>

#include "Driver_GPADC.h"
#include "AON_IOMuxManager.h"
#include "IOMuxManager.h"
#include "gpadc_venus.h"

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_adc, LOG_LEVEL_WRN);

#define DEV_DATA(dev) ((struct adc_csk6_data * const)(dev)->data)
#define DEV_CFG(dev) ((struct adc_csk6_cfg * const)(dev)->config)
#define DEV_HANDLER(dev) ((void *)(DEV_DATA(dev)->handle))
#define CSK_ADC_CHANNEL_SEL(n)	((1 << n) << CSK_GPADC_CHANNEL_SEL_Pos)

#define ADC_TRIGGER_NUM  (3)
#define ADC_CHANNEL_COUNT (7)

/* ADC internal reference voltage (Unit:mV) */
#define CSK6_ADC_VREF_VOL 3300

struct adc_csk6_data {
    /* driver handle */
    void *handle; 
    
	struct adc_context ctx;
	/* Channels mask */
	uint32_t channels;
	/* Save ADC result to the buffer. */
	uint16_t *buffer;
	/*
	 * The sample buffer pointer should be prepared
	 * for writing of next sampling results.
	 */
	uint16_t *repeat_buffer;

	uint8_t triggernum;
};


struct adc_csk6_cfg {
	/* GPIO pin */
	uint8_t pin;
	/* ADC alternate configuration */
	const struct csk6_pinctrl_cfg *pinctrl_list;
	uint8_t pinctrl_list_size;
};


static void adc_context_update_buffer_pointer(struct adc_context *ctx,
					      bool repeat_sampling)
{
	struct adc_csk6_data *data = CONTAINER_OF(ctx, struct adc_csk6_data, ctx);

	if (repeat_sampling) {
		data->buffer = data->repeat_buffer;
	}
}

static int adc_csk6_start_read(const struct device *dev,
				  const struct adc_sequence *sequence)
{
	struct adc_csk6_data *data = DEV_DATA(dev);

	adc_context_start_read(&data->ctx, sequence);

	return adc_context_wait_for_completion(&data->ctx);
}

static int adc_csk6_channel_setup(const struct device *dev,
				   const struct adc_channel_cfg *channel_cfg)
{
	struct adc_csk6_data *data = DEV_DATA(dev);

	if (channel_cfg->channel_id >= ADC_CHANNEL_COUNT) {
		LOG_ERR("ADC Channel %d is not valid", channel_cfg->channel_id);
		return -EINVAL;
	}

	if (channel_cfg->acquisition_time) {
		LOG_ERR("acquisition_time are not supported");
		return -EINVAL;
	}

	if (channel_cfg->differential) {
		LOG_ERR("Differential channels are not supported");
		return -EINVAL;
	}


	if (channel_cfg->gain != ADC_GAIN_1) {
		LOG_ERR("Invalid ADC channel gain");
		return -EINVAL;
	}

	if (channel_cfg->reference != ADC_REF_INTERNAL) {
		LOG_ERR("Invalid ADC channel reference");
		return -EINVAL;
	}

	//TODO: aon_pinmux
	if(6 == channel_cfg->channel_id)
	{
		AON_IOMuxManager_PinConfigure(CSK_AON_IOMUX_PAD_B, 5, CSK_IOMUX_FUNC_DEFAULT);
		LOG_DBG("keysense pinmux set\n");
	}

	data->channels |= CSK_ADC_CHANNEL_SEL(channel_cfg->channel_id);
	HAL_GPADC_Control(DEV_HANDLER(dev), CSK_GPADC_SAMPLETIME_8 | CSK_GPADC_CLKFREQ_12M |	\
	 					data->channels | \
					  	CSK_GPADC_INPUT_MODE_Single | CSK_GPADC_VREF_SEL_1_25V | CSK_GPADC_DMA_DISABLE);

	LOG_DBG("channels(0x%x) setup\n", data->channels);

	return 0;
}

static int adc_csk6_read(const struct device *dev,
			  const struct adc_sequence *sequence)
{
	struct adc_csk6_data *data = DEV_DATA(dev);
	int error = 0;

	data->buffer = sequence->buffer;

	adc_context_lock(&data->ctx, false, NULL);
	error = adc_csk6_start_read(dev, sequence);
	adc_context_release(&data->ctx, error);

	return error;
}

#ifdef CONFIG_ADC_ASYNC
static int adc_csk6_read_async(const struct device *dev,
				 const struct adc_sequence *sequence,
				 struct k_poll_signal *async)
{
	//TODO

	return -EINVAL;
}
#endif

static void adc_csk6_get_sample(const struct device *dev)
{
	struct adc_csk6_data *data = DEV_DATA(dev);

	uint32_t channel_mask = data->ctx.sequence.channels;

	uint16_t adc_value = 0;


	while (channel_mask) 
	{
		uint8_t ch = find_lsb_set(channel_mask) - 1;

		//get adc fifo value
		for(uint8_t i = 0; i < data->triggernum; i++)
		{
			adc_value = HAL_GPADC_GetValue(DEV_HANDLER(dev),CSK_ADC_CHANNEL_SEL(ch));
			LOG_DBG("channel(%d) get adc_value:%d\n", ch, adc_value);
		}

		//take the last sample value
		*data->buffer++ = adc_value;

		channel_mask &= ~BIT(ch);
	}
} 

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct adc_csk6_data *data = CONTAINER_OF(ctx, struct adc_csk6_data, ctx);
	uint32_t channel_mask = ctx->sequence.channels;
	const struct device *dev = DEVICE_DT_INST_GET(0);

	LOG_DBG("start_sampling channel_mask: 0x%x\n", channel_mask);

	//The previous two sample data was inaccurate,so need sample three times
	data->triggernum = ADC_TRIGGER_NUM;
	HAL_GPADC_SetTriggerNum(DEV_HANDLER(dev), data->triggernum);


	HAL_GPADC_Start(DEV_HANDLER(dev));

	//Will wait until the ADC conversion is complete!
	HAL_GPADC_PollForConversion(DEV_HANDLER(dev), 0);

	adc_csk6_get_sample(dev);

	adc_context_on_sampling_done(&data->ctx, dev);
}

static void adc_csk6_isr(const void *arg)
{
	struct device *dev = (struct device *)arg;
	struct adc_csk6_data *data = DEV_DATA(dev);
	GPADC_RESOURCES *adc = data->handle;

	LOG_DBG("ADC ISR triggered.");

	if(adc->irq_handler != NULL)
	{
		adc->irq_handler();
	}

	adc_context_on_sampling_done(&data->ctx, dev);
}

static int adc_csk6_init(const struct device *dev)
{
    struct adc_csk6_data *data = DEV_DATA(dev);
	struct adc_csk6_cfg *config = DEV_CFG(dev);

    data->handle = GPADC();

	HAL_GPADC_Initialize(DEV_HANDLER(dev));

	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    adc_csk6_isr, DEVICE_DT_INST_GET(0), 0);

	adc_context_unlock_unconditionally(&data->ctx);

	PINCTRL_CONFIG_PINS(config->pinctrl_list, config->pinctrl_list_size);

	return 0;
}


static const struct adc_driver_api api_stm32_driver_api = {
	.channel_setup = adc_csk6_channel_setup,
	.read = adc_csk6_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = adc_csk6_read_async,
#endif
	.ref_internal = CSK6_ADC_VREF_VOL,
};

#if DT_INST_NUM_PINCTRLS_BY_IDX(0, 0)
static const struct csk6_pinctrl_cfg                                       							\
		adc_alts[DT_INST_NUM_PINCTRLS_BY_IDX(0, 0)] =          									\
			CSK6_DT_ALT_ITEMS_LIST(0); 
#endif

static struct adc_csk6_data adc_csk6_data_0 = {
		ADC_CONTEXT_INIT_TIMER(adc_csk6_data_0, ctx),
		ADC_CONTEXT_INIT_LOCK(adc_csk6_data_0, ctx),
		ADC_CONTEXT_INIT_SYNC(adc_csk6_data_0, ctx),
};

static const struct adc_csk6_cfg adc_csk6_cfg_0 = {
#if DT_INST_NUM_PINCTRLS_BY_IDX(0, 0)
		.pinctrl_list = adc_alts, 													   
		.pinctrl_list_size = ARRAY_SIZE(adc_alts),	
#endif								   
};

DEVICE_DT_INST_DEFINE(0, adc_csk6_init,
		      NULL,
		      &adc_csk6_data_0,
		      &adc_csk6_cfg_0, PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      &api_stm32_driver_api);
