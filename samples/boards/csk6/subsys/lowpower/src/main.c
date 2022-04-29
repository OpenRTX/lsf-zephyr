/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

#include <cache.h>
#include "venus_ap.h"
#include "cmn_sysctrl_reg_venus.h"
#include "PSRAMManager.h"
#include <string.h>
#include <logging/log.h>
#include <shell/shell.h>


#include <power_control.h>

//#define URPC_SERVER_SIM

#define GET_UINT32(addr)          *((uint32_t *)(addr))
#define GET_UINT16(addr)          *((uint16_t *)(addr))


#define SYSCTRL_RESET_CP    	(0xCAFE000B)
#define SYSCTRL_RELEASE_CP  	(0xCAFE100B)
#define SYSCTRL_RESET_NPU   	(0xCAFE000C)
#define CP_PSRAM_BASE           (0x60000000)
#define AP_PSRAM_BASE           (0x30000000)

#define CP_FW_ADDR (0x18000000 + 0x00200000)
#define CP_FW_LEN 3735552



static void load_cp_bin(void)
{
	__DMB();
	// copy cp code to psram
	// extern unsigned char cp_img_bin[];
	// extern unsigned int cp_img_bin_len;
	memcpy((void *)AP_PSRAM_BASE, (void *)CP_FW_ADDR, CP_FW_LEN);

	sys_cache_data_range((void *)AP_PSRAM_BASE, CP_FW_LEN, K_CACHE_WB);

	__DSB();
	
}


static void run_cp_npu(void)
{
	// enable CP & NPU clock
	IP_SYSCTRL->REG_CORE_CLK_CFG.bit.ENA_CP_CORE_CLK = 0x1;
	IP_SYSCTRL->REG_CORE_CLK_CFG.bit.ENA_NPU_CLK = 0x1;

	// release CP & NPU
	IP_SYSCTRL->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;

	if (IP_SYSCTRL->REG_SW_RESET_CP.all) {
		printk("error,cp reset should not be 0, in this momment\n");
	} else {
		// reset cp
		IP_SYSCTRL->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;

		// release CP
		IP_SYSCTRL->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	}

	// release NPU
	if (IP_SYSCTRL->REG_SW_RESET_NPU.all) { // = 1 NPU reset
		IP_SYSCTRL->REG_SW_RESET_NPU.all = SYSCTRL_RESET_NPU;
	}

	// run HIFI4
	IP_SYSCTRL->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;
}


void main(void)
{
    // set cp boot address
    IP_SYSCTRL->REG_CP_CTRL2.all = CP_PSRAM_BASE;
    IP_SYSCTRL->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 0x1;

    // stall HIFI4
    IP_SYSCTRL->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;

	load_cp_bin();

	run_cp_npu();

	uint32_t cnt = 1;
	while (1) {
		k_msleep(3000);
		if(cnt++%2)
		{
			// enter_lowpower_mode();
		}
		else
		{
			// exit_lowpower_mode();
		}
	}
}

static int psram_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	PSRAM_Initialize();
	return 0;
}

SYS_INIT(psram_init, PRE_KERNEL_2, 0);





// shell for test

static int cmd_lowpower_enter(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t freq = atoi(argv[1]);
	uint8_t psram_flag = atoi(argv[2]);
	if(enter_lowpower_mode(freq, psram_flag)){
		return 1;
	}
	printk("cmd_lowpower_enter\n");
	return 0;
}

static int cmd_lowpower_exit(const struct shell *shell, size_t argc, char **argv)
{
	if(exit_lowpower_mode()){
		return 1;
	}
	printk("cmd_lowpower_exit\n");
	return 0;
}

#define LOWPOWER_IN_HELP                                                                           \
	("lowpower in <freq> <psram_ctl>\n       freq - 10/50/100/200\npsram_ctl - 0:not control psram, 1:control psram\n")
#define LOWPOWER_OUT_HELP ("lowpower out:enter normal status\n")

SHELL_STATIC_SUBCMD_SET_CREATE(sub_lowpower,
			       /* Alphabetically sorted to ensure correct Tab autocompletion. */
			       /*SHELL_CMD_ARG(syntax, subcmd, help, handler, mand, opt)*/
			       SHELL_CMD_ARG(in, NULL, LOWPOWER_IN_HELP, cmd_lowpower_enter, 3, 0),
			       SHELL_CMD_ARG(out, NULL, LOWPOWER_OUT_HELP, cmd_lowpower_exit, 0, 0),

			       SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(lowpower, &sub_lowpower, "lowpower related commands.", NULL);