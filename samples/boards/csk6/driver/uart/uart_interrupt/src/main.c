/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <kernel.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <string.h>
#include <devicetree.h>
#include <logging/log.h>
#include <drivers/uart.h>
LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);
char tx_msg[] = "1234abcd\n";
#define UART0		DT_NODELABEL(uart0)
uint8_t rx_buffer[100];

static void uart_rx_callback(const struct device *dev, void *user_data){
	uart_irq_update(dev);					// 进入中断处理调用
	while (uart_irq_is_pending(dev)) {		// 查询是否有中断挂起
		if (uart_irq_rx_ready(dev)) {
			int recv_len;
			recv_len = uart_fifo_read(dev, rx_buffer, 1);	// 读取字符串
			uart_fifo_fill(dev, rx_buffer, recv_len);		// 发送字符串
		}
	}
}


void main(void)
{
	const struct device *uart = DEVICE_DT_GET(UART0);
	if(!device_is_ready(uart)){
		LOG_ERR("device:%s is not ready!", uart->name);
		return;
	}

	LOG_INF("devices %s is ready", uart->name);

	/* Define the application callback function */
	uart_irq_callback_user_data_set(uart, uart_rx_callback, NULL);

	/* Enable rx interrupts */
	uart_irq_rx_enable(uart);
	uart_irq_tx_enable(uart);

	while(1){
		k_msleep(20);
	}
	return;
}


// static void direct_uart_callback(const struct device *dev, void *user_data)
// {
// 	static uint8_t buf[1];
// 	static bool tx_busy;

// 	uart_irq_update(dev);


// 	if (uart_irq_rx_ready(dev)) {
// 		while (uart_fifo_read(dev, buf, sizeof(buf))) {
// 			if (!tx_busy) {
// 				uart_irq_tx_enable(dev);
// 			}
// 		}
// 	}

// 	if (uart_irq_tx_ready(dev)) {
// 		if (!tx_busy) {
// 			(void)uart_fifo_fill(dev, buf, sizeof(buf));
// 			tx_busy = true;
// 		} else {
// 			tx_busy = false;
// 			uart_irq_tx_disable(dev);
// 			if (buf[0] == 'x') {
// 				uart_irq_rx_disable(dev);
// 				shell_reinit_trigger();
// 			}
// 		}
// 	}
// }