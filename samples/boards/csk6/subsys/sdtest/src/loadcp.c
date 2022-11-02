#include <device.h>
#include <zephyr.h>
#include <stdio.h>
#include <stdlib.h>


// #define LOG_LEVEL 4
#include <logging/log.h>
LOG_MODULE_REGISTER(loadcp);

#include <hal/reg/cmn_sysctrl_reg_venus.h>
#include <hal/venus_ap.h>

int load_cp(void)
{
#define SYSCTRL_RESET_CP    	(0xCAFE000B)
#define SYSCTRL_RELEASE_CP  	(0xCAFE100B)
#define SYSCTRL_RESET_NPU   	(0xCAFE000C)
#define CP_PSRAM_BASE           (0x60000000)
#define AP_PSRAM_BASE           (0x30000000)
#define CP_FLASH_BASE         (0x68400000)  
    SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;

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
	memcpy((void *)AP_PSRAM_BASE, (void *)0x18400000, 290000);
	dcache_clean_range(AP_PSRAM_BASE, AP_PSRAM_BASE + 290000);

	__DSB();

	// run cp
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

	return 0;
}