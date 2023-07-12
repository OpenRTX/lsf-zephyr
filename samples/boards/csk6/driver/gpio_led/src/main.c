/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Driver_GPIO.h"
#include "Driver_GPT_PWM.h"
#include "IOMuxManager.h"
#include "cmn_sysctrl_reg_venus.h"
#include "gpt.h"
#include "gpt_reg_venus.h"
#include "venus_ap.h"

#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <zephyr.h>

#define CSK6001_GPTtest ((GPT_RegDef *)GPT_BASE)

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

#define LED0 DT_GPIO_LABEL(DT_NODELABEL(green_led), gpios)
#define PIN  DT_GPIO_PIN(DT_NODELABEL(green_led), gpios)

void main(void)
{
	const struct device *dev;
	bool led_is_on = true;

	int ret;
	uint8_t i;

	/* init PWM controler */
	ret = HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	if (ret) {
		printk("HAL_GPT_PWMInitialize failed, ret:%d\n", ret);
		return;
	}
	ret = HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);
	if (ret) {
		printk("HAL_GPT_PWMInitialize failed, ret:%d\n", ret);
		return;
	}

	/* pwm channel 0, GPIOA16 */
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, CSK_IOMUX_FUNC_ALTER11);
	/* pwm channel 1, GPIOA17 */
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, CSK_IOMUX_FUNC_ALTER11);
	/* pwm channel 2, GPIOA18 */
	IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, CSK_IOMUX_FUNC_ALTER11);

	/* init PWM channel */
	for (i = 0; i < 3; i++) {
		ret = HAL_GPT_PWMControl(
			GPT0_PWM(),
			CSK_GPT_PWM_MODE | CSK_GPT_PWM_CLKSRC_PCLK |
				CSK_GPT_PWM_OUTMODE_CENTRAL_ALIGNED | CSK_GPT_PWM_OUTPOLARITY_LOW |
				CSK_GPT_PWM_CLKDIV_1 | CSK_GPT_PWM_OPERATION_MODE_PWM,
			i);
		if (ret) {
			printk("HAL_GPT_PWMControl failed, channel:%d, ret:%d\n", i, ret);
			return;
		}

		ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), i, 96000, 50);
		if (ret) {
			printk("HAL_GPT_SetPWMFreqDuty failed, channel:%d, ret:%d\n", i, ret);
		}
	}

	CSK6001_GPTtest->SHADOW_LOAD = 0x707;

	HAL_GPT_EnablePWM(GPT0_PWM(), 0);
	HAL_GPT_EnablePWM(GPT0_PWM(), 1);
	HAL_GPT_EnablePWM(GPT0_PWM(), 2);
	__DSB();
	CSK6001_GPTtest->CH_CNT_EN |= 0x07;

	dev = device_get_binding(LED0);
	if (dev == NULL) {
		printk("LED0 device not found \n");
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("gpio config error \n");
		return;
	}
    
	/* Just call the following code where the frequency and duty cycle are changed */
	// HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), 0, 96000, 20);
	// HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), 1, 96000, 30);
	// HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), 2, 96000, 40);
	// CSK6001_GPTtest->SHADOW_LOAD = 0x707;
}
