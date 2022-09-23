#ifndef _IFR_ANTI_SPOOFING_W_H_
#define _IFR_ANTI_SPOOFING_W_H_

#include <stdint.h>
#include "ifr_types.h"
#include "ifr_defines.h"

typedef struct ifr_anti_spoofing_t_ ifr_anti_spoofing_t;

int ifr_anti_spoofing_alloc(ifr_anti_spoofing_t** pp_this, int8_t* psram_addr, uint32_t *psram_size, int8_t* share_addr, uint32_t *share_size,
	int8_t* res_data, uint32_t res_size);
void ifr_anti_spoofing_destory(ifr_anti_spoofing_t* p_this);

int ifr_anti_spoofing_process(ifr_anti_spoofing_t* p_this, ifr_pixel_format_t fmt, uint8_t *data, uint32_t width, uint32_t height, ifr_rect_t* roi,
	ifr_anti_spoofing_result_t *p_feature_result, uint32_t *p_feature_num);
int ifr_anti_spoofing_set_param(ifr_anti_spoofing_t* p_this, int32_t param, void* value);
int ifr_anti_spoofing_get_param(ifr_anti_spoofing_t* p_this, int32_t param, void* value, int32_t size);

#endif //_IFR_ANTI_SPOOFING_W_H_