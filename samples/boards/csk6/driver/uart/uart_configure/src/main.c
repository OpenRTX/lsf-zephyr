/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include <devicetree.h>
#include <drivers/uart.h>
#include <kernel.h>
#include <sys/printk.h>
#include <zephyr.h>

#define DELAY_TIME_SEC 6

void print_uart_config(const struct uart_config *cfg)
{
	printk("baudrate: %d\n", cfg->baudrate);
	printk("data_bits: %d\n", cfg->data_bits);
	printk("stop_bits: %d\n", cfg->stop_bits);
	printk("flow_ctrl: %d\n", cfg->flow_ctrl);
	printk("parity: %d\n", cfg->parity);
}

void main(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	struct uart_config cfg = {
		.baudrate = 9600,
		.data_bits = UART_CFG_DATA_BITS_8,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
		.parity = UART_CFG_PARITY_NONE,
	};

	if (!device_is_ready(dev)) {
		printk("device:%s is not ready!\n", dev->name);
		return;
	}
	struct uart_config read_cfg = {0};

	if (uart_config_get(dev, &read_cfg) != 0) {
		printk("uart config get failed.\n");
		return;
	}

	printk("current uart configuration:\n");
	print_uart_config(&read_cfg);

	printk("new uart configuration:\n");
	print_uart_config(&cfg);

	int c = DELAY_TIME_SEC;
	while (c) {
		printk("new configuration will configed after %d sec.\n", c--);
		k_msleep(1000);
	}

	if (uart_configure(dev, &cfg) != 0) {
		printk("uart configure failed.\n");
		return;
	}

	if (uart_config_get(dev, &read_cfg) != 0) {
		printk("uart config get failed.\n");
		return;
	}

	printk("uart configuration succeeded.\n");
	printk("config result:\n");
	print_uart_config(&read_cfg);

	while (1) {
		printk("hello world\n");
		k_msleep(500);
	}
}
