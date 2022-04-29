/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include "venus_ap.h"
#include "cmn_sysctrl_reg_venus.h"
#include "PSRAMManager.h"
#include <string.h>

//#define URPC_SERVER_SIM

#define GET_UINT32(addr)          *((uint32_t *)(addr))
#define GET_UINT16(addr)          *((uint16_t *)(addr))


SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef* )CMN_SYSCTRL_BASE;

void main(void)
{

#define SYSCTRL_RESET_CP    	(0xCAFE000B)
#define SYSCTRL_RELEASE_CP  	(0xCAFE100B)
#define SYSCTRL_RESET_NPU   	(0xCAFE000C)
#define CP_PSRAM_BASE           (0x60000000)
#define AP_PSRAM_BASE           (0x30000000)

    // set boot address
    cmn_sys_ctrl->REG_CP_CTRL2.all = CP_PSRAM_BASE;
    cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 0x1;

    // stall HIFI4
    cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;

	// release CP & NPU
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	if(cmn_sys_ctrl->REG_SW_RESET_NPU.all){ // = 1 NPU reset
		cmn_sys_ctrl->REG_SW_RESET_NPU.all = SYSCTRL_RESET_NPU;
	}

	__DMB();
	// copy cp code to psram
	extern unsigned char cp_img_bin[];
	extern unsigned int cp_img_bin_len;
	memcpy((void *)AP_PSRAM_BASE, cp_img_bin, cp_img_bin_len);
	dcache_clean_range(AP_PSRAM_BASE, AP_PSRAM_BASE + cp_img_bin_len);

	__DSB();

	// run cp
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

	while (1) {
		uint8_t cnt =0;
		k_msleep(1000);
		//printk("cnt: %d \r\n", cnt);
	}
}

static int psram_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	PSRAM_Initialize();
	return 0;
}

SYS_INIT(psram_init, PRE_KERNEL_2, 0);
