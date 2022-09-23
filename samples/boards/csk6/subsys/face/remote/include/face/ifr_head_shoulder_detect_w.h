#ifndef _IFR_HEAD_SHOULDER_DETECT_W_H_
#define _IFR_HEAD_SHOULDER_DETECT_W_H_

#include <stdint.h>
#include "ifr_types.h"
#include "ifr_defines.h"


typedef struct ifr_head_shoulder_detect_t_ ifr_head_shoulder_detect_t;

int ifr_head_shoulder_detect_alloc(ifr_head_shoulder_detect_t** pp_this, int8_t* psram_addr, uint32_t *psram_size, int8_t* share_addr, uint32_t *share_size,
	int8_t* res_data, uint32_t res_size);
void ifr_head_shoulder_detect_destory(ifr_head_shoulder_detect_t* p_this);

int ifr_head_shoulder_detect_process(ifr_head_shoulder_detect_t* p_this, ifr_pixel_format_t fmt, uint8_t *data, uint32_t width, uint32_t height,
	ifr_head_shoulder_detect_result_t *p_detect_result, uint32_t *p_detect_count);
int ifr_head_shoulder_detect_set_param(ifr_head_shoulder_detect_t* p_this, int32_t param, void* value);
int ifr_head_shoulder_detect_get_param(ifr_head_shoulder_detect_t* p_this, int32_t param, void* value, int32_t size);

#endif //_IFR_HEAD_SHOULDER_DETECT_W_H_