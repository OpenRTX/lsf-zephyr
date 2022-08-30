/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <avf/framework/avf_platform.h>
#include "dsp_resource.h"

#include "pwr_amp.c"

#define TTS_STRING "聆思科技完成数亿元pre-A轮融资！本轮融资由元禾璞华领投，沄柏资本联合领投，天际资本、中银集团旗下渤海基金、天智投资、连山基金跟投。在本轮融资的加持下，聆思科技必将进一步加速芯片研发，打造极简单芯片系统，助力万物智联时代的爆发。"

extern int  atts_playback(const char* tts_string);

void main(void)
{
	
	if(NULL == avf_stream_platform_register(cp_firmware, sizeof(cp_firmware), dsp_tplg,
				     sizeof(dsp_tplg))){
		printk("avf platform register failed,exit.\n");
		return;
	}
	printk("avf platform register success\n");

	/*Enable audio power-amp*/
	audio_pwr_amp_init();
    audio_pwr_amp_enbale();

	printk("tts playback start\n");
	if(0 != atts_playback(TTS_STRING)){
		printk("tts playback failed\n");
		return;
		
	}
	printk("tts playback over\n");
}
