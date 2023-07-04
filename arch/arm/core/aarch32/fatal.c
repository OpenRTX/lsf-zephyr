/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Kernel fatal error handler for ARM Cortex-M and Cortex-R
 *
 * This module provides the z_arm_fatal_error() routine for ARM Cortex-M
 * and Cortex-R CPUs.
 */

#include <zephyr/kernel.h>
#include <kernel_arch_data.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(os, CONFIG_KERNEL_LOG_LEVEL);

#if defined(CONFIG_EXTRA_EXCEPTION_INFO)
#ifndef _CALL_STACK_MAX_DEPTH
#define _CALL_STACK_MAX_DEPTH       16
#endif

/* check the disassembly instruction is 'BL' or 'BLX' */
static bool disassembly_ins_is_bl_blx(uint32_t addr) {
    uint16_t ins1 = *((uint16_t *)addr);
    uint16_t ins2 = *((uint16_t *)(addr + 2));

#define BL_INS_MASK         0xF800
#define BL_INS_HIGH         0xF800
#define BL_INS_LOW          0xF000
#define BLX_INX_MASK        0xFF00
#define BLX_INX             0x4700

    if ((ins2 & BL_INS_MASK) == BL_INS_HIGH && (ins1 & BL_INS_MASK) == BL_INS_LOW) {
        return true;
    } else if ((ins2 & BLX_INX_MASK) == BLX_INX) {
        return true;
    } else {
        return false;
    }
}

static size_t _backtrace_call_stack(uint32_t *buffer, size_t size, const z_arch_esf_t *esf)
{
	uint32_t stack_start_addr, stack_size, pc;
    uint32_t depth = 0;
    bool regs_saved_lr_is_valid = false;
	uint32_t sp = (uint32_t)esf->extra_info.callee->psp;

	uint32_t code_start_addr = 0x18000000;
	uint32_t code_size = 1024 * 1024;


	/* first depth is PC */
	buffer[depth++] = esf->basic.pc;
	/* fix the LR address in thumb mode */
	pc = esf->basic.lr - 1;
	/* fix the LR address to pc address */
	pc -= 1;
	if ((pc >= code_start_addr) && (pc <= code_start_addr + code_size) && (depth < _CALL_STACK_MAX_DEPTH)
			&& (depth < size)) {
		buffer[depth++] = pc;
		regs_saved_lr_is_valid = true;
	}


	//gets the current thread stack space
	k_tid_t thread = k_current_get();
	stack_start_addr = thread->stack_info.start;
	stack_size = thread->stack_info.size;

	/*
		when stack overflow? --Due to the stack limit of the m33 processor, this effectively 
		prevents stack pushed operations that could compromise the integrity of the stack.
	*/

    /* copy called function address */
    for (; sp < stack_start_addr + stack_size; sp += sizeof(size_t)) {
        /* the *sp value may be LR, so need decrease a word to PC */
        pc = *((uint32_t *) sp) - sizeof(size_t);
        /* the Cortex-M using thumb instruction, so the pc must be an odd number */
        if (pc % 2 == 0) {
            continue;
        }
        /* fix the PC address in thumb mode */
        pc = *((uint32_t *) sp) - 1;

        if ((pc >= code_start_addr + sizeof(size_t)) && (pc <= code_start_addr + code_size) && (depth < _CALL_STACK_MAX_DEPTH)
                /* check the the instruction before PC address is 'BL' or 'BLX' */
                && disassembly_ins_is_bl_blx(pc - sizeof(size_t)) && (depth < size)) {
			/* fix the LR address to pc address */
			pc -= 1;

            /* the second depth function may be already saved, so need ignore repeat */
            if ((depth == 2) && regs_saved_lr_is_valid && (pc == buffer[1])) {
                continue;
            }
            buffer[depth++] = pc;
        }
    }

	return depth;
}

#include <stdio.h>
static char call_stack_info[_CALL_STACK_MAX_DEPTH * (8 + 1)] = { 0 };
static void _print_call_stack(const z_arch_esf_t *esf)
{
	size_t i, cur_depth = 0;
	uint32_t call_stack_buf[_CALL_STACK_MAX_DEPTH] = {0};
	cur_depth = _backtrace_call_stack(call_stack_buf, _CALL_STACK_MAX_DEPTH, esf);

    for (i = 0; i < cur_depth; i++) {
        sprintf(call_stack_info + i * (8 + 1), "%08lx", (unsigned long)call_stack_buf[i]);
        call_stack_info[i * (8 + 1) + 8] = ' ';
    }

	if (cur_depth) {
        call_stack_info[cur_depth * (8 + 1) - 1] = '\0';
        LOG_ERR("Show more call stack info by run: addr2line -e build/zephyr/zephyr.elf -a -f -p %s", call_stack_info);
    } else {
        LOG_ERR("Dump call stack has an error");
    }
}
#endif	//CONFIG_EXTRA_EXCEPTION_INFO

