#ifndef _LS_CUTLINE_API_H_
#define _LS_CUTLINE_API_H_

#include "ls_cutline_type.h"

#ifdef __cplusplus
extern "C" {
#endif

cutline_err_t ls_cutline_create(cutline_inst_t *p_inst, p_cutline_init_param params);
cutline_err_t ls_cutline_get_version(cutline_inst_t p_inst, i8_t **version);
cutline_err_t ls_cutline_destroy(cutline_inst_t p_inst);
cutline_err_t ls_cutline_set_param(cutline_inst_t p_inst, i32_t key, void *value);
cutline_err_t ls_cutline_get_param(cutline_inst_t p_inst, i32_t key, void *value);
cutline_err_t ls_cutline_process(cutline_inst_t p_inst, cutline_frame *input, cutline_frame *output);

#ifdef __cplusplus
}
#endif
#endif /* _LS_CUTLINE_API_H_ */
