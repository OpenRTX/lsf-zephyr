/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_keysense

#include <zephyr.h>
#include <drivers/kscan.h>
#include <drivers/adc.h>

#include "Driver_KEYSENSE.h"
#include "keysense.h"
#include "AON_IOMuxManager.h"
#include "IOMuxManager.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_keysense, LOG_LEVEL_WRN);

#define DEV_DATA(dev) ((struct keysense_csk6_data * const)(dev)->data)
#define DEV_CFG(dev) ((struct keysense_csk6_cfg * const)(dev)->config)
#define DEV_HANDLER(dev) (DEV_DATA(dev)->handle)
#define DEV_CB(dev) (DEV_DATA(dev)->cb)

//offset of analog key voltage monitoring
#define ANALOG_KEY_TRIGGER_OFFSET 100

/* Common settings supported by most ADCs */
#define ADC_RESOLUTION		11

static int csk6_adc_keysense_get_sample(const struct device *dev, uint32_t *value);
static void keysense_press_handler(struct k_work *work);
static void keysense_release_handler(struct k_work *work);
K_WORK_DEFINE(key_press_work, keysense_press_handler);
K_WORK_DEFINE(key_release_work, keysense_release_handler);

struct adc_channel_cfg channel_cfg = {
	.gain = ADC_GAIN_1,
	.reference = ADC_REF_INTERNAL,
	/* channel ID will be overwritten below */
	.channel_id = 0,
	.differential = 0
};

struct adc_sequence sequence = {
	/* individual channels will be added below */
	.channels    = 0,
	.resolution  = ADC_RESOLUTION,
};

struct keysense_csk6_cfg {
	const struct device *adc_dev;
	uint8_t ch;
	uint32_t *analog_key_voltage;
	uint8_t analog_key_num;
};

struct keysense_csk6_data {
    /* driver handle */
    void *handle; 

	int32_t adc_vref;
	int16_t adc_sample_buffer[1];

	kscan_callback_t cb;
};

static void keysense_press_handler(struct k_work *work)
{
	const struct device *dev = DEVICE_DT_INST_GET(0);
	uint32_t key_voltage = 0;
	uint8_t i;
	struct keysense_csk6_cfg *config = DEV_CFG(dev);

	csk6_adc_keysense_get_sample(dev, &key_voltage);

	LOG_DBG("KEYSENSE press, voltage:%d mV\n", key_voltage);

	for(i = 0; i < config->analog_key_num; i++)
	{
		if((key_voltage > (config->analog_key_voltage[i] - ANALOG_KEY_TRIGGER_OFFSET)) && (key_voltage < (config->analog_key_voltage[i] + ANALOG_KEY_TRIGGER_OFFSET)))
			break;
	}

	if(i < config->analog_key_num)
	{
		DEV_CB(dev)(dev, i, 0, true);
	}
}

static void keysense_release_handler(struct k_work *work)
{
	const struct device *dev = DEVICE_DT_INST_GET(0);
	uint32_t key_voltage = 0;

	csk6_adc_keysense_get_sample(dev, &key_voltage);

	LOG_DBG("KEYSENSE release, voltage:%d mV\n", key_voltage);

	DEV_CB(dev)(dev, 0, 0, false);
}

static void keysense_csk6_isr(const void *arg)
{
	struct device *dev = (struct device *)arg;
	KEYSENSE_RESOURCES *keysense = DEV_HANDLER(dev);

	LOG_DBG("KEYSENSE ISR triggered.");

	if(keysense->irq_handler != NULL)
	{
		keysense->irq_handler();
	}
}

static uint32_t adc_to_millivolts(uint16_t adc_value, int32_t vref)
{
	int32_t mv_value = adc_value - 2048;

	adc_raw_to_millivolts(vref, ADC_GAIN_1,
					ADC_RESOLUTION, &mv_value);

	return mv_value;
}

