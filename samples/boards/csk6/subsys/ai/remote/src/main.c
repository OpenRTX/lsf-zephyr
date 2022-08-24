/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include <logging/log.h>
#include <w_pilot_err.h>
#include <pilot_app.h>
#include "venus_cp.h"
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
	// //test
	CSK_test_function(ifr_inst);
	// //unint
	ret_error = UINIT_ENGINE(ifr_inst);
	return 0;
}

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

// static void
// csk_dma_irq_handler(const struct device *dev)
// {
//     dma_irq_handler();
// }


void main(void)
{
	// int ret;


	GPIO_SetDir(GPIOA(), (1 << 5), 1);
	GPIO_SetDir(GPIOA(), (1 << 4), 1);
	GPIO_PinWrite(GPIOA(), (1 << 5), 0);
	GPIO_PinWrite(GPIOA(), (1 << 4), 0);
	dma_init();
	sys_init();
	LOG_INF("SOF on22222 %s", CONFIG_BOARD);
	start_app();
	/* sof_main is actually SOF initialization */
	// ret = sof_main(0, NULL);
	// if (ret) {
	// 	LOG_ERR("SOF initialization failed");
	// }

	LOG_INF("SOF initialized");
}
