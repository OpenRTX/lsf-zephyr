#ifndef _ISTITCH_API_H_
#define _ISTITCH_API_H_

#include "istitch_type.h"

#ifdef __cplusplus
extern "C" {
#endif

	ISTITCH_ERROR iStitchCreate(ISTITCH_INST *pInst, PIStitchInitParam pParams);

	ISTITCH_ERROR iStitchGetVersion(ISTITCH_INST pInst,ivStr *pszVersion);

	ISTITCH_ERROR iStitchDestroy(ISTITCH_INST pInst);

	ISTITCH_ERROR iStitchSetParam(ISTITCH_INST pInst,ivInt32 key,ivCPointer value);

	ISTITCH_ERROR iStitchGetParam(ISTITCH_INST pInst, ivInt32 key, ivPointer value);

	ISTITCH_ERROR iStitchInput(ISTITCH_INST pInst, ivInt32 nDataType, ivPointer pData, ivUInt32 nDataSize,ISTITCH_ISTATUS nDataStatus);

	ISTITCH_ERROR iStitchGetResult(ISTITCH_INST pInst, ivInt32 nResultType, ivPointer pResult, ivPUInt32 nResultSize, ISTITCH_OSTATUS *nResultStatus);

#ifdef __cplusplus
}
#endif
#endif
