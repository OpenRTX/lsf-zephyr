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
#define IR0_DEV_NAME DT_LABEL(DT_ALIAS(ir))
#else
#error "can't find IR0 device"
#endif

// extern int z_impl_infrared_configure(struct device * dev, ir_config_t * config);

// __pinned_func
// static inline int infrared_configure(struct device * dev, ir_config_t * config)
// {
// 	compiler_barrier();
// 	return z_impl_infrared_configure(dev, config);
// }

void main(void)
{
	struct device *ir_dev;
	ir_config_t ir_config;
	ir_data_t ir_data;
	int ret;

	ir_dev = DEVICE_DT_GET(DT_NODELABEL(ir0));
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

	printk("\tIR Send\n");

	ir_data.addr = 0x12;
	ir_data.cmd = 0x34;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		printk("Failed to send infrared signal, error code %d\n", ret);
		return;
	}

	printk("\tIR Recv\n");

	ret = infrared_recv(ir_dev, &ir_data);

	ret = infrared_recv_timeout(ir_dev, &ir_data, 100);
	if (ret == 0) {
		printk("Received IR signal with address 0x%08X and command 0x%08X\n", ir_data.addr,
		       ir_data.cmd);
	} else {
		printk("Failed to receive infrared signal, error code %d\n", ret);
	}

	while (1) {
		k_msleep(1000);
	}
}
