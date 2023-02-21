/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/entropy.h>
#include <random/rand32.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

#define LED0 DT_GPIO_LABEL(DT_NODELABEL(green_led), gpios)
#define PIN DT_GPIO_PIN(DT_NODELABEL(green_led), gpios)

void main(void)
{
    const struct device *dev;
    bool led_is_on = true;
    int ret;

    printk("LED0 %s \n", LED0);
    printk("PIN %d \n", PIN);

    dev = device_get_binding(LED0);
    if (dev == NULL)
    {
        printk("LED0 device not found \n");
        return;
    }

    ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        printk("gpio config error \n");
        return;
    }

    while (1) {
        gpio_pin_set(dev, PIN, (int)led_is_on);
        led_is_on = !led_is_on;
        uint32_t to_mslepp = sys_rand32_get();
        to_mslepp = to_mslepp % 1000;
        to_mslepp = to_mslepp < 50 ? to_mslepp + 50 : to_mslepp;
        printk("to mslepp:%u\n", to_mslepp);
        // if (!led_is_on) {
        //     k_msleep(100);
        // } else {
        //     k_msleep(to_mslepp);
        // }
        k_msleep(to_mslepp);
    }
}
