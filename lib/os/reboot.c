/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/reboot.h>
#include <kernel.h>
#include <sys/printk.h>

#if CONFIG_SOC_CSK6
#include "drivers/gpio.h"
#define GPIO_PORT DT_LABEL(DT_NODELABEL(gpiob))
#define GPIO_BOOT_PIN0    0
#define GPIO_BOOT_PIN1    1
#endif

extern void sys_arch_reboot(int type);
extern void sys_clock_disable(void);

FUNC_NORETURN void sys_reboot(int type)
{
	(void)irq_lock();
#ifdef CONFIG_SYS_CLOCK_EXISTS
	sys_clock_disable();
#endif

#if CONFIG_SOC_CSK6
#if (CONFIG_FLASH_WRITE_BLOCK_SIZE == 4)
	extern int csk_flash_clear_add4(void);
	csk_flash_clear_add4();
#endif
	const struct device *dev = device_get_binding(GPIO_PORT);
	gpio_pin_configure(dev, GPIO_BOOT_PIN0, GPIO_PULL_UP);
	gpio_pin_configure(dev, GPIO_BOOT_PIN1, GPIO_PULL_UP);
#endif
	sys_arch_reboot(type);

	/* should never get here */
	printk("Failed to reboot: spinning endlessly...\n");
	for (;;) {
		k_cpu_idle();
	}
}
