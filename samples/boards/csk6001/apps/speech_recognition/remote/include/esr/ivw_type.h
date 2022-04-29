#ifndef __IVW_TYPE_H__
#define __IVW_TYPE_H__
#include "ivw_defines.h"

typedef enum
{
	CallBackFuncNameWakeUpChannel0 = 0,
	CallBackFuncNameWakeUpChannel1,
	CallBackFuncNameWakeUpChannel2,
	CallBackFuncNamePreWakeUpChannel0,
	CallBackFuncNamePreWakeUpChannel1,
	CallBackFuncNamePreWakeUpChannel2,
	CallBackFuncNameGetResult,
	CallBackFuncNameVadBegin,
	CallBackFuncNameVadEnd,
	CallBackFuncNameWavProcessd,
#ifdef HTK_DUMP_MLP_POSTERIOR
	CallBackFuncHtkDumpMlpFeaChannel0,
	CallBackFuncHtkDumpMlpFeaChannel1,
	CallBackFuncHtkDumpMlpFeaChannel2,
#endif
#ifdef HTK_DUMP_FEA
	CallBackFuncHtkDumpFeaChannel0,
	CallBackFuncHtkDumpFeaChannel1,
	CallBackFuncHtkDumpFeaChannel2,
#endif
	CallBackFuncNameCount
}ECallBackType;

typedef enum
{
	DEFAULT_RESET = 0, // 仅重置解码模块
	ALL_RESET		   // 重置所有功能模块
}EResetType;

typedef ivInt (*PIVWCallBack)(const ivChar* pIvwParam,void *pUserParam); 

typedef enum
{
	SID_LAST_WAKE_RLT = 0,		// 最后一次唤醒结果
	SID_PRIOR_DATA				// 模型中的先验信息
}EStatusID;

#define MAX_RES_NUMS (6)

typedef enum
{
	INDEX_FSMN = 0,
	INDEX_WFSA,
	INDEX_KEYWORD_MAIN,
	INDEX_KEYWORD_ASR,
	INDEX_MLP_CNN
}EIvwResIndex;

typedef enum
{
	FSMN = 0,
	WFSA,
	KEYWORD_MAIN,
	KEYWORD_ASR,
	MLP_CNN,
	MLP_CNN_SHUFFLENETV2,
	MLP_CNN_RESGROUP
}EIvwResType;

typedef enum
{
	E_DATA_FLUSH = 0,		// 清空历史，表示情况所有计算过程中产生的历史数据
	E_DATA_FEA,				// 特征数据
	E_DATA_FEA_NORM,
	E_DATA_FEA_FLOAT,
	E_DATA_FEA_POSTER
}EDataType;
#endif
