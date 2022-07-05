#ifndef __SEARCH__
#define __SEARCH__

#include "dict.h"
#include <stdlib.h>
#include <lisa_mem.h>

#define RULE_MALLOC(size) lisa_mem_alloc(size)
#define RULE_FREE(ptr) lisa_mem_free(ptr)
#define RULE_REALLOC(ptr, size) lisa_mem_realloc(ptr, size)

int dict_search(const unsigned int in_size, const char* const in_word, char* out, dict_inst_t inst, dictCallback_t cb, void* deccompress_data);

#endif
