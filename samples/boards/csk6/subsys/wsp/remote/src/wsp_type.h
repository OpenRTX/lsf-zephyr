#ifndef __WSP_TYPE_H__
#define __WSP_TYPE_H__

#include "wsp_defines.h"
// typedef enum
// {
// 	CallBackFuncNameCry = 0,
// 	CallBackFuncNameCount
// }ECallBackType;

// typedef enum
// {
// 	DEFAULT_RESET = 0, // �����ý���ģ��
// 	ALL_RESET		   // �������й���ģ��
// }EResetType;

#define MAX_RES_NUMS (6)

// typedef enum
// {
// 	E_DATA_FEA_NO_NORM = 0,				// ��������
// 	E_DATA_FEA_NORM
// }EDataType;

typedef enum
{
	WSP_INDEX_MLP_CNN = 0,
	WSP_INDEX_MLP_ENCODER,
	WSP_INDEX_MLP_DECODER,
	WSP_INDEX_PHONE_DICT,
}EWspResIndex;

typedef enum
{
	E_INPUT_STATUS_BEGIN = 0,		//input begin,will reset all input data and result;
	E_INPUT_STATUS_CONTINUE = 1, 	//append data  and can get middle result;
	E_INPUT_STATUS_END = 2,			// finished input data ,and can get final result;
	E_INPUT_STATUS_CANCEL = 3,
}EInputStatus;

typedef int phone_score_type;
#endif
