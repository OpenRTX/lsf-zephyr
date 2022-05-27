/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <sys/reboot.h>


void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);
	int cnt = 30;
	while(cnt--){
		printk("Hello World!\n");
		k_sleep(K_MSEC(100));
	}

	sys_reboot(0);
	k_sleep(K_MSEC(100));
	printk("sys_reboot Hello World!\n");
}
