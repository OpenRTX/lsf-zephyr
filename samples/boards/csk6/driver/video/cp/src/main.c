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

    /* Send msg to ap core to start the video stream */
	ipm_register_callback(ipm, ipm_callback, NULL);
	ipm_set_enabled(ipm, 1);
	ipm_send(ipm, 1, 0, &x, sizeof(x));

	/* Start video capture */
	if (video_stream_start(video)) {
		printk("Unable to start video.\n");
		return;
	}
	printk("start video steam.\n");

	uint32_t last_tick = 0;
	while (1) {
		ret = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		if (ret == 0) {
			if (vbuf->timestamp - last_tick > 0) {
				printk("FPS:%d.\n", 1000 / (vbuf->timestamp - last_tick));
			}
			last_tick = vbuf->timestamp;
		}
	}
}
