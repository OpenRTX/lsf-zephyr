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

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(Infrared, LOG_LEVEL_INF);

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
		LOG_ERR("Failed to get infrared device");
		return;
	}

	LOG_INF("IR Sample Start");

	ir_config.protocol = IR_PROTOCOL_NEC;
	ir_config.freq = IR_PROTOCOL_FREQ_DEFAULT;
	ir_config.timeout = 100;
	ret = infrared_configure(ir_dev, &ir_config);
	if (ret != 0) {
		LOG_ERR("Failed to configure infrared device, error code %d\n", ret);
		return;
	}

	LOG_INF("1.IR Send");
	ir_data.addr = 0x12ED;
	ir_data.cmd = 0x34CB;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	LOG_INF("2.IR Send");
	k_sleep(K_MSEC(100));

	ir_data.addr = 0x56A9;
	ir_data.cmd = 0x7887;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	LOG_INF("3.IR Rece");
	ret = infrared_recv_timeout(ir_dev, &ir_data, K_MSEC(1000));
	if (ret == 0) {
		LOG_ERR("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
		       ir_data.cmd);
	}

	while (1) {
		ret = infrared_recv(ir_dev, &ir_data);
		if (ret == 0) {
			LOG_INF("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
			ir_data.cmd);
		}
	}
}
