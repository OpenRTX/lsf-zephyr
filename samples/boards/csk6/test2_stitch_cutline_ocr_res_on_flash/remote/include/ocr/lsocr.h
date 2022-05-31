#ifndef _ISOCR_API_H_
#define _ISOCR_API_H_

#include "lsocr_type.h"
#include "lsocr_err.h"

#ifdef __cplusplus
extern "C" {
#endif
	lsocr_errcode_t lsocr_get_info(lsocr_info_t **info);

	lsocr_errcode_t lsocr_create(lsocr_handle_t *handle, lscor_init_param_t *params);

	lsocr_errcode_t lsocr_get_version(lsocr_handle_t handle,str_t *version);

	lsocr_errcode_t lsocr_destroy(lsocr_handle_t handle);

	lsocr_errcode_t lsocr_set_param(lsocr_handle_t handle,int32_t key, cptr_t value,uint32_t len);

	lsocr_errcode_t lsocr_get_param(lsocr_handle_t handle, int32_t key, ptr_t value, uint32_t *len);

	lsocr_errcode_t lsocr_process(lsocr_handle_t handle, lsocr_frame_t in_data, lsocr_result_t *result);


#ifdef __cplusplus
}
#endif
#endif