static void esf_dump(const z_arch_esf_t *esf)
{
	LOG_ERR("r0/a1:  0x%08x  r1/a2:  0x%08x  r2/a3:  0x%08x",
		esf->basic.a1, esf->basic.a2, esf->basic.a3);
	LOG_ERR("r3/a4:  0x%08x r12/ip:  0x%08x r14/lr:  0x%08x",
		esf->basic.a4, esf->basic.ip, esf->basic.lr);
	LOG_ERR(" xpsr:  0x%08x", esf->basic.xpsr);
#if defined(CONFIG_FPU) && defined(CONFIG_FPU_SHARING)
	for (int i = 0; i < ARRAY_SIZE(esf->fpu.s); i += 4) {
		LOG_ERR("s[%2d]:  0x%08x  s[%2d]:  0x%08x"
			"  s[%2d]:  0x%08x  s[%2d]:  0x%08x",
			i, (uint32_t)esf->fpu.s[i],
			i + 1, (uint32_t)esf->fpu.s[i + 1],
			i + 2, (uint32_t)esf->fpu.s[i + 2],
			i + 3, (uint32_t)esf->fpu.s[i + 3]);
	}
	LOG_ERR("fpscr:  0x%08x", esf->fpu.fpscr);
#endif
#if defined(CONFIG_EXTRA_EXCEPTION_INFO)
	const struct _callee_saved *callee = esf->extra_info.callee;

	if (callee != NULL) {
		LOG_ERR("r4/v1:  0x%08x  r5/v2:  0x%08x  r6/v3:  0x%08x",
			callee->v1, callee->v2, callee->v3);
		LOG_ERR("r7/v4:  0x%08x  r8/v5:  0x%08x  r9/v6:  0x%08x",
			callee->v4, callee->v5, callee->v6);
		LOG_ERR("r10/v7: 0x%08x  r11/v8: 0x%08x    psp:  0x%08x",
			callee->v7, callee->v8, callee->psp);
	}

	LOG_ERR("EXC_RETURN: 0x%0x", esf->extra_info.exc_return);

#endif /* CONFIG_EXTRA_EXCEPTION_INFO */
	LOG_ERR("Faulting instruction address (r15/pc): 0x%08x",
		esf->basic.pc);
}

void z_arm_fatal_error(unsigned int reason, const z_arch_esf_t *esf)
{

	if (esf != NULL) {
		esf_dump(esf);
		#if defined(CONFIG_EXTRA_EXCEPTION_INFO)
		_print_call_stack(esf);
		#endif
	}
	z_fatal_error(reason, esf);
}

/**
 * @brief Handle a software-generated fatal exception
 * (e.g. kernel oops, panic, etc.).
 *
 * Notes:
 * - the function is invoked in SVC Handler
 * - if triggered from nPRIV mode, only oops and stack fail error reasons
 *   may be propagated to the fault handling process.
 * - We expect the supplied exception stack frame to always be a valid
 *   frame. That is because, if the ESF cannot be stacked during an SVC,
 *   a processor fault (e.g. stacking error) will be generated, and the
 *   fault handler will executed instead of the SVC.
 *
 * @param esf exception frame
 * @param callee_regs Callee-saved registers (R4-R11)
 */
void z_do_kernel_oops(const z_arch_esf_t *esf, _callee_saved_t *callee_regs)
{
#if !(defined(CONFIG_EXTRA_EXCEPTION_INFO) && defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE))
	ARG_UNUSED(callee_regs);
#endif
	/* Stacked R0 holds the exception reason. */
	unsigned int reason = esf->basic.r0;

#if defined(CONFIG_USERSPACE)
	if (z_arm_preempted_thread_in_user_mode(esf)) {
		/*
		 * Exception triggered from user mode.
		 *
		 * User mode is only allowed to induce oopses and stack check
		 * failures via software-triggered system fatal exceptions.
		 */
		if (!((esf->basic.r0 == K_ERR_KERNEL_OOPS) ||
			(esf->basic.r0 == K_ERR_STACK_CHK_FAIL))) {

			reason = K_ERR_KERNEL_OOPS;
		}
	}

#endif /* CONFIG_USERSPACE */

#if !defined(CONFIG_EXTRA_EXCEPTION_INFO)
	z_arm_fatal_error(reason, esf);
#else
	z_arch_esf_t esf_copy;

	memcpy(&esf_copy, esf, offsetof(z_arch_esf_t, extra_info));
#if defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
	/* extra exception info is collected in callee_reg param
	 * on CONFIG_ARMV7_M_ARMV8_M_MAINLINE
	 */

	esf_copy.extra_info = (struct __extra_esf_info) {
		.callee = callee_regs,
	};
#else
	/* extra exception info is not collected for kernel oops
	 * path today so we make a copy of the ESF and zero out
	 * that information
	 */
	esf_copy.extra_info = (struct __extra_esf_info) { 0 };
#endif /* CONFIG_ARMV7_M_ARMV8_M_MAINLINE */

	z_arm_fatal_error(reason, &esf_copy);
#endif /* CONFIG_EXTRA_EXCEPTION_INFO */
}

FUNC_NORETURN void arch_syscall_oops(void *ssf_ptr)
{
	uint32_t *ssf_contents = ssf_ptr;
	z_arch_esf_t oops_esf = { 0 };

	/* TODO: Copy the rest of the register set out of ssf_ptr */
	oops_esf.basic.pc = ssf_contents[3];

	z_arm_fatal_error(K_ERR_KERNEL_OOPS, &oops_esf);
	CODE_UNREACHABLE;
}
