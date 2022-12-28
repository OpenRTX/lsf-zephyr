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

K_SEM_DEFINE(i2c_sem_m2s, 0, 1);
K_SEM_DEFINE(i2c_sem_s2m, 0, 1);

void i2c_master_thread(void *v1, void *v2, void *v3);
void i2c_slave_thread(void *v1, void *v2, void *v3);

int i2c_master_init(void)
{
	i2c0_dev = device_get_binding(I2C0_DEV_NAME);
	if (i2c0_dev == NULL)
	{
		printk("I2C0: Device is not found.\n");
		return -ENODEV;
	}

	if (!device_is_ready(i2c0_dev))
	{
		printk("I2C0: Device is not ready.\n");
		return -ENODEV;
	}

	/* i2c master configure */
	if (i2c_configure(i2c0_dev, i2c0_cfg))
	{
		printk("I2C0 config failed\n");
		return -EINVAL;
	}

	return 0;
}

int i2c_slave_init(void)
{
	i2c1_dev = device_get_binding(I2C1_DEV_NAME);
	if (i2c1_dev == NULL)
	{
		printk("I2C1: Device is not found.\n");
		return -ENODEV;
	}

	if (!device_is_ready(i2c1_dev))
	{
		printk("I2C1: Device is not ready.\n");
		return -ENODEV;
	}

	/* i2c slave configure */
	if (i2c_configure(i2c1_dev, i2c1_cfg))
	{
		printk("I2C1 config failed\n");
		return -EINVAL;
	}

	return 0;
}

void main(void)
{
	int ret = 0;
	printk("I2C START TEST\n");

	/* i2c master and slave initialize */
	ret = i2c_master_init();
	if (ret != 0)
	{
		return;
	}

	ret = i2c_slave_init();
	if (ret != 0)
	{
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

	printk("I2C Thread creaeted\n");

	while (1)
	{
		k_msleep(10);
	}
}

#define TEST_MASTER_WRITE_SLAVE_READ
#define TEST_MASTER_READ_SLAVE_WRITE
#define BUF_SIZE (64)//(I2C API)The maximum single transfer size is 255 bytes

uint8_t master_buf[BUF_SIZE] = {0};
uint8_t slave_buf[BUF_SIZE] = {0};

void i2c_master_thread(void *v1, void *v2, void *v3)
{
	int ret = 0;
	printk("I2C0:Master Write, I2C1:Slave Read\n");
#ifdef TEST_MASTER_WRITE_SLAVE_READ
	for (int i = 0; i < sizeof(master_buf); i++)
	{
		master_buf[i] = i + 5;
	}
	// 1.Taking a Semaphore, I2C_Slave_Thread notifies I2C_Master_Thread that it can write data
	k_sem_take(&i2c_sem_s2m, K_FOREVER);
	ret = i2c_write(i2c0_dev, master_buf, sizeof(master_buf), SLAVE_ADDRESS);
	if (ret != 0)
	{
		printk("[%d:%s]i2c_write ret:%d\n", __LINE__, __FUNCTION__, ret);
	}
	// 2.Go to the next test
	k_sem_take(&i2c_sem_m2s, K_FOREVER);
#endif
	/**************************************************************************/
#ifdef TEST_MASTER_READ_SLAVE_WRITE

	printk("[%d:%s]2.I2C1:Slave Write, I2C0:Master Read\n", __LINE__, __FUNCTION__);
	printk("Wait 500ms...\n");
	k_msleep(500);

	// 3.Giving a Semaphore, I2C_Master_Thread notifies I2C_Slave_Thread that data can be written
	k_sem_give(&i2c_sem_m2s);
	k_msleep(30); //Wait
	memset(master_buf, 0, sizeof(master_buf));
	ret = i2c_read(i2c0_dev, master_buf, sizeof(master_buf), SLAVE_ADDRESS);
	if (ret != 0)
	{
		printk("[%d:%s]i2c_read ret:%d\n", __LINE__, __FUNCTION__, ret);
	}

	printk("I2C0:Master Read data");
	for (int i = 0; i < sizeof(master_buf); i++)
	{
		if (i % 16 == 0)
			printk("\n[0x%06X] | ", i);
		printk("  %02X", master_buf[i]);
		__ASSERT(master_buf[i] == slave_buf[i],
			 "master recv error, master_buf[%d]: 0x%02X != slave_buf[%d]: 0x%02X\n", i, master_buf[i],
			 i, slave_buf[i]);
	}
	printk("\n");
#endif

	printk("I2C Master_Thread Done\n");
	
	printk("I2C TEST DONE\n");
	while (1)
	{
		k_msleep(10);
	}
}

void i2c_slave_thread(void *v1, void *v2, void *v3)
{
	int ret = 0;
	printk("[%d:%s]1.I2C0:Master Write, I2C1:Slave Read\n", __LINE__, __FUNCTION__);
#ifdef TEST_MASTER_WRITE_SLAVE_READ
	memset(slave_buf, 0, sizeof(slave_buf));
	// 1.Giving a Semaphore, I2C_Slave_Thread notifies I2C_Master_Thread that data can be written
	k_sem_give(&i2c_sem_s2m);
	ret = i2c_read(i2c1_dev, slave_buf, sizeof(slave_buf), SLAVE_ADDRESS);
	if (ret != 0)
	{
		printk("[%d:%s]i2c_read ret:%d\n", __LINE__, __FUNCTION__, ret);
	}

	/* data compare */
	printk("I2C1:Slave Read data:");
	for (int i = 0; i < sizeof(slave_buf); i++)
	{
		if (i % 16 == 0)
			printk("\n[0x%06X] | ", i);
		printk("  %02X", slave_buf[i]);
		__ASSERT(master_buf[i] == slave_buf[i],
			 "slave recv error, master_buf[%d]: 0x%02X != slave_buf[%d]: 0x%02X\n", i, master_buf[i],
			 i, slave_buf[i]);
	}
	printk("\n");
	k_sem_give(&i2c_sem_m2s);
	// 2.Go to the next test
#endif 
	/**************************************************************************/

#ifdef TEST_MASTER_READ_SLAVE_WRITE
	printk("[%d:%s]2.I2C1:Slave Write, I2C0:Master Read\n", __LINE__, __FUNCTION__);
	/* reset data value */
	memset(slave_buf, 0, sizeof(slave_buf));
	for (int i = 0; i < sizeof(slave_buf); i++)
	{
		slave_buf[i] = i;
	}

	// 3.Taking a Semaphore, I2C_Master_Thread notifies I2C_Slave_Thread that data can be written
	k_sem_take(&i2c_sem_m2s, K_FOREVER);
	ret = i2c_write(i2c1_dev, slave_buf, sizeof(slave_buf),  0);
	if (ret != 0)
	{
		printk("[%d:%s]i2c_write ret:%d\n", __LINE__, __FUNCTION__, ret);
	}
#endif 
	printk("I2C Salve_Thread Done\n");
	while (1)
	{
		k_msleep(10);
	}
}

