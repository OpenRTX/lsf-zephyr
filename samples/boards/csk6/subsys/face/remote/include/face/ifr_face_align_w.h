#ifndef _IFR_FACE_ALIGN_W_H_
#define _IFR_FACE_ALIGN_W_H_

#include <stdint.h>
#include "ifr_types.h"
#include "ifr_defines.h"

typedef struct ifr_face_align_t_ ifr_face_align_t;

int ifr_face_align_alloc(ifr_face_align_t** pp_this, int8_t* psram_addr, uint32_t *psram_size, int8_t* share_addr, uint32_t *share_size,
	int8_t* res_data, uint32_t res_size);
void ifr_face_align_destory(ifr_face_align_t* p_this);

int ifr_face_align_process(ifr_face_align_t* p_this, ifr_pixel_format_t fmt, uint8_t *data, uint32_t width, uint32_t height, ifr_rect_t* roi,
	ifr_face_align_result_t *p_result, uint32_t *p_result_count);
int ifr_face_align_set_param(ifr_face_align_t* p_this, int32_t param, void* value);
int ifr_face_align_get_param(ifr_face_align_t* p_this, int32_t param, void* value, int32_t size);

#endif //_IFR_FACE_ALIGN_W_H_