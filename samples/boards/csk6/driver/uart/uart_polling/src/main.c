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
#define UART0		DT_NODELABEL(uart0)

void main(void)
{
	char rx_char;
	const struct device *uart = DEVICE_DT_GET(UART0);
	if(!device_is_ready(uart)){
		LOG_ERR("device:%s is not ready!", uart->name);
		return;
	}
	while(1){
		if(uart_poll_in(uart, &rx_char) == 0){/*read a char*/
            uart_poll_out(uart, rx_char);	  /*send a char*/           
        }	
			
	}
	return;
}

