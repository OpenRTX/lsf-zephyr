/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/zephyr.h>

void function_in_sram(int32_t value)
{
	printk("Address of function_in_sram %p\n", &function_in_sram);
	printk("Hello World! %s\n", CONFIG_BOARD);
}
