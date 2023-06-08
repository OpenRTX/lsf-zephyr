/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <zephyr.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

#define LED0 DT_GPIO_LABEL(DT_NODELABEL(green_led), gpios)
#define PIN  DT_GPIO_PIN(DT_NODELABEL(green_led), gpios)

void main(void)
{
	const struct device *dev;
	bool led_is_on = true;
	int ret;

	printk("LED0 %s \n", LED0);
	printk("PIN %d \n", PIN);

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
	// DT_INST(inst, DT_DRV_COMPAT)
	const struct pwm_dt_spec spec = PWM_DT_SPEC_GET(DT_NODELABEL(motor_pwm));

	if (!device_is_ready(spec.dev)) {
		printk("device: %s is not ready\n", spec.dev->name);
	}

	printk("name: %s \n", spec.dev->name);
	printk("channel: %d \n", spec.channel);
	printk("period: %d \n", spec.period);
	printk("flag: %d \n", spec.flags);

	int r = pwm_set_dt(&spec, 1000 * 1000 * 1000, 500 * 1000 * 1000);

	if (r) {
		printk("pwm set dt failed, r:%d\n", r);
	}

	while (1) {
		gpio_pin_set(dev, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);
	}
}
