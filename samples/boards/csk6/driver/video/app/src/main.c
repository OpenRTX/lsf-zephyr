/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <drivers/video.h>
#include <device.h>

#include "venus_ap.h"
#include "cmn_sysctrl_reg_venus.h"
#include "PSRAMManager.h"

#include <logging/log.h>
#include <drivers/uart.h>
#include <drivers/ipm.h>

LOG_MODULE_REGISTER(main, 4);

#define CP_BOOT_ADDRESS 0x60000000
#define SYSCTRL_RESET_CP (0xCAFE000B)
#define SYSCTRL_RELEASE_CP (0xCAFE100B)
#define SYSCTRL_RESET_NPU (0xCAFE000C)
#define CP_PSRAM_BASE (0x68400000)
#define AP_PSRAM_BASE (0x30000000)

void prepare_cp_run(void)
{
	PSRAM_Initialize();

	SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;

	// set boot address
	cmn_sys_ctrl->REG_CP_CTRL2.all = CP_BOOT_ADDRESS;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 0x1;

	// stall HIFI4
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;

	// release CP & NPU
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	if (cmn_sys_ctrl->REG_SW_RESET_NPU.all) { // = 1 NPU reset
		cmn_sys_ctrl->REG_SW_RESET_NPU.all = SYSCTRL_RESET_NPU;
	}

	__DMB();
	__DSB();

	// run cp
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

	printk("Set CP boot address:%x.\n", CP_BOOT_ADDRESS);
	printk("Boot CP...\n");
}

#define VIDEO_DEV DT_LABEL(DT_INST(0, byd_bf30a2))

struct k_work_delayable dwork;

static void ipm_callback(const struct device *dev, void *context, uint32_t id, volatile void *data)
{
	printk("id=%d.\n", id);
	k_work_schedule(&dwork, K_MSEC(10));
}

static void _work(struct k_work *work)
{
	const struct device *video = device_get_binding(VIDEO_DEV);

	if (!device_is_ready(video)) {
		printk("ipm device is null.\n");
		return;
	}

	/* Start video capture */
	if (video_stream_start(video)) {
		printk("Unable to start video.\n");
		return;
	}

	printk("start video steam.\n");
}

void main(void)
{
	const struct device *ipm = DEVICE_DT_GET(DT_NODELABEL(mailbox0));

	printk("Hello World! %s\n", CONFIG_BOARD);
	prepare_cp_run();

	if (!device_is_ready(ipm)) {
		printk("ipm device is null.\n");
		return;
	}

	/* Recv cp msg by IPM */
	ipm_register_callback(ipm, ipm_callback, NULL);
	ipm_set_enabled(ipm, 1);

	/* define a delay work to start open video steam. */
	k_work_init_delayable(&dwork, _work);

	while (1) {
		k_yield();
	}
}
