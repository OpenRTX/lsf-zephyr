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

void Test_IR_Hardware_PROTOCOL_NEC(const struct device *ir_dev)
{
	/**
	 * @brief IR Remote Control NEC Protocol Frame Format:
	 * 	Each frame consists of:
	 * 	a. A start bit
	 * 	b. An 8-bit address code
	 * 	c. The 8-bit ones' complement of the address code (address code's inverse)
	 * 	d. An 8-bit data code
	 * 	e. The 8-bit ones' complement of the data code (data code's inverse)
	 * 	f. An 8-bit checksum (address code + data code, ignoring carries)
	 * NEC:
	 * xxx┌───────┐   ╱ Address (8bit) ╲╱  Address (8bit)   ╲╱ Coommand(8bit) ╲╱ Coommand(8bit)    ╲xxxxx
	 * xxx┘       └───╲                ╱╲ (Logical Inverse) ╱╲                ╱╲ (Logical Inverse) ╱xxxxx
	 *
	 */
	int ret;
	ir_config_t ir_config;
	ir_data_t ir_data;
	ir_config.protocol = IR_PROTOCOL_NEC;
	ir_config.freq = IR_PROTOCOL_FREQ_DEFAULT;
	ir_config.timeout = 100;
	ret = infrared_configure(ir_dev, &ir_config);
	if (ret != 0) {
		LOG_ERR("Failed to configure infrared device, error code %d\n", ret);
		return;
	}

	LOG_INF("PROTOCOL_NEC: 1.IR Send");
	ir_data.addr = 0x12ED;
	ir_data.cmd = 0x34CB;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	/**
	 * @note:
	 * 	Sending a frame of data requires an interval of time (greater than 67.5ms)
	 * 	before continuing to send the next frame of data.
	 * 	Otherwise, the device will return an error code of busy (-IR_ERR_BUSY).
	 */
	k_sleep(K_MSEC(100));
	LOG_INF("PROTOCOL_NEC: 2.IR Send");

	ir_data.addr = 0x56A9;
	ir_data.cmd = 0x7887;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	LOG_INF("PROTOCOL_NEC: 3.IR Receive");
	ret = infrared_recv_timeout(ir_dev, &ir_data, K_MSEC(1000));
	if (ret == 0) {
		LOG_ERR("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
		       ir_data.cmd);
	} else if (ret == -IR_ERR_TIMEOUT) {
		LOG_WRN("Failed to receive infrared signal, timeout\n");
	} else {
		LOG_WRN("Failed to receive infrared signal, error code %d\n", ret);
	}

	LOG_INF("PROTOCOL_NEC: 4.IR Receive");
	ret = infrared_recv(ir_dev, &ir_data);
	if (ret == 0) {
		LOG_INF("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
		ir_data.cmd);
	}
}


void Test_IR_Hardware_PROTOCOL_TOSHIBA_9012(const struct device *ir_dev)
{
	int ret;
	ir_config_t ir_config;
	ir_data_t ir_data;
	ir_config.protocol = IR_PROTOCOL_TOSHIBA_9012;
	ir_config.freq = IR_PROTOCOL_FREQ_DEFAULT;
	ir_config.timeout = 100;
	ret = infrared_configure(ir_dev, &ir_config);
	if (ret != 0) {
		LOG_ERR("Failed to configure infrared device, error code %d\n", ret);
		return;
	}

	LOG_INF("IR_PROTOCOL_TOSHIBA_9012: 1.IR Send");
	ir_data.addr = 0x1234;
	ir_data.cmd = 0x5678;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	LOG_INF("IR_PROTOCOL_TOSHIBA_9012: 2.IR Send");
	k_sleep(K_MSEC(100));

	ir_data.addr = 0x56A9;
	ir_data.cmd = 0x7887;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	LOG_INF("IR_PROTOCOL_TOSHIBA_9012: 3.IR Receive");
	ret = infrared_recv_timeout(ir_dev, &ir_data, K_MSEC(1000));
	if (ret == 0) {
		LOG_ERR("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
		       ir_data.cmd);
	} else if (ret == -IR_ERR_TIMEOUT) {
		LOG_WRN("Failed to receive infrared signal, timeout\n");
	} else {
		LOG_WRN("Failed to receive infrared signal, error code %d\n", ret);
	}

	LOG_INF("IR_PROTOCOL_TOSHIBA_9012: 4.IR Receive");
	ret = infrared_recv(ir_dev, &ir_data);
	if (ret == 0) {
		LOG_INF("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
		ir_data.cmd);
	}
}

void Test_IR_Hardware_PROTOCOL_PHILIPS_RC5(const struct device *ir_dev)
{
	int ret;
	ir_config_t ir_config;
	ir_data_t ir_data;
	ir_config.protocol = IR_PROTOCOL_PHILIPS_RC5;
	ir_config.freq = IR_PROTOCOL_FREQ_DEFAULT;
	ir_config.timeout = 100;
	ret = infrared_configure(ir_dev, &ir_config);
	if (ret != 0) {
		LOG_ERR("Failed to configure infrared device, error code %d\n", ret);
		return;
	}

	LOG_INF("IR_PROTOCOL_PHILIPS_RC5: 1.IR Send");
	ir_data.addr = 0x16;	//0b0101_0110
	ir_data.cmd = 0x03; 	//0b0100_0011
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	LOG_INF("IR_PROTOCOL_PHILIPS_RC5: 2.IR Send");
	k_sleep(K_MSEC(100));

	ir_data.addr = 0x56;
	ir_data.cmd = 0x43;
	ret = infrared_send(ir_dev, &ir_data);
	if (ret != 0) {
		LOG_ERR("Failed to send infrared signal, error code %d\n", ret);
	}

	LOG_INF("IR_PROTOCOL_PHILIPS_RC5: 3.IR Receive");
	ret = infrared_recv_timeout(ir_dev, &ir_data, K_MSEC(1000));
	if (ret == 0) {
		LOG_ERR("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
		       ir_data.cmd);
	} else if (ret == -IR_ERR_TIMEOUT) {
		LOG_WRN("Failed to receive infrared signal, timeout\n");
	} else {
		LOG_WRN("Failed to receive infrared signal, error code %d\n", ret);
	}

	LOG_INF("IR_PROTOCOL_PHILIPS_RC5: 4.IR Receive");
	ret = infrared_recv(ir_dev, &ir_data);
	if (ret == 0) {
		LOG_INF("Received IR signal with address 0x%02X and command 0x%02X", ir_data.addr,
		ir_data.cmd);
	}
}

void main(void)
{
	const struct device *ir_dev;


	ir_dev = DEVICE_DT_GET(IR0_DEV_NAME);
	if (ir_dev == NULL) {
		LOG_ERR("Failed to get infrared device");
		return;
	}

	LOG_INF("IR Sample Start");
	// Test_IR_Hardware_PROTOCOL_NEC(ir_dev);
	// Test_IR_Hardware_PROTOCOL_TOSHIBA_9012(ir_dev);
	Test_IR_Hardware_PROTOCOL_PHILIPS_RC5(ir_dev);
}

