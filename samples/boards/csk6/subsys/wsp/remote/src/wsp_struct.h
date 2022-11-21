#ifndef __WSP_STRUCT_H__
#define __WSP_STRUCT_H__

#include "wsp_defines.h"
#include "wsp_type.h"

typedef struct _TWspInfo
{
	ivInt share_mem_size;				// 共享内存大小，必须在sharemem中分配 
	ivInt inst_mem_size;				// 实例内存大小，可以在psram、sram或者sharemem中分配

	ivInt input_pcm_channel;			// 输入音频通道数
	ivInt input_pcm_frame_count;		// 输入音频帧数
	ivInt input_pcm_frame_ms;			// 输入音频每帧的时长(单位ms)
	ivInt input_pcm_bit;				// 输入音频采样精度
	ivInt input_pcm_sample_rate;		// 输入音频采样率

	ivInt mlp_res_type;					// 声学模型类型

	char  version[64];					// 版本描述
}T_wsp_info, *PT_wsp_info;

typedef struct _TWspRes
{
	ivUInt32		res_size_arr_[MAX_RES_NUMS];   // 资源大小
	char*   		res_buf_arr_[MAX_RES_NUMS];	   // 资源存放地址
}T_wsp_res, *PT_wsp_res;

typedef struct _TWspInitParam
{
	char *		share_mem_addr;			// 共享内存起始地址
	char *		inst_mem_addr;			// psram起始地址
	ivInt		share_mem_size;			// 外部申请的共享内存大小
	ivInt       inst_mem_size;			// 外部申请的psram内存大小
	ivInt       share_mem_size_used;	// 实例库中实际使用的sharemem内存大小
	ivInt       inst_mem_size_used;		// 实例库中实际使用的psram内存大小
	T_wsp_res   res;					// 资源(包括mlp、word_dict)
}T_wsp_init_param, *PT_wsp_init_param;

//#define MAX_EXTRA_DATA (2)
//typedef struct _TCspExtraData
//{
//	EDataType   data_type;				// 数据类型
//	ivInt		frame;					// 帧数
//	ivInt		dim;					// 维度
//	void *		data[MAX_EXTRA_DATA];	// 数据地址
//}TCspExtraData, *PCspExtraData;
#endif
