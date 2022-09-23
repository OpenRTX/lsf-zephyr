#ifndef _IFR_GASTURE_RECOGNIZE_W_H_
#define _IFR_GASTURE_RECOGNIZE_W_H_

#include <stdint.h>
#include "ifr_types.h"
#include "ifr_defines.h"

typedef struct ifr_gasture_recognize_t_ ifr_gasture_recognize_t;

int ifr_gasture_recognize_alloc(ifr_gasture_recognize_t** pp_this, int8_t* psram_addr, uint32_t *psram_size, int8_t* share_addr, uint32_t *share_size,
	int8_t* res_data, uint32_t res_size);
void ifr_gasture_recognize_destory(ifr_gasture_recognize_t* p_this);

int ifr_gasture_recognize_process(ifr_gasture_recognize_t* p_this, ifr_pixel_format_t fmt, uint8_t *data, uint32_t width, uint32_t height, ifr_rect_t* roi,
	ifr_gasture_recognize_result_t *p_feature_result, uint32_t *p_feature_num);
int ifr_gasture_recognize_set_param(ifr_gasture_recognize_t* p_this, int32_t param, void* value);
int ifr_gasture_recognize_get_param(ifr_gasture_recognize_t* p_this, int32_t param, void* value, int32_t size);

#endif //_IFR_GASTURE_RECOGNIZE_W_H_