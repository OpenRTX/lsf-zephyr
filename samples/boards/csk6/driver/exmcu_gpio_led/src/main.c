/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <zephyr.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

#define LED0 DT_GPIO_LABEL(DT_NODELABEL(green_led), gpios)
#define PIN  DT_GPIO_PIN(DT_NODELABEL(green_led), gpios)

typedef struct exgpio_test_pin {
	const struct device *dev;
	int pin;
} exgpio_test_pin_t;

typedef struct exgpio_test_group {
	exgpio_test_pin_t a;
	exgpio_test_pin_t b;
} exgpio_test_group_t;

#define EXGPIO_TEST_OUTPUT(status, fmt, ...)                                                       \
	printk("[%s][TEST][EXGPIO]:%s, " fmt "\n", (status) ? " PASS " : "FAILED", __FUNCTION__,   \
	       __VA_ARGS__)

#define EXGPIO_TEST_GROUP_DEFINE(dev1_name, pin1, dev2_name, pin2)                                 \
	{                                                                                          \
		.a =                                                                               \
			{                                                                          \
				.dev = DEVICE_DT_GET(DT_NODELABEL(dev1_name)),                     \
				.pin = pin1,                                                       \
			},                                                                         \
		.b = {.dev = DEVICE_DT_GET(DT_NODELABEL(dev2_name)),                               \
		      .pin = pin2,                                                                 \
		}                                                                                  \
	}

const static struct exgpio_test_group groups[] = {
	EXGPIO_TEST_GROUP_DEFINE(exgpioc, 0, exgpiod, 0),
	EXGPIO_TEST_GROUP_DEFINE(exgpioc, 2, exgpiod, 2),
	EXGPIO_TEST_GROUP_DEFINE(exgpioc, 3, exgpiod, 3),
	EXGPIO_TEST_GROUP_DEFINE(exgpioc, 4, exgpiod, 4),
};

static uint8_t exgpio_test_group_case_output_input(const struct device *dev_out, int out_pin,
						   const struct device *dev_in, int in_pin,
						   uint8_t test_level)
{
	gpio_pin_configure(dev_out, out_pin, GPIO_OUTPUT);
	gpio_pin_configure(dev_in, in_pin, GPIO_INPUT);
	gpio_pin_set_raw(dev_out, out_pin, test_level);

	/* sleep sometimes to wait exmcu detect level changed */
	k_msleep(10);
	int value = gpio_pin_get_raw(dev_in, in_pin);

	// printk("[TEST]%s%d-->%s%d, level:%s, STATUS: %s\n", dev_out->name, out_pin, dev_in->name,
	//        in_pin, test_level ? "HIGH" : "LOW", value == test_level ? "OK" : "FAIL");
	EXGPIO_TEST_OUTPUT(value == test_level, "%s%d--[%s]-->%s%d", dev_out->name,
			   out_pin, test_level ? "HIGH" : "LOW ", dev_in->name, in_pin);

	return value == test_level;
}

static void exgpio_test_group_pull_mode(const struct device *dev_in1, int in_pin1,
					const struct device *dev_in2, int in_pin2,
					uint8_t pull_mode)
{
	gpio_pin_configure(dev_in1, in_pin1,
			   GPIO_INPUT | (pull_mode ? GPIO_PULL_UP : GPIO_PULL_DOWN));
	gpio_pin_configure(dev_in2, in_pin2,
			   GPIO_INPUT | (pull_mode ? GPIO_PULL_UP : GPIO_PULL_DOWN));
	/* sleep sometimes to wait exmcu detect level changed */
	k_msleep(200);
	int value1 = gpio_pin_get_raw(dev_in1, in_pin1);
	int value2 = gpio_pin_get_raw(dev_in2, in_pin2);

	EXGPIO_TEST_OUTPUT(((value1 == value2) & (value1 == pull_mode)), "%s%d<--[%s]-->%s%d",
			   dev_in1->name, in_pin1, pull_mode ? " UP " : "DOWN", dev_in2->name,
			   in_pin2);
}

void exgpio_test_group(void)
{
	int i;
	uint8_t check = 1;

	for (i = 0; i < sizeof(groups) / sizeof(groups[0]); i++) {
		if (!device_is_ready(groups[i].a.dev)) {
			printk("group %d, device %s is not ready\n", i, groups[i].a.dev->name);
		}

		if (!device_is_ready(groups[i].b.dev)) {
			printk("group %d, device %s is not ready\n", i, groups[i].b.dev->name);
		}

		printk("\n-------GPIO,GROUP %d-------\n", i);
		check &= exgpio_test_group_case_output_input(groups[i].a.dev, groups[i].a.pin,
							     groups[i].b.dev, groups[i].b.pin, 1);
		check &= exgpio_test_group_case_output_input(groups[i].a.dev, groups[i].a.pin,
							     groups[i].b.dev, groups[i].b.pin, 0);
		check &= exgpio_test_group_case_output_input(groups[i].a.dev, groups[i].a.pin,
							     groups[i].b.dev, groups[i].b.pin, 1);

		check &= exgpio_test_group_case_output_input(groups[i].b.dev, groups[i].b.pin,
							     groups[i].a.dev, groups[i].a.pin, 1);
		check &= exgpio_test_group_case_output_input(groups[i].b.dev, groups[i].b.pin,
							     groups[i].a.dev, groups[i].a.pin, 0);
		check &= exgpio_test_group_case_output_input(groups[i].b.dev, groups[i].b.pin,
							     groups[i].a.dev, groups[i].a.pin, 1);
	}

	if (check) {
		printk("GPIO LOOPBACK TEST OK\n");
	}
}

void exgpio_pull_mode_test(void)
{
	int i;
	for (i = 0; i < sizeof(groups) / sizeof(groups[0]); i++) {
		if (!device_is_ready(groups[i].a.dev)) {
			printk("group %d, device %s is not ready\n", i, groups[i].a.dev->name);
		}

		if (!device_is_ready(groups[i].b.dev)) {
			printk("group %d, device %s is not ready\n", i, groups[i].b.dev->name);
		}

		printk("\n-------PULL,GROUP %d-------\n", i);
		exgpio_test_group_pull_mode(groups[i].a.dev, groups[i].a.pin, groups[i].b.dev,
					    groups[i].b.pin, 0);
		exgpio_test_group_pull_mode(groups[i].a.dev, groups[i].a.pin, groups[i].b.dev,
					    groups[i].b.pin, 1);
		exgpio_test_group_pull_mode(groups[i].a.dev, groups[i].a.pin, groups[i].b.dev,
					    groups[i].b.pin, 0);
		exgpio_test_group_pull_mode(groups[i].a.dev, groups[i].a.pin, groups[i].b.dev,
					    groups[i].b.pin, 1);
		exgpio_test_group_pull_mode(groups[i].a.dev, groups[i].a.pin, groups[i].b.dev,
					    groups[i].b.pin, 0);
	}
}

void main(void)
{
	printk("exmcu gpio led sample\n");
	exgpio_test_group();
	exgpio_pull_mode_test();
}
