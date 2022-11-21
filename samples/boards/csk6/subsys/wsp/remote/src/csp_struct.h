#ifndef __CSP_STRUCT_H__
#define __CSP_STRUCT_H__

#include "csp_defines.h"
#include "csp_type.h"

typedef struct _TCspInfo
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
}T_csp_info, *PT_csp_info;

typedef struct _TCspRes
{
	ivUInt32		res_size_arr_[MAX_RES_NUMS];   // 唤醒资源大小
	char*   		res_buf_arr_[MAX_RES_NUMS];	   // 唤醒资源存放地址
}T_csp_res, *PT_csp_res;

typedef struct _TCspInitParam
{
	char *		share_mem_addr;			// 共享内存起始地址
	char *		inst_mem_addr;			// psram起始地址
	ivInt		share_mem_size;			// 外部申请的共享内存大小
	ivInt       inst_mem_size;			// 外部申请的psram内存大小
	ivInt       share_mem_size_used;	// 实例库中实际使用的sharemem内存大小
	ivInt       inst_mem_size_used;		// 实例库中实际使用的psram内存大小
	T_csp_res   res;					// 资源(包括mlp、word_dict)
}T_csp_init_param, *PT_csp_init_param;

//#define MAX_EXTRA_DATA (2)
//typedef struct _TCspExtraData
//{
//	EDataType   data_type;				// 数据类型
//	ivInt		frame;					// 帧数
//	ivInt		dim;					// 维度
//	void *		data[MAX_EXTRA_DATA];	// 数据地址
//}TCspExtraData, *PCspExtraData;
#endif
