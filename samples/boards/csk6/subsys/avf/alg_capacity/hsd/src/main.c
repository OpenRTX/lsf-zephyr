/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsp_resource.h"
#include <zephyr.h>
#include <stdio.h>
#include <avf/framework/avf_platform.h>
#include <avf/modules/hsd/hsd.h>
#include <drivers/video.h>
#include <device.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#define VIDEO_DEV DT_LABEL(DT_NODELABEL(dvp))
#define VBUF_NUM 2
#define IMAGE_HEIGHT 240
#define IMAGE_WIDTH 320
#define IMAGE_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT * 2)

const struct device *video;
uint8_t image_buf[IMAGE_SIZE];

void main(void)
{
	if (NULL == avf_stream_platform_register(dsp_firmware, sizeof(dsp_firmware), dsp_tplg,
						 sizeof(dsp_tplg))) {
		printk("avf platform register failed,exit.\n");
		return;
	}

	hsd_t *hsd = hsd_create();

	hsd_set_flag(hsd, HSD_FLAG_DEFAULT);

	// video = device_get_binding(VIDEO_DEV);

	// if (video == NULL) {
	// 	LOG_ERR("Video device %s not found, "
	// 		"fallback to software generator.",
	// 		VIDEO_DEV);

	// 	return;
	// }

	// printk("- Device name: %s\n", VIDEO_DEV);

	// hsd_start(hsd, video);

	// while (1) {
	// 	LOG_DBG("Loop transporting...");
	// }

	// hsd_stop(hsd);

	printk("AP EXEC END\n");
}
