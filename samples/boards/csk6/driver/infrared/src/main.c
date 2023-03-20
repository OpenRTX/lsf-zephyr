/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <zephyr.h>

#include <zephyr/device.h>
#include <zephyr/drivers/infrared.h>
#include <zephyr/sys/printk.h>

#define IR_SEND_BUFFER_SIZE 50

uint8_t ir_send_buffer[IR_SEND_BUFFER_SIZE];

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ir0), okay)
#define IR0_DEV_NAME DT_NODELABEL(ir0)
#else
#error "can't find IR0 device"
#endif


void main(void)
{
	const struct device *ir_dev;
	ir_config_t ir_config;
	ir_data_t ir_data;
	int ret;

	ir_dev = DEVICE_DT_GET(IR0_DEV_NAME);
	if (ir_dev == NULL) {
		printk("Failed to get infrared device\n");
		return;
	}

	printk("\tIR Sample Start\n");

	ir_config.mode = IR_PROTOCOL_NEC;
	ir_config.freq = IR_PROTOCOL_FREQ_DEFAULT;
	ir_config.timeout = 100;
	ret = infrared_configure(ir_dev, &ir_config);
	if (ret != 0) {
		printk("Failed to configure infrared device, error code %d\n", ret);
		return;
	}

	printk("\t1.IR Send\n");

	ir_data.addr = 0x12ED;
	ir_data.cmd = 0x34CB;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		printk("Failed to send infrared signal, error code %d\n", ret);
		// return;
	}
	printk("\t2.IR Send\n");

	ir_data.addr = 0x56A9;
	ir_data.cmd = 0x7887;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		printk("Failed to send infrared signal, error code %d\n", ret);
		// return ;
	}

	k_msleep(100);
	printk("\tIR Recv\n");
	while (1)
	{
		ret = infrared_recv(ir_dev, &ir_data);
		if (ret == 0) {
			printk("Received IR signal with address 0x%08X and command 0x%08X\n", ir_data.addr,
			ir_data.cmd);
			// break;
		} 
		// else {
		// 	printk("Failed to receive infrared signal, error code %d\n", ret);
		// }
		k_msleep(2000);
	}

	ret = infrared_recv_timeout(ir_dev, &ir_data, 100);
	if (ret == 0) {
		printk("Received IR signal with address 0x%08X and command 0x%08X\n", ir_data.addr,
		       ir_data.cmd);
	} else {
		printk("Failed to receive infrared signal, error code %d\n", ret);
	}

	while (1) {
		printk(".\n");
		k_msleep(2000);
	}
}
