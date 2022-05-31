#ifndef _ISTITCH_TYPE_H_
#define _ISTITCH_TYPE_H_


#include "istitch_define.h"

typedef  void * ISTITCH_INST;
typedef unsigned int ISTITCH_ERROR;
typedef unsigned int ISTITCH_ISTATUS;
typedef unsigned int ISTITCH_OSTATUS;

#define ISTITCH_PIX_FMT_DEFAULT     0
#define ISTITCH_PIX_FMT_GRAY8       1
#define ISTITCH_PIX_FMT_RGB888      2
#define ISTITCH_PIX_FMT_BGR888      3
#define ISTITCH_PIX_FMT_RGBA8888    4
#define ISTITCH_PIX_FMT_BGRA8888    5
#define ISTITCH_PIX_FMT_NV21        6
#define ISTITCH_PIX_FMT_NV12        7
#define ISTITCH_PIX_FMT_I420        8
#define ISTITCH_PIX_FMT_YV12        9

#define ISTITCH_INPUT_DATA_FRAME                0               ///< 拼接数据输入-图像帧数据（参考ISTITCH_FRAME结构）
#define ISTITCH_INPUT_DATA_STREAM               1               ///< 拼接数据输入-文件字节流
#define ISTITCH_RESULT_DATA_FRAME               0               ///< 拼接输出结果类型-图像帧数据（参考ISTITCH_FRAME结构）
#define ISTITCH_RESULT_DATA_SHIFT               1               ///< 拼接输出结果类型-当前帧相对于上一帧的偏移

#define ISTITCH_ISTATUS_BEGIN                   0               ///< 输入数据状态（首帧数据）
#define ISTITCH_ISTATUS_CONTINUE                1               ///< 输入数据状态（中间数据）
#define ISTITCH_ISTATUS_END                     2               ///< 输入数据状态（末帧数据）

#define ISTITCH_OSTATUS_NONE                    0               ///< 输出数据状态（无结果）
#define ISTITCH_OSTATUS_CONTINUE                1               ///< 输出数据状态（中间结果）
#define ISTITCH_OSTATUS_DONE                    2               ///< 输出数据状态（最终结果）

typedef enum _IStitchEngineParam
{
	ISTITCH_INST_PARAM_INVALID = -1,
	ISTITCH_INST_PARAM_SKIP_X_PIXEL,
	ISTITCH_INST_PARAM_SKIP_FRAME_MAX,
	ISTITCH_INST_PARAM_BLANK_NUM,
	ISTITCH_INST_PARAM_MATCH_TH,
}EIStitchEngineParam;

typedef struct _IStitchInitParam
{
	ivPByte RamAddr;
	ivPByte PSRamAddr;
	ivPByte TempRamAddr;

	ivUInt32 RamTotal;
	ivUInt32 RamUsed;
	
	ivUInt32 PSRamTotal;
	ivUInt32 PSRamUsed;

	ivUInt32 TempRamTotal;
	ivUInt32 TempRamUsed;

}TIStitchInitParam,*PIStitchInitParam;

typedef struct _ISTITCH_FRAME
{
	ivUInt32    fmt;
	union
	{
		ivInt32 w;
		ivInt32 width;
	};
	union
	{
		ivInt32 h;
		ivInt32 height;
	};
	union
	{
		ivInt32 step;
		ivInt32 stride;
		ivInt32 channel;
	};
	ivPByte data;
} ISTITCH_FRAME;

#endif
