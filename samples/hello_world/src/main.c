/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>

extern void boot_cp(void);
void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);
	boot_cp();
	printk("after boot cp !\n");

}
