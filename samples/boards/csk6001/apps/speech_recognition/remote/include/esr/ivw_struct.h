#ifndef __IVW_STRUCT_H__
#define __IVW_STRUCT_H__

#include "ivw_defines.h"
#include "ivw_type.h"

typedef struct _WakeUpResult
{
	ivInt iFrameStart_;
	ivInt nFrameDuration_;
	ivInt nFillerScore_;
	ivInt nKeyWordScore_;

	ivInt nCM_Thresh_;
	ivInt nCM_;
	ivInt iResID_;
	char  pSzLabel_[64];
}TWakeUpResult;

typedef struct _TIvwInfo
{
	ivInt share_mem_size;				// 共享内存大小，必须在sharemem中分配 
	ivInt inst_mem_size;				// 实例内存大小，可以在psram、sram或者sharemem中分配

	ivInt input_pcm_channel;			// 输入音频通道数
	ivInt input_pcm_frame_count;		// 输入音频帧数
	ivInt input_pcm_frame_ms;			// 输入音频每帧的时长(单位ms)
	ivInt input_pcm_bit;				// 输入音频采样精度
	ivInt input_pcm_sample_rate;		// 输入音频采样率

	ivInt output_frame_ms;		        // 输出结果帧长(单位ms)

	ivInt max_main_word_count;			// 主唤醒词最大数量
	ivInt max_asr_word_count;			// 命令词最大数量

	EIvwResType mlp_res_type;			// 声学模型类型

	char  version[64];					// 版本描述
}TIvwInfo, *PIvwInfo;

typedef struct _TIvwRes
{
	ivUInt32		res_size_arr_[MAX_RES_NUMS];   // 唤醒资源大小
	char*   		res_buf_arr_[MAX_RES_NUMS];	   // 唤醒资源存放地址
}TIvwRes, *PIvwRes;

typedef struct _TIvwInitParam
{
	char *		share_mem_addr;			// 共享内存起始地址
	char *		inst_mem_addr;			// psram起始地址
	ivInt		share_mem_size;			// 外部申请的共享内存大小
	ivInt       inst_mem_size;			// 外部申请的psram内存大小
	ivInt       share_mem_size_used;	// 唤醒库中实际使用的sharemem内存大小
	ivInt       inst_mem_size_used;		// 唤醒库中实际使用的psram内存大小
	TIvwRes		ivw_res;				// 资源(包括mlp、keywod_main、keyword_asr)
}TIvwInitParam, *PIvwInitParam;

#define MAX_EXTRA_DATA (2)
typedef struct _TIvwExtraData
{
	EDataType   data_type;				// 数据类型
	ivInt		frame;					// 帧数
	ivInt		dim;					// 维度
	void *		data[MAX_EXTRA_DATA];	// 数据地址
}TIvwExtraData, *PIvwExtraData;
#endif
