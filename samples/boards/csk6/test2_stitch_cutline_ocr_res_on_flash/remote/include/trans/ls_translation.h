#ifndef __LS_TRANSLATION_H__
#define __LS_TRANSLATION_H__

#include <stdint.h>
#include "ls_translation_def.h"
#include "ls_translation_type.h"



#ifdef __cplusplus
extern "C" {
#endif
	extern lstrans_errcode_e lstrans_get_info(mem_addr_t* res, char *info, int *info_size);

	extern lstrans_errcode_e lstrans_create(lstrans_inst *handle, lstrans_mem_t *params);

	extern lstrans_errcode_e lstrans_destroy(lstrans_inst handle);

	extern lstrans_errcode_e lstrans_set_param(lstrans_inst handle, int32_t key, void* value, uint32_t len);

	extern lstrans_errcode_e lstrans_get_param(lstrans_inst handle, int32_t key, void* value, uint32_t *len);
	/*UTF8 string*/
	extern lstrans_errcode_e lstrans_process(lstrans_inst handle, data_t* in_data, data_t* result, void* ctrl);

	extern lstrans_errcode_e lstrans_register_callback(lstrans_inst handle, lstans_cb_type_e FuncType, 
													  const PLSTransCallBack pFunc, void *pUserParam);

	extern lstrans_errcode_e lstrans_unregister_callback(lstrans_inst handle, lstans_cb_type_e FuncType);

#ifdef __cplusplus
}
#endif

#endif