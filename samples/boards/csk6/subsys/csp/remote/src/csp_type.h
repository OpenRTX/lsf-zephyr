#ifndef __CSP_TYPE_H__
#define __CSP_TYPE_H__

#include "csp_defines.h"
typedef enum
{
	CallBackFuncNameCry = 0,
	CallBackFuncNameCount
}ECallBackType;

typedef enum
{
	DEFAULT_RESET = 0, // 仅重置解码模块
	ALL_RESET		   // 重置所有功能模块
}EResetType;

#define MAX_RES_NUMS (6)

typedef enum
{
	E_DATA_FEA_NO_NORM = 0,				// 特征数据
	E_DATA_FEA_NORM
}EDataType;

typedef enum
{
	CSP_INDEX_MLP_CNN = 0,
	CSP_INDEX_MLP_ENCODER,
	CSP_INDEX_MLP_DECODER,
	CSP_INDEX_PHONE_DICT,
}ECspResIndex;

typedef int phone_score_type;
#endif
