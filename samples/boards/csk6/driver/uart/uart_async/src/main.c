/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include <devicetree.h>
#include <drivers/uart.h>
#include <kernel.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);

K_SEM_DEFINE(tx_done, 0, 1);
K_SEM_DEFINE(rx_rdy, 0, 1);

#define UART_ASYNC_SAMPLE_DEVICE_NAME DT_LABEL(DT_NODELABEL(uart1))

static void uart_async_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);

	const uint8_t *events_name[] = {
		"tx done",
		"tx abort",
		"rx ready",
		"rx buf request",
		"rx buf released",
		"rx disabled",
	};

	printk("evt type: %d, event name:%s\n", evt->type, events_name[evt->type]);

	switch (evt->type) {
	case UART_TX_DONE:
		k_sem_give(&tx_done);
		break;
	case UART_RX_RDY:
		k_sem_give(&rx_rdy);
		break;
	default:
		break;
	}
}

static const uint8_t tx_buf[] = "uart async tx-rx sample";
static uint8_t rx_buf[sizeof(tx_buf)] __attribute__((section(".nocache"))) = {0};

void main(void)
{
	const struct device *uart_dev = device_get_binding(UART_ASYNC_SAMPLE_DEVICE_NAME);

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("uart device not ready!");
		return;
	}

	printk("Uart async tx-rx sample\n");

	memset(rx_buf, 0, sizeof(rx_buf));

	uart_callback_set(uart_dev, uart_async_callback, NULL);

	uart_rx_enable(uart_dev, rx_buf, sizeof(rx_buf), 50 * USEC_PER_MSEC);

	uart_tx(uart_dev, tx_buf, sizeof(tx_buf), 100 * USEC_PER_MSEC);

	if (k_sem_take(&tx_done, K_MSEC(100)) != 0) {
		LOG_ERR("TX_DONE timeout\n");
		return;
	}

	if (k_sem_take(&rx_rdy, K_MSEC(100)) != 0) {
		LOG_ERR("RX_RDY timeout\n");
		return;
	}

	if (memcmp(tx_buf, rx_buf, sizeof(rx_buf)) != 0) {
		printk("Data check failed\n");
		return;
	}

	printk("Uart async tx-rx complete\n");
}
