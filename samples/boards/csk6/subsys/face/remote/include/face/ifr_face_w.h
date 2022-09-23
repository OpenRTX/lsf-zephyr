#ifndef _IFR_FACE_W_H_
#define _IFR_FACE_W_H_

#include <stdint.h>
#include "ifr_types.h"
#include "ifr_defines.h"

typedef struct ifr_face_t_ ifr_face_t;

//face engine
void ifr_face_getinfo(ifr_face_info_t* info);
int ifr_face_alloc(ifr_face_t** pp_this, ifr_face_param_t* param);
void ifr_face_destory(ifr_face_t* p_this);
int ifr_face_set_param(ifr_face_t* p_this, int32_t param, void* value);
int ifr_face_get_param(ifr_face_t* p_this, int32_t param, void* value, int32_t size);
int ifr_face_recognize(ifr_face_t* p_this, ifr_pixel_format_t fmt, uint8_t *data, uint32_t width, uint32_t height,
	ifr_face_recog_result_t* p_result, uint32_t *p_result_num);
int ifr_face_compare_feature(ifr_face_t* p_this, float *feature1, float *feature2, uint32_t feature_dim, float* score);
int ifr_face_search_featuredb(ifr_face_t* p_this, const float* feature_db, const float* query, unsigned int feature_num, unsigned int feature_dim,
	unsigned int top_k, int *top_idxs, float *top_scores);

#endif //_IFR_FACE_W_H_
