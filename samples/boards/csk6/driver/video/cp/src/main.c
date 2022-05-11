/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <drivers/video.h>
#include <drivers/uart.h>
#include <device.h>
#include <drivers/ipm.h>

#include <string.h>

#define VIDEO_DEV DT_LABEL(DT_INST(0, byd_bf30a2))

#define UART2_DEV DT_LABEL(DT_INST(0, listenai_csk_uart))

static void ipm_callback(const struct device *dev, void *context, uint32_t id, volatile void *data)
{
	printk("id=%d.\n", id);
}

void main(void)
{
	const struct device *video = device_get_binding(VIDEO_DEV);
	const struct device *ipm = DEVICE_DT_GET(DT_NODELABEL(mailbox0));
	struct video_buffer *vbuf;
	int ret;

	// xthal_set_cache_prefetch(XTHAL_PREFETCH_DISABLE);
	// xthal_set_region_attribute((int *)0x5FF00000, 0x2000, XCHAL_CA_BYPASS, 0);

	printk("Hello World! %s\n", CONFIG_BOARD);

	if (!device_is_ready(ipm)) {
		printk("ipm device is null.\n");
		return;
	}

	if (video == NULL) {
		printk("video device is null.\n");
		return;
	}

	int x = 0;

	/* Start video capture. Make sure cp starts before ap. */
	if (video_stream_start(video)) {
		printk("Unable to start video.\n");
		return;
	}
	printk("start video steam.\n");

    /* Send msg to ap core to start the video stream */
	ipm_register_callback(ipm, ipm_callback, NULL);
	ipm_set_enabled(ipm, 1);
	ipm_send(ipm, 1, 0, &x, sizeof(x));

    uint32_t cnt = 0;
    uint32_t err_times = 0;

	uint32_t last_tick = 0;
	while (1) {
		ret = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		if (ret == 0) {
			if (vbuf->timestamp - last_tick > 0) {
				int fps = 1000 / (vbuf->timestamp - last_tick);
				if(fps != 62 && fps != 71 && fps != 66){
					err_times++;
				}
				printk("FPS:%d total: %d err_time: %d rate: %d%%\n", fps, cnt++, err_times, err_times / cnt * 100);
			}
			last_tick = vbuf->timestamp;
		} else {
            printk("recv frame timeout.\n");
        }
	}
}

static void test_thread(void *p1, void *p2, void *p3)
{
    while(1) {
        // printk("test_thread is running.\n");
        k_msleep(10);
    }
}

#define DELAY_MSEC 1000

K_THREAD_DEFINE(bf30a2_thread_id_1, 2048,                                \
			test_thread, NULL, NULL, NULL,               \
			0, 0, DELAY_MSEC + 1);

K_THREAD_DEFINE(bf30a2_thread_id_2, 2048,                                \
			test_thread, NULL, NULL, NULL,               \
			1, 0, DELAY_MSEC + 3);

K_THREAD_DEFINE(bf30a2_thread_id_3, 2048,                                \
			test_thread, NULL, NULL, NULL,               \
			2, 0, DELAY_MSEC + 2);

K_THREAD_DEFINE(bf30a2_thread_id_4, 2048,                                \
			test_thread, NULL, NULL, NULL,               \
			3, 0, DELAY_MSEC + 4);

// K_THREAD_DEFINE(bf30a2_thread_id_5, 2048,                                \
// 			test_thread, NULL, NULL, NULL,               \
// 			4, 0, DELAY_MSEC + 5);
