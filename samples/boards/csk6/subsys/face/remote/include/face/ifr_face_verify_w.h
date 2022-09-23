#ifndef _IFR_FACE_VERIFY_W_H_
#define _IFR_FACE_VERIFY_W_H_

#include <stdint.h>
#include "ifr_types.h"
#include "ifr_defines.h"

typedef struct ifr_face_verify_t_ ifr_face_verify_t;

int ifr_face_verify_alloc(ifr_face_verify_t** pp_this, int8_t* psram_addr, uint32_t *psram_size, int8_t* share_addr, uint32_t *share_size,
	int8_t* res_data, uint32_t res_size);
void ifr_face_verify_destory(ifr_face_verify_t* p_this);

int ifr_face_verify_process(ifr_face_verify_t* p_this, ifr_pixel_format_t fmt, uint8_t *data, uint32_t width, uint32_t height, ifr_rect_t* roi, ifr_point_t *points, uint32_t point_num,
	float *p_feature_result, uint32_t *p_feature_num);
int ifr_face_verify_compare_feature(ifr_face_verify_t* p_this, float *feature1, float *feature2, uint32_t feature_dim, float* score);
int ifr_face_verify_search_featuredb(ifr_face_verify_t* p_this, const float* feature_db, const float* query, unsigned int feature_num, unsigned int feature_dim,
	unsigned int top_k, int *top_idxs, float *top_scores);
int ifr_face_verify_set_param(ifr_face_verify_t* p_this, int32_t param, void* value);
int ifr_face_verify_get_param(ifr_face_verify_t* p_this, int32_t param, void* value, int32_t size);

#endif //_IFR_FACE_VERIFY_W_H_