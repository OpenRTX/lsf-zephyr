/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>

#include "venus_ap.h"

#define PSRAM_CP_DOMAIN (0x60000000)
#define PSRAM_AP_DOMAIN (0x30000000)

static void dual_core_init(bool loadcp)
{
	// set boot address
	IP_SYSCTRL->REG_CP_CTRL2.bit.CP_BOOT_ADDR = PSRAM_CP_DOMAIN;
	IP_SYSCTRL->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 1;

	// stall HIFI4
	IP_SYSCTRL->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 1;

	// release CP & NPU
	IP_SYSCTRL->REG_SW_RESET_CP.all = 0xCAFE100B;
	if (IP_SYSCTRL->REG_SW_RESET_NPU.all)
		IP_SYSCTRL->REG_SW_RESET_NPU.all = 0xCAFE000C;
	__DMB();

	// copy cp code to psram
	uint32_t addr = 0x18101000, size = 1600000;
	printk("load cp(size=%#x) from flash(%#x) to psram(%#x) ...\n", size, addr,
	       PSRAM_AP_DOMAIN);
	memcpy((void *)PSRAM_AP_DOMAIN, (const void *)addr, size);
	SCB_CleanDCache_by_Addr(PSRAM_AP_DOMAIN, size);
	__DSB();

	// run cp
	IP_SYSCTRL->REG_SW_RESET_CP.all = 0xCAFE000B;
	IP_SYSCTRL->REG_SW_RESET_CP.all = 0xCAFE100B;
	IP_SYSCTRL->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

/*------------------------------private define------------------------------------*/
#define MEM_BANK_CNT		4
#define SHAREMEMORY_BANK0_REMAP 1
#define AP_SYS_RAM_BANK_SIZE	0x00010000
#define AP_SYS_RAM_BANK_ADDR	(0x20080000 + AP_SYS_RAM_BANK_SIZE * MEM_BANK_CNT)
	IP_SYSCTRL->REG_AP_CTRL1.bit.AP_RAM_SEL = (SHAREMEMORY_BANK0_REMAP << MEM_BANK_CNT);
	memset((void *)AP_SYS_RAM_BANK_ADDR, 0x00, AP_SYS_RAM_BANK_SIZE);
	SCB_CleanDCache_by_Addr(AP_SYS_RAM_BANK_ADDR, AP_SYS_RAM_BANK_SIZE);

	IP_SYSCTRL->REG_MISC_CLK_CFG2.bit.ENA_USBC_CLK = 0x01;
}

void main(void)
{
	dual_core_init(1);
	printk("Hello World! %s\n", CONFIG_BOARD);
}
