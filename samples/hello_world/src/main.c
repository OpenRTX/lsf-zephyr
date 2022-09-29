/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/zephyr.h>

uint8_t test_msg[] =  "12345678901234567890123456";
uint8_t test_msg1[] = "abcdefghijklmnopqrstabcdef";
uint8_t test_rx[128] __attribute__((__section__(".nocache"))) = {0};

K_SEM_DEFINE(sem, 0, 1);

void dump(uint8_t *buf, uint32_t len)
{
	while (len--) {
		printk("%02X ", *buf++);
	}
	printk("\n");
}

void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	const uint8_t *types[] = {
		"tx_done",	   "tx_abort",	 "rx_ready", "rx_buf_request",
		"rx_buf_released", "rx_disable", "rx_stop",
	};
	printk("uart_callback, evt type:%s\n", types[evt->type]);
	if (evt->type == UART_RX_RDY || evt->type == UART_RX_BUF_RELEASED) {
		printk("evt->data.rx.buf:%p, buf:%p\n", evt->data.rx.buf, test_rx);
		k_sem_give(&sem);
	}
}

void main(void)
{
	int err;

	printk("Hello World! %s\n", CONFIG_BOARD);
	const struct device *device = device_get_binding(DT_LABEL(DT_NODELABEL(uart1)));
	memset(test_rx, 0, sizeof(test_rx));
	if (!device_is_ready(device)) {
		printk("device is not ready\n");
		return;
	}
	err = uart_callback_set(device, uart_callback, NULL);
	if (err != 0) {
		printk("uart_callback_set failed: %d\n", (err));
		return;
	}

	printk("rx target size: %d\n", strlen(test_msg) * 3);
	err = uart_rx_enable(device, test_rx, strlen(test_msg) * 3, 1000 * 1000);
	if (err != 0) {
		printk("uart_rx_enable failed: %d\n", (err));
	}

	printk("tx size: %d\n", strlen(test_msg));
	err = uart_tx(device, test_msg, strlen(test_msg), 1000);
	if (err != 0) {
		printk("uart_tx failed: %d\n", (err));
	}

	// while (1) {
	/* timeout */
	k_sem_take(&sem, K_FOREVER);
	k_msleep(1000);
	printk("recv: %s.\n", test_rx);
	dump(test_rx, strlen(test_msg) * 3);
	/* send again */
	printk("tx size: %d\n", strlen(test_msg));
	err = uart_tx(device, test_msg1, strlen(test_msg), 100 * 1000);
	if (err != 0) {
		printk("uart_tx failed: %d\n", (err));
	}
	/* rx complete */
	// k_sem_take(&sem, K_FOREVER);
	k_msleep(1000);
	printk("recv: %s.\n", test_rx);
	dump(test_rx, strlen(test_msg) * 3);
	printk("tx size: %d\n", strlen(test_msg));
	err = uart_tx(device, test_msg1, strlen(test_msg), 100 * 1000);
	if (err != 0) {
		printk("uart_tx failed: %d\n", (err));
	}
	k_msleep(1000);
	printk("recv: %s.\n", test_rx);
	dump(test_rx, strlen(test_msg) * 3);
	// }
}
