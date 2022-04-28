/*
 * Copyright (c) 2021 LisentAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <xtensa/xtruntime.h>
#include <irq_nextlevel.h>
#include <xtensa/hal.h>
#include <init.h>

#include "soc.h"

#ifdef CONFIG_DYNAMIC_INTERRUPTS
#include <sw_isr_table.h>
#endif
#define LOG_LEVEL CONFIG_SOC_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(soc);

void z_soc_irq_enable(uint32_t irq)
{
	/*
	 * enable core interrupt
	 */
	z_xtensa_irq_enable(XTENSA_IRQ_NUMBER(irq));
}

void z_soc_irq_disable(uint32_t irq)
{
	/*
	 * disable the interrupt in interrupt controller
	 */
	z_xtensa_irq_disable(XTENSA_IRQ_NUMBER(irq));
}

int z_soc_irq_is_enabled(unsigned int irq)
{
	int ret = 0;

	/* regular interrupt */
	ret = z_xtensa_irq_is_enabled(XTENSA_IRQ_NUMBER(irq));

	return ret;
}
void __memmap_init(void)
{
	typedef struct {
		uint32_t const *src;
		uint32_t *dest;
		uint32_t wlen;
	} __copy_table_t;

	extern const __copy_table_t __copy_table_start__;
	extern const __copy_table_t __copy_table_end__;

	for (__copy_table_t const *pTable = &__copy_table_start__; pTable < &__copy_table_end__;
	     ++pTable) {
		for (uint32_t i = 0u; i < pTable->wlen/4; ++i) {
			pTable->dest[i] = pTable->src[i];
		}
	}

}

void xtensa_real_exit(int status){
	#include <xtensa/simcall.h>
	register int a2 __asm__ ("a2") = SYS_exit;
	register int a3 __asm__ ("a3") = status;
	__asm__ volatile ("simcall"
			  : : "a" (a2), "a" (a3));
};