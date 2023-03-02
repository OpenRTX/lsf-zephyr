/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <zephyr/sys/util.h>

#include <zephyr/drivers/i2c.h>
#include <i2c_slave_vdev.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(vdev_demo, LOG_LEVEL_INF);

#define NODE_VIRTUAL_DEV DT_NODELABEL(i2cslavevdev0)
#define TEST_DATA_SIZE	256
static uint8_t i2_vdev_0_data[TEST_DATA_SIZE] = "0123456789abcdefghij";

const struct device *i2_vdev_0 = NULL;
void i2_slave_vdev_init(void)
{
	int ret;
	const char *label_0 = DT_LABEL(NODE_VIRTUAL_DEV);
    printk("I2C Slave Virtual device Note: %s \n", label_0);
	i2_vdev_0 = device_get_binding(label_0);
	int addr_0 = DT_REG_ADDR(NODE_VIRTUAL_DEV);
	
    if (i2_vdev_0 == NULL)
    {
        printk("I2C Slave Virtual device not found \n");
        return;
    }
	printk("Device: %s, I2C Addr: 0x%X", label_0, addr_0);
	int i=0;
	for(i=0; i<TEST_DATA_SIZE; i++) {
		i2_vdev_0_data[i] = i;
	}
	ret = slave_vdev_program(i2_vdev_0, i2_vdev_0_data, TEST_DATA_SIZE);

	/* Attach each i2_vdev to its owning bus as a slave device. */
	ret = i2c_slave_driver_register(i2_vdev_0);
}

int vdev_handler(const struct device *dev, const uint32_t event, const struct i2c_msg msg)
{
	// Slave Virtual Device USER Handler
	LOG_DBG("Event:0x%X", event);
	if (event & SLAVE_VDEV_EVENT_READ_DONE) {
		printk("I2C Transmit data length:%d\n", msg.len);

	} else if (event & SLAVE_VDEV_EVENT_WRITE_DONE) {
		printk("I2C Receive data length:%d\n", msg.len);
	}
	return 0;
}

void main(void)
{
	i2_slave_vdev_init();

	slave_vdev_event_handler_register(i2_vdev_0, &vdev_handler);
	start_slave_vdev_handler_task();
	while (1)
	{
		k_msleep(2000);
	}
}
