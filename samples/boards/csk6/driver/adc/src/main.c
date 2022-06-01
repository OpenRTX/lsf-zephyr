/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/adc.h>

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define ADC_NUM_CHANNELS	DT_PROP_LEN(DT_PATH(zephyr_user), io_channels)

#if ADC_NUM_CHANNELS > 3
#error "Currently only 1 or 2 channels supported in this sample"
#endif

#define ADC_NODE		DT_PHANDLE(DT_PATH(zephyr_user), io_channels)

/* Common settings supported by most ADCs */
#define ADC_RESOLUTION		11
#define ADC_GAIN		ADC_GAIN_1
#define ADC_REFERENCE		ADC_REF_INTERNAL

/* Get the numbers of up to two channels */
static uint8_t channel_ids[ADC_NUM_CHANNELS] = {
	DT_IO_CHANNELS_INPUT_BY_IDX(DT_PATH(zephyr_user), 0),
#if ADC_NUM_CHANNELS == 2
	DT_IO_CHANNELS_INPUT_BY_IDX(DT_PATH(zephyr_user), 1)
#endif

#if ADC_NUM_CHANNELS == 3
	DT_IO_CHANNELS_INPUT_BY_IDX(DT_PATH(zephyr_user), 1),
	DT_IO_CHANNELS_INPUT_BY_IDX(DT_PATH(zephyr_user), 2)
#endif
};

static int16_t sample_buffer[ADC_NUM_CHANNELS];

struct adc_channel_cfg channel_cfg = {
	.gain = ADC_GAIN_1,
	.reference = ADC_REFERENCE,
	.channel_id = 0,
};

struct adc_sequence sequence = {
	/* individual channels will be added below */
	.channels    = 0,
	.buffer      = sample_buffer,
	/* buffer size in bytes, not number of samples */
	.buffer_size = sizeof(sample_buffer),
	.resolution  = ADC_RESOLUTION,
};

void main(void)
{
	int err;
	const struct device *dev_adc = DEVICE_DT_GET(ADC_NODE);

	printk("ADC Sample start\n");

	if (!device_is_ready(dev_adc)) {
		printk("ADC device not found\n");
		return;
	}

	/*
	 * Configure channels individually prior to sampling
	 */
	for (uint8_t i = 0; i < ADC_NUM_CHANNELS; i++) {
		channel_cfg.channel_id = channel_ids[i];

		adc_channel_setup(dev_adc, &channel_cfg);

		sequence.channels |= BIT(channel_ids[i]);
	}

	int32_t adc_vref = adc_ref_internal(dev_adc);

	printk("adc_vref %d.\n", adc_vref);

	while (1) {
		/*
		 * Read sequence of channels (fails if not supported by MCU)
		 */
		err = adc_read(dev_adc, &sequence);
		if (err != 0) {
			printk("ADC reading failed with error %d.\n", err);
			return;
		}

		for (uint8_t i = 0; i < ADC_NUM_CHANNELS; i++) {
			int32_t raw_value = sample_buffer[i];

			if (adc_vref > 0) {
				/*
				 * Convert raw reading to millivolts if driver
				 * supports reading of ADC reference voltage
				 */
				int32_t mv_value = raw_value - 2048;

				adc_raw_to_millivolts(adc_vref, ADC_GAIN_1,
					ADC_RESOLUTION, &mv_value);
				printk(" adc(%d)= %d mV  ", raw_value, mv_value);
			}
		}
		printk("\n");

		k_sleep(K_MSEC(5000));
	}
}
