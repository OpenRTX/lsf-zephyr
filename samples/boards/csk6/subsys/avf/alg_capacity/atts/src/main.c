/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <avf/framework/avf_platform.h>
#include "dsp_resource.h"

#include "pwr_amp.c"

#define TTS_STRING "这是一段TTS测试语音"

extern int atts_playback(const char *tts_string);

void main(void)
{
	if (NULL == avf_stream_platform_register(NULL, 0, dsp_tplg, sizeof(dsp_tplg))) {
		printk("avf platform register failed,exit.\n");
		return;
	}
	printk("avf platform register success\n");

	/*Enable audio power-amp*/
	audio_pwr_amp_init();
	audio_pwr_amp_enbale();

	printk("tts playback start\n");
	if (0 != atts_playback(TTS_STRING)) {
		printk("tts playback failed\n");
		return;
	}
	printk("tts playback over\n");
}
