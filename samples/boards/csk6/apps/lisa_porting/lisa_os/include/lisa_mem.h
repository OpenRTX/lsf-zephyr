#ifndef __LISA_MEM__
#define __LISA_MEM__

#include <stddef.h>
#include <stdint.h>

void *lisa_mem_alloc(uint32_t size);
void *lisa_mem_realloc(void *ptr, uint32_t size);
void *lisa_mem_calloc(uint32_t count, uint32_t size);
void lisa_mem_free(void *ptr);
char *lisa_strdup(const char *s);
void lisa_heap_info(void);

#endif // __LISA_MEM__