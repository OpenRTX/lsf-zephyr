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

    uint32_t loop_cnt = 0;
    uint32_t last_tick = 0;
    uint32_t frame_cnt = 0;
    struct video_buffer *buf = NULL;

    uint32_t tick;
    while(1) {
        LOG_INF("start video steam, loop count:%d\n", ++loop_cnt);

        /* Start video capture */
        if (video_stream_start(video)) {
            printk("Unable to start video.\n");
            return;
        }
        
        tick = k_uptime_get_32();
        while((k_uptime_get_32() - tick) < 3000) {
            video_dequeue(video, VIDEO_EP_OUT, &buf, K_FOREVER);
            if (buf != NULL) {
                frame_cnt++;
                int fps = (buf->timestamp - last_tick) > 0 ? 1000 / (buf->timestamp - last_tick) : 0;
                // LOG_INF("FPS:%d, frame count:%d, vbuf:%p.", fps, frame_cnt, buf);
                last_tick = buf->timestamp;
                memset(buf->buffer, 0, buf->bytesused);
                video_enqueue(video, VIDEO_EP_OUT, buf);
            }
            buf = NULL;
        }

        LOG_INF("stop video steam, loop count:%d\n", loop_cnt);
        /* buffer will be automatically flushed after stopping. */
        if (video_stream_stop(video)) {
            printk("Unable to start video.\n");
            return;
        }

        int r;
        /* NO WAIT */
        while((r = video_dequeue(video, VIDEO_EP_OUT, &buf, K_NO_WAIT)) == 0) {
            memset(buf->buffer, 0, buf->bytesused);
            video_enqueue(video, VIDEO_EP_OUT, buf);
        }
        LOG_INF("sleep.");
        k_msleep(1 * 1000);
    }
}

K_THREAD_DEFINE(test_thread_0_thread, 1024,                \
			test_thread_0, NULL, NULL, NULL,               \
			0, 0, 0);
