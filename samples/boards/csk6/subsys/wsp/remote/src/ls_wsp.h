#ifndef __LS_WSP_H__
#define __LS_WSP_H__

#include "wsp_defines.h"
#include "wsp_type.h"

typedef void *LS_WSP_HANDLE;
#include "wsp_struct.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

LS_WSP_API ivInt ls_wsp_get_info(_Out PT_wsp_info p_info, PT_wsp_res p_res);

LS_WSP_API ivInt ls_wsp_create(_Out LS_WSP_HANDLE *ph_wsp_inst, _Inout PT_wsp_init_param p_init_param);

LS_WSP_API ivInt ls_wsp_destroy(_In LS_WSP_HANDLE h_wsp_inst);

LS_WSP_API ivInt ls_wsp_set_parameter(_In LS_WSP_HANDLE h_wsp_inst, _In ivInt param, _In ivInt value);

LS_WSP_API ivInt ls_wsp_get_parameter(_In LS_WSP_HANDLE h_wsp_inst, _In ivInt param, _Out ivInt *value);

LS_WSP_API ivInt ls_wsp_start(_In LS_WSP_HANDLE h_wsp_inst);

LS_WSP_API ivInt ls_wsp_stop(_In LS_WSP_HANDLE h_wsp_inst);

LS_WSP_API ivInt ls_wsp_cancel(_In LS_WSP_HANDLE h_wsp_inst);

LS_WSP_API ivInt ls_wsp_write(_In LS_WSP_HANDLE h_wsp_inst, _In short *samples, _In ivInt samples_in_char);

LS_WSP_API ivInt ls_wsp_get_rlt(_In LS_WSP_HANDLE h_wsp_inst,  _Out void *rst, _Inout ivInt *buf_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //__w_csp_H__

