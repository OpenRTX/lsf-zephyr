/*
 * Copyright (c) 2021 LisentAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * THIS FILE WAS AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * Functions here are designed to produce efficient code to
 * search an Xtensa bitmask of interrupts, inspecting only those bits
 * declared to be associated with a given interrupt level.  Each
 * dispatcher will handle exactly one flagged interrupt, in numerical
 * order (low bits first) and will return a mask of that bit that can
 * then be cleared by the calling code.  Unrecognized bits for the
 * level will invoke an error handler.
 */

#include <xtensa/config/core-isa.h>
#include <sys/util.h>
#include <sw_isr_table.h>

#if !defined(XCHAL_INT0_LEVEL) || XCHAL_INT0_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT1_LEVEL) || XCHAL_INT1_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT2_LEVEL) || XCHAL_INT2_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT3_LEVEL) || XCHAL_INT3_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT4_LEVEL) || XCHAL_INT4_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT5_LEVEL) || XCHAL_INT5_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT6_LEVEL) || XCHAL_INT6_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT7_LEVEL) || XCHAL_INT7_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT8_LEVEL) || XCHAL_INT8_LEVEL != 2
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT9_LEVEL) || XCHAL_INT9_LEVEL != 3
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT10_LEVEL) || XCHAL_INT10_LEVEL != 4
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT11_LEVEL) || XCHAL_INT11_LEVEL != 4
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT12_LEVEL) || XCHAL_INT12_LEVEL != 4
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT13_LEVEL) || XCHAL_INT13_LEVEL != 5
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT14_LEVEL) || XCHAL_INT14_LEVEL != 7
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT15_LEVEL) || XCHAL_INT15_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT16_LEVEL) || XCHAL_INT16_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT17_LEVEL) || XCHAL_INT17_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT18_LEVEL) || XCHAL_INT18_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT19_LEVEL) || XCHAL_INT19_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif
#if !defined(XCHAL_INT20_LEVEL) || XCHAL_INT20_LEVEL != 1
#error core-isa.h interrupt level does not match dispatcher!
#endif


__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int_x(unsigned int mask, int *irqs, uint32_t irq_size){
	int irq;
	for (size_t i = 0; i < irq_size; i++)
	{
		if (mask & 1 << irqs[i]) {
			mask = 1 << irqs[i];
			irq = irqs[i];
			goto handle_irq;
		}
	}
	return 0;
handle_irq:
	_sw_isr_table[irq].isr(_sw_isr_table[irq].arg);
	return mask;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int1(unsigned int mask)
{
    int irq;
	mask &= XCHAL_INTLEVEL1_MASK;
    		
    if (mask & 1 << 0) {
        mask = 1 << 0;
        irq = 0;
    } else if (mask & 1 << 1) {
        mask = 1 << 1;
        irq = 1;
    } else if (mask & 1 << 2) {
        mask = 1 << 2;
        irq = 2;
    } else if (mask & 1 << 3) {
        mask = 1 << 3;
        irq = 3;
    } else if (mask & 1 << 4) {
        mask = 1 << 4;
        irq = 4;
    } else if (mask & 1 << 5) {
        mask = 1 << 5;
        irq = 5;
    } else if (mask & 1 << 6) {
        mask = 1 << 6;
        irq = 6;
    } else if (mask & 1 << 7) {
        mask = 1 << 7;
        irq = 7;
    } else {
    	return 0;
    }

	_sw_isr_table[irq].isr(_sw_isr_table[irq].arg);
	return mask;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int2(unsigned int mask)
{
	return 0;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int3(unsigned int mask)
{
	return 0;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int5(unsigned int mask)
{
    int irq;
	mask &= XCHAL_INTLEVEL1_MASK;
    		
    if (mask & 1 << 13) {
        mask = 1 << 13;
        irq = 13;
        goto handle_irq;
    }
	return 0;
handle_irq:
	_sw_isr_table[irq].isr(_sw_isr_table[irq].arg);
	return mask;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int0(unsigned int mask)
{
	return 0;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int4(unsigned int mask)
{
    int irq;
	mask &= XCHAL_INTLEVEL1_MASK;
    		
    if (mask & 1 << 10) {
        mask = 1 << 10;
        irq = 10;
        goto handle_irq;
    } else if (mask & 1 << 11) {
        mask = 1 << 11;
        irq = 11;
        goto handle_irq;
    }
	return 0;
handle_irq:
	_sw_isr_table[irq].isr(_sw_isr_table[irq].arg);
	return mask;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int6(unsigned int mask)
{
	return 0;
}

__attribute__ ((section(".iram.text"))) static inline int _xtensa_handle_one_int7(unsigned int mask)
{
    int irq;
	mask &= XCHAL_INTLEVEL1_MASK;
    		
    if (mask & 1 << 14) {
        mask = 1 << 14;
        irq = 14;
        goto handle_irq;
    }
	return 0;
handle_irq:
	_sw_isr_table[irq].isr(_sw_isr_table[irq].arg);
	return mask;
}

