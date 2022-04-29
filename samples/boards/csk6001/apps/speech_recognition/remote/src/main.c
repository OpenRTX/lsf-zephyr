/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include "hal/ClockManager.h"
#include "luna/luna.h"

#include "task_algo.h"


#define SLAVE_STACK_SIZE 81920
K_THREAD_STACK_DEFINE(algo_stack_area, SLAVE_STACK_SIZE);
struct k_thread algo_thread_data;

extern void algo_thread(void *v1, void *v2, void *v3);

static uint8_t  __attribute__ ((section (".dram1.bss"), aligned(512))) src1[512] = {0};
static uint8_t  __attribute__ ((section (".dram1.bss"), aligned(512))) src22[512] = {0};
static uint8_t  __attribute__ ((section (".dram1.bss"), aligned(512))) dst[512] = {0};

static void luna_test(void)
{
	int32_t ret = 0;
	printk("src1:%p, src22:%p, dst:%p \n", src1, src22, dst);

	for(int i = 0; i < 512; i++){
		src1[i] = 3;
		src22[i] = 4;
		dst[i] = 0;
	}

	ret = luna_add_q7_int8(src1, src22, dst, 512, 0);
	printk("ret = %d \n", ret);

	for(int i = 0; i < 512; i++){
		printk("0x%x ", dst[i]);

		if((i+1) % 16 == 0){
			printk("\n");
		}
	}
	printk("\n");
}

int luna_printf(const char * fmt, ...)
{
	static char buf[1024] = { 0 };
	va_list ap;

	va_start(ap, fmt);
	uint32_t n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if(n > 0) printk("[luna]: %s\n", buf);

	return n;
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
	printk("luna_val: %d \n", luna_val);
    printk("[Remote][%s %d] Done...\n",__FUNCTION__,__LINE__);
}

void main(void)
{
	printk("[Remote][%s %d] Enter...\n",__FUNCTION__,__LINE__);
	sys_init();

	luna_test();

	/* create algothrim thread */
	int pri = k_thread_priority_get(k_current_get());

	k_thread_create(&algo_thread_data, algo_stack_area,
					K_THREAD_STACK_SIZEOF(algo_stack_area),
					algo_thread, NULL, NULL, NULL, pri, 0, K_NO_WAIT);

}
