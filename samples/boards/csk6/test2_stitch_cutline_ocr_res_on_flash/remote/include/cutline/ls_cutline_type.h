#ifndef _LS_CUTLINE_TYPE_H_
#define _LS_CUTLINE_TYPE_H_


#include "ls_cutline_define.h"

typedef  void * cutline_inst_t;
typedef u32_t cutline_err_t;
typedef u32_t cutline_status;

#define CUTLINE_PIX_FMT_DEFAULT     0
#define CUTLINE_PIX_FMT_GRAY8       1
#define CUTLINE_PIX_FMT_RGB888      2
#define CUTLINE_PIX_FMT_BGR888      3
#define CUTLINE_PIX_FMT_RGBA8888    4
#define CUTLINE_PIX_FMT_BGRA8888    5
#define CUTLINE_PIX_FMT_NV21        6
#define CUTLINE_PIX_FMT_NV12        7
#define CUTLINE_PIX_FMT_I420        8
#define CUTLINE_PIX_FMT_YV12        9

#define CUTLINE_INPUT_DATA_FRAME                0               ///< 拼接数据输入-图像帧数据（参考CUTLINE_FRAME结构）
#define CUTLINE_INPUT_DATA_STREAM               1               ///< 拼接数据输入-文件字节流
#define CUTLINE_RESULT_DATA_FRAME               0               ///< 拼接输出结果类型-图像帧数据（参考CUTLINE_FRAME结构）
#define CUTLINE_RESULT_DATA_SHIFT               1               ///< 拼接输出结果类型-当前帧相对于上一帧的偏移

#define CUTLINE_ISTATUS_BEGIN                   0               ///< 输入数据状态（首帧数据）
#define CUTLINE_ISTATUS_CONTINUE                1               ///< 输入数据状态（中间数据）
#define CUTLINE_ISTATUS_END                     2               ///< 输入数据状态（末帧数据）

#define CUTLINE_OSTATUS_NONE                    0               ///< 输出数据状态（无结果）
#define CUTLINE_OSTATUS_CONTINUE                1               ///< 输出数据状态（中间结果）
#define CUTLINE_OSTATUS_DONE                    2               ///< 输出数据状态（最终结果）

typedef enum CUTLINE_ENG_PARAM_ENUM
{
	CUTLINE_INST_PARAM_INVALID = -1,
	CUTLINE_INST_PARAM_SKIP_X_PIXEL,
	CUTLINE_INST_PARAM_SKIP_FRAME_MAX,
	CUTLINE_INST_PARAM_BLANK_NUM,
	CUTLINE_INST_PARAM_MATCH_TH,
}E_CUTLINE_ENG_PARAM;

typedef struct _cutline_init_param
{
	u32_t *ram_addr;
	u32_t *psram_addr;
	u32_t *temp_ram_addr;
	u32_t *res_addr;

	u32_t ram_total;
	u32_t ram_used;
	u32_t psram_total;
	u32_t psram_used;
	u32_t temp_ram_total;
	u32_t temp_ram_used;
	u32_t res_size;
}cutline_init_param, *p_cutline_init_param;

typedef struct _cutline_img_size
{
	u32_t width;
	u32_t height;
	u32_t channel;
}cutline_img_size;

typedef struct _cutline_frame
{
	u32_t    fmt;
	union
	{
		u32_t w;
		u32_t width;
	};
	union
	{
		u32_t h;
		u32_t height;
	};
	union
	{
		u32_t channel;
		u32_t step;
	};
	
	byte_t* data;
}cutline_frame;

#endif /* _LS_CUTLINE_TYPE_H_ */
