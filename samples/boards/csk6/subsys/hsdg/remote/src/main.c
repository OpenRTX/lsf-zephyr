/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include <logging/log.h>
#include <w_pilot_err.h>
#include <pilot_app.h>
#include <venus_cp.h>
#include <luna_bits.h>
#include <ClockManager.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


//CSK star app
int start_app(void)
{
	int ret_error = IPILOT_SUCCESS;
	IPLT_INST ifr_inst = NULL;
	//init
	ret_error = INIT_ENGINE(&ifr_inst);
	//set params
	SET_PARMA_ENGINE(ifr_inst);
	//test
	CSK_test_function(ifr_inst);
	//unint
	ret_error = UINIT_ENGINE(ifr_inst);
	return 0;
}

extern void luna_init();
void sys_init(void)
{
    // enable NPU Clock
	__HAL_CRM_NPU_CLK_ENABLE();

    // release NPU
    if(IP_SYSCTRL->REG_SW_RESET_NPU.all){
        IP_SYSCTRL->REG_SW_RESET_NPU.all = 0xCAFE000C;
    }
    luna_init();

	uint32_t luna_val = *((volatile uint32_t *)(ADDR_LUNA_MODULE_ST));

	LOG_INF("luna_val: %d", luna_val);
}


extern void dma_init();
void main(void)
{

	LOG_INF("pilot app start");

	dma_init();
	sys_init();
	start_app();

}
