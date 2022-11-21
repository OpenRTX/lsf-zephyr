#ifndef __LS_CSP_H__
#define __LS_CSP_H__

#include "csp_defines.h"
#include "csp_type.h"

typedef void *LS_CSP_HANDLE;
#include "csp_struct.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

LS_CSP_API ivInt ls_csp_get_info(_Out PT_csp_info p_info, _In PT_csp_res p_res);

LS_CSP_API ivInt ls_csp_create(_Out LS_CSP_HANDLE *ph_csp_inst, _Inout PT_csp_init_param p_init_param);

LS_CSP_API ivInt ls_csp_destroy(_In LS_CSP_HANDLE h_csp_inst);

LS_CSP_API ivInt ls_csp_set_parameter(_In LS_CSP_HANDLE h_csp_inst, _In ivInt param, _In ivInt value);

LS_CSP_API ivInt ls_csp_get_parameter(_In LS_CSP_HANDLE h_csp_inst, _In ivInt param, _Out ivInt *value);

LS_CSP_API ivInt ls_csp_write(_In LS_CSP_HANDLE h_csp_inst, _In short *samples, _In ivInt samples_in_char);

LS_CSP_API ivInt ls_csp_get_rlt(_In LS_CSP_HANDLE h_csp_inst, _In char *word_phone, _Out void *rst, _Inout ivInt *buf_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //__w_csp_H__