static int csk6_adc_keysense_get_sample(const struct device *dev, uint32_t *value)
{
	struct keysense_csk6_cfg *config = DEV_CFG(dev);
	struct keysense_csk6_data *data = DEV_DATA(dev);
	int err = 0;

	channel_cfg.channel_id = config->ch;
	adc_channel_setup(config->adc_dev, &channel_cfg);

	sequence.channels |= BIT(config->ch);
	sequence.buffer = data->adc_sample_buffer;
	sequence.buffer_size = sizeof(data->adc_sample_buffer);

	err = adc_read(config->adc_dev, &sequence);
	if (err != 0) {
		LOG_ERR("ADC get keysense err %d.\n", err);
		return -EINVAL;
	}

	data->adc_vref = adc_ref_internal(config->adc_dev);

	*value = adc_to_millivolts(data->adc_sample_buffer[0],data->adc_vref);

	return 0;
}


static void keysense_press_event(void* param)
{
	k_work_submit(&key_press_work);
}

static void keysense_release_event(void* param)
{
	k_work_submit(&key_release_work);
}

static int kscan_csk6_config(const struct device *dev,
				kscan_callback_t callback)
{
	struct keysense_csk6_cfg *config = DEV_CFG(dev);

	if (!device_is_ready(config->adc_dev)) {
		LOG_ERR("keysense can not use adc sample \n");
		return -EINVAL;
	}

	DEV_CB(dev) = callback;

	LOG_DBG("analog key trigger %d %d %d \n", config->analog_key_voltage[0], config->analog_key_voltage[1], config->analog_key_voltage[2]);

	AON_IOMuxManager_PinConfigure(CSK_AON_IOMUX_PAD_B, 5, CSK_IOMUX_FUNC_DEFAULT);

    HAL_KEYSENSE_RegisterCallback(DEV_HANDLER(dev), CSK_KEYSENSE_RELEASE, keysense_release_event);
    HAL_KEYSENSE_RegisterCallback(DEV_HANDLER(dev), CSK_KEYSENSE_PRESS, keysense_press_event);
	HAL_KEYSENSE_InterruptEnable(DEV_HANDLER(dev), CSK_KEYSENSE_INTERRUPT_MODE_RELEASE |\
											 CSK_KEYSENSE_INTERRUPT_MODE_PRESS);

    HAL_KEYSENSE_Enable(DEV_HANDLER(dev));

	return 0;
}

static int kscan_csk6_init(const struct device *dev)
{
    DEV_HANDLER(dev) = KEYSENSE();

	HAL_KEYSENSE_Initialize(DEV_HANDLER(dev));
	HAL_KEYSENSE_Control(DEV_HANDLER(dev), 0x00200040);

	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    keysense_csk6_isr, DEVICE_DT_INST_GET(0), 0);

	return 0;
}

static const struct kscan_driver_api kscan_csk6_api = {
	.config = kscan_csk6_config,
};


#define ADC_NUM_CHANNELS	DT_PROP_LEN(DT_NODELABEL(keysense), io_channels)
#if ADC_NUM_CHANNELS > 2
#error "CSK6 Keysense only use one channel to sample"
#endif

static uint32_t key_voltage_level[] = DT_PROP(DT_NODELABEL(keysense), analogkey_trigger);

static struct keysense_csk6_data keysense_csk6_data_0;

static const struct keysense_csk6_cfg keysense_csk6_cfg_0 = {
	.adc_dev = DEVICE_DT_GET(DT_PHANDLE(DT_NODELABEL(keysense), io_channels)),
	.ch = DT_IO_CHANNELS_INPUT_BY_IDX(DT_NODELABEL(keysense), 0),
	.analog_key_voltage = key_voltage_level,
	.analog_key_num = DT_PROP_LEN(DT_NODELABEL(keysense), analogkey_trigger)
};

DEVICE_DT_INST_DEFINE(0, kscan_csk6_init,
		      NULL,
		      &keysense_csk6_data_0,
		      &keysense_csk6_cfg_0, PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      &kscan_csk6_api);


