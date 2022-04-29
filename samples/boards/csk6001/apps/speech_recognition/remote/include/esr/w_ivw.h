#ifndef __W_IVW_H__
#define __W_IVW_H__

#include "param_module.h"
#include "ivw_defines.h"
#include "ivw_type.h"

typedef void *WIVW_INST;
#include "ivw_struct.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

WIVW_API ivInt wIvwGetInfo(_Out PIvwInfo pInfo);

WIVW_API ivInt wIvwCreate(_Out WIVW_INST  *wIvwInst, _Inout const PIvwInitParam pInitParam);

WIVW_API ivInt wIvwDestroy(_In WIVW_INST wIvwInst);

/**************************************************/
/*
 *	功能:设置参数
 *  参数: 
 *		wInwInst 唤醒实例句柄
 *		param    参数名称，请查看param_module.h
 *		value	 要设置的参数值
 *	注意: 需要在wIvwStart前设置参数。设置门限，需要在wIvwStart之后。
 */
WIVW_API ivInt wIvwSetParameter(_In WIVW_INST wIvwInst, _In ivInt param, _In ivInt value);

WIVW_API ivInt wIvwGetParameter(_In WIVW_INST wIvwInst, _In ivInt param, _Out ivInt *value);

WIVW_API ivInt wIvwRegisterCallBacks(_In WIVW_INST wIvwInst, _In ECallBackType FuncType, _In const PIVWCallBack pFunc, _In_opt void *pUserParam);

WIVW_API ivInt wIvwUnRegisterCallBacks(_In WIVW_INST wIvwInst, _In ECallBackType FuncType);

WIVW_API ivInt wIvwStart(_In WIVW_INST wIvwInst, _In EResetType reset_type);

WIVW_API ivInt wIvwStop(_In WIVW_INST wIvwInst);

WIVW_API ivInt wIvwWrite(_In WIVW_INST wIvwInst, _In const short *samples, _In ivInt nSamplesInChar);

WIVW_API ivInt wIvwWriteOtherAttr(_In WIVW_INST wIvwInst, _In EDataType data_type, _In_opt PIvwExtraData data);

WIVW_API ivInt wIvwGetStatus(_In WIVW_INST wIvwInst, _In EStatusID eStatusId, _Out void *rst, _Inout ivInt *nBufLen);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //__W_IVW_H__

