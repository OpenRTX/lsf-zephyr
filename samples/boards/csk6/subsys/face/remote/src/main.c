/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include <logging/log.h>
#include <venus_cp.h>
#include <luna_bits.h>
#include <ClockManager.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


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
#define BUFFER_SIZE (1024 * 100)
// uint8_t dst_buffer[BUFFER_SIZE] __attribute__((aligned(4)));
// uint8_t src_buffer[BUFFER_SIZE] __attribute__((aligned(4)));
void main(void)
{

	LOG_INF("pilot app start");
	dma_init();
	sys_init();
	// printk("\nDMA copy start\n");

	// uint32_t start = k_cycle_get_32();
	// dma_cpy_async(0, src_buffer, dst_buffer, BUFFER_SIZE);
	// dma_wait_complete(0);
	// uint32_t time = (double)(k_cycle_get_32() - start) / sys_clock_hw_cycles_per_sec() * 1000 * 1000;
	// printk("DMA copy cost time: %d us\n", time);
	// printk("DMA copy finish\n");
	main_app(0, 0);
}
