/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/i2c.h>
#include <string.h>

#if DT_NODE_HAS_STATUS(DT_ALIAS(i2c_0), okay)
#define I2C0_DEV_NAME DT_LABEL(DT_ALIAS(i2c_0))
#else
#error "can't find I2C0 device"
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(i2c_1), okay)
#define I2C1_DEV_NAME DT_LABEL(DT_ALIAS(i2c_1))
#else
#error "can't find I2C1 device"
#endif

#define SLAVE_ADDRESS (0x35) /* csk6 default i2c slave address 0x35 */
uint32_t i2c0_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER;
uint32_t i2c1_cfg = 0;

const struct device *i2c0_dev = NULL;
const struct device *i2c1_dev = NULL;

#define MASTER_STACK_SIZE 4096
#define SLAVE_STACK_SIZE 4096

K_THREAD_STACK_DEFINE(master_stack_area, MASTER_STACK_SIZE);
K_THREAD_STACK_DEFINE(slave_stack_area, SLAVE_STACK_SIZE);

struct k_thread master_thread_data;
struct k_thread slave_thread_data;

uint8_t data0[256] = { 0 };
uint8_t data1[256] = { 0 };

void i2c_master_init(void)
{
	i2c0_dev = device_get_binding(I2C0_DEV_NAME);
	if (i2c0_dev == NULL) {
		printk("I2C0: Device is not found.\n");
	}

	if (!device_is_ready(i2c0_dev)) {
		printk("I2C0: Device is not ready.\n");
		return;
	}

	/* i2c master configure */
	if (i2c_configure(i2c0_dev, i2c0_cfg)) {
		printk("I2C0 config failed\n");
		return;
	}
}

void i2c_slave_init(void)
{
	i2c1_dev = device_get_binding(I2C1_DEV_NAME);
	if (i2c1_dev == NULL) {
		printk("I2C1: Device is not found.\n");
	}

	if (!device_is_ready(i2c1_dev)) {
		printk("I2C1: Device is not ready.\n");
		return;
	}

	/* i2c slave configure */
	if (i2c_configure(i2c1_dev, i2c1_cfg)) {
		printk("I2C1 config failed\n");
		return;
	}
}

void i2c_master_thread(void *v1, void *v2, void *v3)
{
	for (int i = 0; i < sizeof(data0) - 1; i++) {
		data0[i] = i;
	}

	/* 1. master send, slave receive */
	i2c_write(i2c0_dev, data0, sizeof(data0) - 1, SLAVE_ADDRESS);

	/* 2. master receive, slave send */
	i2c_read(i2c0_dev, data0, sizeof(data0), SLAVE_ADDRESS);

	for (int i = 0; i < sizeof(data1); i++) {
		if (data0[i] != data1[i]) {
			printk("data0[%d]: %d != data1[%d]: %d\n", i, data0[i], i, data1[i]);
		}
	}

	printk("I2C TEST DONE\n");

	while (1) {
		k_msleep(10);
	}
}

void i2c_slave_thread(void *v1, void *v2, void *v3)
{
	/* 1. master send, alave receive */
	printk("master send, slave receive\n");
	i2c_read(i2c1_dev, data1, sizeof(data1), 0);

	/* data compare */
	for (int i = 0; i < sizeof(data1); i++) {
		if (data0[i] != data1[i]) {
			printk("data0[%d]: %d != data1[%d]: %d\n", i, data0[i], i, data1[i]);
		}
	}

	/* reset data value */
	memset(data0, sizeof(data0), 0);
	for (int i = 0; i < sizeof(data1); i++) {
		data1[i] = i;
	}

	/* 2. master recevice, alave send */
	printk("master receive, slave send\n");
	i2c_write(i2c1_dev, data1, sizeof(data1) - 1, 0);

	while (1) {
		k_msleep(10);
	}
}

void main(void)
{
	printk("I2C START TEST\n");

	/* i2c master and slave initialize */
	i2c_master_init();
	i2c_slave_init();

	/* create master and slave thread */
	int pri = k_thread_priority_get(k_current_get());

	k_thread_create(&slave_thread_data, slave_stack_area,
					K_THREAD_STACK_SIZEOF(slave_stack_area),
					i2c_slave_thread, NULL, NULL, NULL, pri, 0, K_NO_WAIT);

	k_thread_create(&master_thread_data, master_stack_area,
					K_THREAD_STACK_SIZEOF(master_stack_area), i2c_master_thread, NULL,
					NULL, NULL, pri, 0, K_NO_WAIT);

	printk("i2c thread creaeted\n");

	while (1) {
		k_msleep(10);
	}
}
