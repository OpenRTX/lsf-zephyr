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

uint8_t master_buf[256] = { 0 };
uint8_t slave_buf[256] = { 0 };

int i2c_master_init(void)
{
	i2c0_dev = device_get_binding(I2C0_DEV_NAME);
	if (i2c0_dev == NULL) {
		printk("I2C0: Device is not found.\n");
		return -ENODEV;
	}

	if (!device_is_ready(i2c0_dev)) {
		printk("I2C0: Device is not ready.\n");
		return -ENODEV;
	}

	/* i2c master configure */
	if (i2c_configure(i2c0_dev, i2c0_cfg)) {
		printk("I2C0 config failed\n");
		return -EINVAL;
	}

	return 0;
}

int i2c_slave_init(void)
{
	i2c1_dev = device_get_binding(I2C1_DEV_NAME);
	if (i2c1_dev == NULL) {
		printk("I2C1: Device is not found.\n");
		return -ENODEV;
	}

	if (!device_is_ready(i2c1_dev)) {
		printk("I2C1: Device is not ready.\n");
		return -ENODEV;
	}

	/* i2c slave configure */
	if (i2c_configure(i2c1_dev, i2c1_cfg)) {
		printk("I2C1 config failed\n");
		return -EINVAL;
	}

	return 0;
}

K_SEM_DEFINE(i2c_sem_m2s, 0, 1);
K_SEM_DEFINE(i2c_sem_s2m, 0, 1);

void i2c_master_thread(void *v1, void *v2, void *v3)
{
	k_sem_take(&i2c_sem_s2m, K_FOREVER);

	for (int i = 0; i < sizeof(master_buf) - 1; i++) {
		master_buf[i] = i;
	}

	/* 1. master send, slave receive */
	i2c_write(i2c0_dev, master_buf, sizeof(master_buf) - 1, SLAVE_ADDRESS);

	memset(master_buf, 0, sizeof(master_buf));

	printk("master receive, slave send\n");
	/* 2. master receive, slave send */
	i2c_read(i2c0_dev, master_buf, sizeof(master_buf), SLAVE_ADDRESS);

	k_sem_give(&i2c_sem_m2s);

	for (int i = 0; i < sizeof(slave_buf); i++) {
		__ASSERT(master_buf[i] == slave_buf[i],
			 "master recv error, master_buf[%d]: 0x%02X != slave_buf[%d]: 0x%02X\n", i, master_buf[i],
			 i, slave_buf[i]);
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
	memset(slave_buf, 0, sizeof(slave_buf));
	i2c_read(i2c1_dev, slave_buf, sizeof(slave_buf), 0);
	k_sem_give(&i2c_sem_s2m);

	/* data compare */
	for (int i = 0; i < sizeof(slave_buf); i++) {
		__ASSERT(master_buf[i] == slave_buf[i],
			 "slave recv error, master_buf[%d]: 0x%02X != slave_buf[%d]: 0x%02X\n", i, master_buf[i],
			 i, slave_buf[i]);
	}

	printk("slave received ok!\n");

	/* reset data value */
	k_sem_take(&i2c_sem_m2s, K_FOREVER);
	for (int i = 0; i < sizeof(slave_buf); i++) {
		slave_buf[i] = i;
	}

	i2c_write(i2c1_dev, slave_buf, sizeof(slave_buf) - 1, 0);

	while (1) {
		k_msleep(10);
	}
}

void main(void)
{
	int ret = 0;
	printk("I2C START TEST\n");

	/* i2c master and slave initialize */
	ret = i2c_master_init();
	if(ret != 0) {
		return;
	}

	ret = i2c_slave_init();
	if(ret != 0) {
		return;
	}

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
