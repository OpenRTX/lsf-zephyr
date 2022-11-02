#ifndef __RES_STORAGE_H__
#define __RES_STORAGE_H__

typedef struct {          
	uint32_t inner_ram_size;
} res_storage_cgf_t;

int res_storage_init(void);

int res_storage_config(res_storage_cgf_t *cfg);

int res_storage_copy(void *src, void *dst, uint32_t size);

int res_storage_write(void *src, void *dst, uint32_t size);

#endif  /* __RES_STORAGE_H__ */