/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <cache.h>
#include <drivers/video.h>
#include <device.h>

#include "venus_ap.h"
#include "cmn_sysctrl_reg_venus.h"
#include "PSRAMManager.h"

#include <logging/log.h>
#include <drivers/uart.h>
#include <drivers/ipm.h>

LOG_MODULE_REGISTER(main, 4);

#define VIDEO_DEV DT_LABEL(DT_INST(0, byd_bf30a2))

#if !DT_PROP(DT_INST(0, byd_bf30a2), csk6_spec)
#define TEST_AT_CP 1
#else
#define TEST_AT_CP 0
#endif

#if !TEST_AT_CP

#define CP_BOOT_ADDRESS 0x60000000
#define SYSCTRL_RESET_CP (0xCAFE000B)
#define SYSCTRL_RELEASE_CP (0xCAFE100B)
#define SYSCTRL_RESET_NPU (0xCAFE000C)
#define CP_PSRAM_BASE (0x68400000)
#define AP_PSRAM_BASE (0x30000000)

#define CP_FW_ADDR (0x18000000 + 0x00200000)
#define CP_FW_LEN 500000

// void prepare_cp_run()
// {
// 	PSRAM_Initialize();

// 	SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;

// 	// set boot address
// 	cmn_sys_ctrl->REG_CP_CTRL2.all = CP_BOOT_ADDRESS;
// 	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 0x1;

// 	// stall HIFI4
// 	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;

// 	// release CP & NPU
// 	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
// 	if (cmn_sys_ctrl->REG_SW_RESET_NPU.all) { // = 1 NPU reset
// 		cmn_sys_ctrl->REG_SW_RESET_NPU.all = SYSCTRL_RESET_NPU;
// 	}

// 	__DMB();
//     printk("Copy Image, %X->%X, size:%u.\n", CP_FW_ADDR, AP_PSRAM_BASE, CP_FW_LEN);
//     memcpy((void *)AP_PSRAM_BASE, (void *)CP_FW_ADDR, CP_FW_LEN);

//     sys_cache_data_range((void *)AP_PSRAM_BASE, CP_FW_LEN, K_CACHE_WB);
// 	__DSB();

// 	// run cp
// 	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
// 	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
// 	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

// 	printk("Set CP boot address:%x.\n", CP_BOOT_ADDRESS);
// 	printk("Boot CP...\n");
// }

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
	printk("Hello World! %s\n", CONFIG_BOARD);

    const struct device *video = device_get_binding(VIDEO_DEV);

	if (!device_is_ready(video)) {
		printk("ipm device is null.\n");
		return;
	}

	const struct device *ipm = DEVICE_DT_GET(DT_NODELABEL(mailbox0));
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
    int i = 0;

	while (1) {
        printk("running, %d\n", i++);
		k_sleep(K_MSEC(1000));
	}
}

#else

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

    const struct device *video = device_get_binding(VIDEO_DEV);

	if (!device_is_ready(video)) {
		printk("ipm device is null.\n");
		return;
	}

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}

static void test_thread_0(void *p1, void *p2, void *p3)
{
    const struct device *video = device_get_binding(VIDEO_DEV);

    while(1) {
        LOG_INF("start video steam.\n");
        /* Start video capture */
        if (video_stream_start(video)) {
            printk("Unable to start video.\n");
            return;
        }
        k_msleep(3 * 1000);
        LOG_INF("stop video steam.\n");
        if (video_stream_stop(video)) {
            printk("Unable to start video.\n");
            return;
        }
        k_msleep(3 * 1000);
    }
}

static void test_thread_1(void *p1, void *p2, void *p3)
{
    while(1) {
        const char *name = k_thread_name_get(k_current_get());
        LOG_INF("thread %s is running.", name);
        k_yield();
    }
}

static void test_thread_2(void *p1, void *p2, void *p3)
{
    struct video_buffer *buf = NULL;
    const struct device *video = device_get_binding(VIDEO_DEV);

    while(1) {
        int r = video_dequeue(video, VIDEO_EP_OUT, &buf, K_FOREVER);
        LOG_INF("video_dequeue r:%d, buf:%p", r, buf);
        if (r == 0 && buf != NULL) {
            video_enqueue(video, VIDEO_EP_OUT, buf);
        }
    }
}

K_THREAD_DEFINE(test_thread_0_thread, 1024,                \
			test_thread_0, NULL, NULL, NULL,               \
			-1, 0, 0);

K_THREAD_DEFINE(test_thread_1_thread, 1024,                \
			test_thread_1, NULL, NULL, NULL,               \
			0, 0, 0);

K_THREAD_DEFINE(test_thread_2_thread, 1024,                \
			test_thread_2, NULL, NULL, NULL,               \
			0 , 0, 0);

#endif