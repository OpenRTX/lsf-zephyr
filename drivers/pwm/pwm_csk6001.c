/*
 * Copyright (c) 2021 listenai Systems (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_pwm

#include <errno.h>

#include <soc.h>
#include <soc_dt.h>
#include <drivers/pwm.h>
#include <drivers/pinmux.h>
#include <device.h>
#include <kernel.h>
#include <init.h>
#include "hal/Driver_GPT_PWM.h"
#include "hal/venus_ap.h"
#include "hal/reg/cmn_sysctrl_reg_venus.h"
#include <logging/log.h>
#include "hal/gpt.h"

LOG_MODULE_REGISTER(pwm_csk6001, CONFIG_PWM_LOG_LEVEL);

#define MAX_PWM_CHANNEL  8


/** PWM configuration. */
struct pwm_csk_config {
	uint8_t clock_source;
	uint8_t clock_prescaler;
	uint8_t mode;
	/* PWM alternate configuration */
    const struct csk6_pinctrl_cfg *pinctrl_list;
    uint8_t pinctrl_list_size;
};

static uint32_t get_polarity(pwm_flags_t flags)
{
	if ((flags & PWM_POLARITY_MASK) == PWM_POLARITY_NORMAL) {
		return CSK_GPT_PWM_OUTPOLARITY_HIGH;
	}

	return CSK_GPT_PWM_OUTPOLARITY_LOW;
}

static inline uint32_t get_clock_source(uint8_t clock_source)
{

	return (3 - clock_source) << CSK_GPT_PWM_CLKSRC_Pos;
}

static inline uint32_t get_mode(uint8_t mode)
{
	return (mode - 1) << CSK_GPT_PWM_OUTMODE_Pos;
}

static uint32_t get_clock_prescaler(uint8_t clock_prescaler)
{
	uint8_t bit = 0;
	while(clock_prescaler){
		if (clock_prescaler & 0x01){
			break;
		}
		bit++;
		clock_prescaler = clock_prescaler >> 1;
	}
	return bit;
}

static int pwm_csk_get_cycles_per_sec(const struct device *dev,
					uint32_t pwm,
					uint64_t *cycles)
{
	const struct pwm_csk_config *cfg = dev->config;
	uint64_t clockfreq[4] = {0, PCLOCK, CLOCK_EXT, CLOCK_XTAL};
	*cycles = clockfreq[cfg->clock_source] >> get_clock_prescaler(cfg->clock_prescaler) ;
	return 0;
}

static int pwm_csk_pin_set(const struct device *dev, uint32_t pwm,
			     uint32_t period_cycles, uint32_t pulse_cycles,
			     pwm_flags_t flags)
{	
	uint8_t duty_cycle;
	int32_t freq;
	uint64_t cycles_per_sec;
	int ret;
	const struct pwm_csk_config *cfg = dev->config;
	if (pwm > MAX_PWM_CHANNEL) {
		LOG_ERR("Invalid channel (%d)", pwm);
		return -EINVAL;
	}

	if (pulse_cycles > period_cycles) {
		LOG_ERR("Invalid combination of pulse and period cycles");
		return -EINVAL;
	}

	duty_cycle = 100.0 * pulse_cycles / period_cycles;
	/* period_cycles to period */
	pwm_csk_get_cycles_per_sec(dev, pwm, &cycles_per_sec);
	/* period to freq */
	freq = cycles_per_sec / period_cycles;

	HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | get_clock_prescaler(cfg->clock_prescaler) << CSK_GPT_PWM_CLKDIV_Pos | 
		get_clock_source(cfg->clock_source) | get_polarity(flags) | get_mode(cfg->mode) | 
		CSK_GPT_PWM_OPERATION_MODE_PWM, pwm);
	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), pwm, freq, duty_cycle);
	if (ret != CSK_DRIVER_OK){
		return -EIO;
	}

	ret = HAL_GPT_EnablePWM(GPT0_PWM(), pwm);
	if (ret != CSK_DRIVER_OK){
		return -EIO;
	}
	return 0;
}


static const struct pwm_driver_api pwm_csk_driver_api = {
	.pin_set = pwm_csk_pin_set,
	.get_cycles_per_sec = pwm_csk_get_cycles_per_sec,
};


static int pwm_csk_init(const struct device *dev)
{
	const struct pwm_csk_config *config = dev->config;
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);

	PINCTRL_CONFIG_PINS(config->pinctrl_list, config->pinctrl_list_size);
	
	return 0;
}

#define PWM_DEVICE_INIT(index)                                                 	\
																				\
	static const struct csk6_pinctrl_cfg                                        \
        pwm_alts_##index[DT_INST_NUM_PINCTRLS_BY_IDX(index, 0)] =               \
            CSK6_DT_ALT_ITEMS_LIST(index);                                      \
									               								\
	static const struct pwm_csk_config pwm_csk_config_##index = {      			\
		.clock_source = DT_INST_PROP(index, clock_source),                		\
		.clock_prescaler = DT_INST_PROP(index, clock_prescaler),           		\
		.mode = DT_INST_PROP(index, mode),           							\
        .pinctrl_list = pwm_alts_##index,                   					\
		.pinctrl_list_size = ARRAY_SIZE(pwm_alts_##index),                      \
	};                                                                     		\
																				\
	DEVICE_DT_INST_DEFINE(index, &pwm_csk_init, NULL,                    		\
			    NULL,                           								\
			    &pwm_csk_config_##index, POST_KERNEL,            				\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                				\
			    &pwm_csk_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_DEVICE_INIT)
