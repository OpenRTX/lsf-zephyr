/*
 * Copyright (c) 2017 BayLibre, SAS
 * Copyright (c) 2020 Nordic Semiconductor ASA
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

#define NODE_EP0 DT_NODELABEL(eeprom0)

#define TEST_DATA_SIZE	20
static const uint8_t eeprom_0_data[TEST_DATA_SIZE] = {
	0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24,
	0x25, 0x26, 0x27, 0x28, 0x29,
};

void test_virtual_eeprom(void)
{
	const char *label_0 = DT_LABEL(NODE_EP0);
	const struct device *eeprom_0 = device_get_binding(label_0);
	int addr_0 = DT_REG_ADDR(NODE_EP0);

	int ret;
	ret = eeprom_slave_program(eeprom_0, eeprom_0_data, TEST_DATA_SIZE);
	
	/* Attach each EEPROM to its owning bus as a slave device. */
	ret = i2c_slave_driver_register(eeprom_0);
	if (ret != 0) {
		printk("Failed to register EEPROM %s, ret=%d\n", label_0, ret);
	}
}

void main(void)
{
	test_virtual_eeprom();
	while (1)
	{
		k_msleep(2000);
	}
}
