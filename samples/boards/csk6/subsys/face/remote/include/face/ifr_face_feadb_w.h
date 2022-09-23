#ifndef _IFR_FACE_FEADB_W_H_
#define _IFR_FACE_FEADB_W_H_

#include <stdint.h>
#include "ifr_types.h"
#include "ifr_defines.h"
#include "ifr_params.h"

typedef struct ifr_face_feadb_t_ ifr_face_feadb_t;

int ifr_face_feadb_alloc(ifr_face_feadb_t** pp_this, int8_t* psram_addr, uint32_t *psram_size, int8_t* share_addr, uint32_t *share_size);
void ifr_face_feadb_destory(ifr_face_feadb_t* p_this);

int ifr_face_feadb_add(ifr_face_feadb_t* p_this, const float* items, unsigned int items_num, int *labels);
int ifr_face_feadb_clear(ifr_face_feadb_t* p_this);
int ifr_face_feadb_search(ifr_face_feadb_t* p_this, const float *query, unsigned int items_num,
	unsigned int top_k, int *top_idxs, float *top_scores);
int ifr_face_feadb_save(ifr_face_feadb_t* p_this, void* res_data, unsigned int *res_size);
int ifr_face_feadb_load(ifr_face_feadb_t* p_this, void* res_data, unsigned int res_size);

int ifr_face_feadb_set_param(ifr_face_feadb_t* p_this, int32_t param, void* value);
int ifr_face_feadb_get_param(ifr_face_feadb_t* p_this, int32_t param, void* value, int32_t size);

#endif //_IFR_FACE_FEADB_W_H_