/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample echo app for CDC ACM class
 *
 * Sample app for USB CDC ACM class driver. The received data is echoed back
 * to the serial port.
 */

#include <stdio.h>
#include <string.h>
#include <device.h>
#include <drivers/uart.h>
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <hal/venus_ap.h>
#include "hal/gpio.h"
#include "hal/Driver_GPIO.h"
#include <hal/IOMuxManager.h>


#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;


/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
#define LED0	DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)
#else
/* A build error here means your board isn't set up to blink an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED0	""
#define PIN	0
#define FLAGS	0
#endif

static void output_init(void)
{
	const struct device *dev;
	bool led_is_on = true;
	int ret;

	dev = device_get_binding(LED0);
	if (dev == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (ret < 0) {
		return;
	}
}

__attribute__ ((section (".itcm"))) int csk_gpio_isr(void)
{

    uint32_t int_status;
	(*((GPIO_RegDef*)0x45900000)).REG_DATAOUT.all |= 1<<5;
    GPIO_RegDef *gpio_reg = (GPIO_RESOURCES *)0x45A00000;
    int_status = gpio_reg->REG_INTRSTATUS.all;
    // clear gpio interrupt status
    gpio_reg->REG_INTRSTATUS.all = int_status;
	(*((GPIO_RegDef*)0x45900000)).REG_DATAOUT.all &= ~(1<<5);
    return 0;
}



static void input_init(void){
	int ret;

	if (!device_is_ready(button.port)) {
		printk( "Error: button device %s is not ready\n", button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk( "Error %d: failed to configure %s pin %d\n", ret, button.port->name,
				button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_FALLING );
	if (ret != 0) {
		printk( "Error %d: failed to configure interrupt on %s pin %d\n", ret,
				button.port->name, button.pin);
		return;
	}

	// gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	// gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);


	printk(" button init success\n");
}

void gpio_init(void){
	output_init();
	input_init();
}



#define ARRAY_INDEX_MAX   (20)
Z_GENERIC_SECTION(.psram_section) static char psram_buffer[ARRAY_INDEX_MAX][32*1024];
Z_GENERIC_SECTION(.psram_section) static uint32_t ui32_psram_counter;
void main(void)
{
	ui32_psram_counter = 0;
	printk("start...\n");
	gpio_init();


	while(1){
		psram_buffer[ui32_psram_counter][0] += ui32_psram_counter;
		ui32_psram_counter++;
		if(ui32_psram_counter >= ARRAY_INDEX_MAX){
			ui32_psram_counter = 0;
		}
	}
}

