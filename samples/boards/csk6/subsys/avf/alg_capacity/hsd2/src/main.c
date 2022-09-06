/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsp_resource.h"
#include <zephyr.h>
#include <stdio.h>
#include <avf/framework/avf_platform.h>
#include <avf/avf_topology_def.h>
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
#define __packed  __attribute__((packed,aligned(4)));
const struct device *video;
uint8_t image_buf[IMAGE_SIZE];
struct ePILOT_INST_PARAM
{
        void *ram_addr;
        void *psram_addr;

        unsigned int ram_total;
		unsigned int ram_used;
        unsigned int psram_total;
		unsigned int psram_used;
}__packed;

typedef enum
{
	PARAM_PILOT_NONE					= 0,

	//FACE DETECT
	PARAM_PILOT_FACE_DETECT_BEGIN       = 3000,
	PARAM_DETECT_THRES                  = 3001,
	PARAM_DETECT_PROTHRES				= 3002,
	PARAM_DETECT_NMSTHRES				= 3003,
	PARAM_DETECT_PIXESIZE				= 3004,
	PARAM_DETECT_RESVERSION				= 3005,
	PARAM_PILOT_FACE_DETECT_END,

	//FACE VERIFY
	PARAM_PILOT_FACE_VERIFY_BEGIN       = 4000,
	PARAM_VERIFY_THRES                  = 4001,
	PARAM_VERIFY_REGU_A					= 4002,
	PARAM_VERIFY_REGU_B					= 4003,
	PARAM_VERIFY_RESVERSION             = 4004,
	PARAM_PILOT_FACE_VERIFY_END,

	//HEAD SHOULDER
	PARAM_PILOT_HEAD_SHOULDER_DETECT_BEGIN	= 5000,
	PARAM_HEAD_SHOULDER_DETECT_THRES	= 5001,
	PARAM_HEAD_SHOULDER_DETECT_LOSS_CNT = 5002,
	PARAM_HEAD_SHOULDER_DETECT_IOU		= 5003,
	PARAM_HEAD_SHOULDER_DETECT_PIXESIZE = 5004,
	PARAM_HEAD_SHOULDER_DETECT_PROTHRES = 5005,
	PARAM_HEAD_SHOULDER_DETECT_NMSTHRS  = 5006,
	PARAM_HEAD_SHOULDER_DETECT_MODEL    = 5007,
	PARAM_HEAD_SHOULDER_DETECT_IOA		= 5008,
	PARAM_HEAD_SHOULDER_DETECT_IOB		= 5009,
	PARAM_HEAD_SHOULDER_DETECT_CIOU		= 5010,
	PARAM_HEAD_SHOULDER_DETECT_TIMEOUT	= 5011,
	PARAM_HEAD_SHOULDER_DETECT_RESVERSION=5012,
	PARAM_PILOT_HEAD_SHOULDER_DETECT_END,

	//FACE ALIGN
	PARAM_PILOT_FACE_ALIGN_BEGIN 		= 6000,
	PARAM_ALIGN_RESVERSION              = 6001,
	PARAM_PILOT_FACE_ALIGN_END,

	//GESTURE
	PARAM_PILOT_GESTURE_BEGIN           = 7000,
	PARAM_GESTURE_YES_THRES             = 7001,
	PARAM_GESTURE_OK_THRES              = 7002,
	PARAM_GESTURE_STOP_THRES            = 7003,
	PARAM_GESTURE_LIKE_THRES            = 7004,
	PARAM_GESTURE_SIX_THRES             = 7005,
	PARAM_GESTURE_OTHER_THRES           = 7006,
	PARAM_GESTURE_RESVERSION            = 7007,
	PARAM_PILOT_GESTURE_END,
}PILOT_PARAM;
struct PILOT_SET_PARAM
{
	PILOT_PARAM param_name;
	char* value;
	int len; 
}__packed;
	
typedef enum
{
	PIX_FMT_GRAY8 = 1,
	PIX_FMT_BGR888 = 3,
	PIX_FMT_BGRA8888 = 4
}pixel_format;

typedef enum{
	TYPE_GESTURE_RECOGNITION,
	TYPE_HEAD_SHOULDER_DETECT
}hsd_type;

struct HSD_SET_DATA{
	hsd_type type;
	pixel_format pixel_format;
	int width;
	int height;
	void* data;
}__packed;
#include <avf/avf_topology_def.h>
void main(void)
{
	int iret;
	if (NULL == avf_stream_platform_register(dsp_firmware, sizeof(dsp_firmware), dsp_tplg,
						 sizeof(dsp_tplg))) {
		printk("avf platform register failed,exit.\n");
		return;
	}

	hsd_t *hsd = hsd_create();

	struct ePILOT_INST_PARAM init_param;
	int *addr = 1;	
	init_param.psram_addr = addr;
	init_param.ram_addr = addr;
	init_param.ram_total = 503;
	init_param.ram_used = 666;
	init_param.psram_total = 456;
	init_param.psram_used = 999;
	hsd_set_params(hsd, AVF_CTL_MSGID_HSD_INIT_ADRRESS, &init_param, sizeof(init_param));

	struct PILOT_SET_PARAM set_params;
	set_params.param_name = PARAM_DETECT_THRES;
	set_params.value = "2.34";
	set_params.len = strlen(set_params.value);
	hsd_set_params(hsd, AVF_CTL_MSGID_HSD_SET_PARAMS, &set_params, sizeof(set_params));

	struct HSD_SET_DATA set_data;
	set_data.type = TYPE_HEAD_SHOULDER_DETECT;
	set_data.pixel_format = PIX_FMT_BGR888;
	set_data.height = 320;
	set_data.width = 180;
	set_data.data = "abcd";
	hsd_set_params(hsd, AVF_CTL_MSGID_HSD_GET_PARAMS, &set_data, sizeof(set_data));

	
	iret = avf_ctl_get_binary(avf_stream_platform_get(),AVG_CTL_COMP_NAME_HSD_CONFIG,AVF_CTL_MSGID_HSD_INIT_ADRRESS,image_buf,256);
	printk("get data len %d\n",iret);

	// hsd_start_no_video(hsd);

	// while (1) {
	// 	LOG_DBG("Loop transporting...");
	// }

	// hsd_stop(hsd);

	printk("AP EXEC END\n");
}
