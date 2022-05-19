/*
 * Copyright (c) 2022 ListenAI
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
