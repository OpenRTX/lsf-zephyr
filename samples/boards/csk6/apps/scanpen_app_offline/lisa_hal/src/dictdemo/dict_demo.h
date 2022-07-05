#ifndef _DICT_DEMO_
#define _DICT_DEMO_

#include "dict.h"

int dict_init(dict_type_e type);

void dict_test(const char* const in_word, char* out, char* gif_out, int* gif_len);

int rule_dict_test(int in_size, const char* const in_word, char* out);

void dict_deinit();

char* get_word_type(const char* key, unsigned int key_len, dict_type_e* dict_type);

#endif //_DICT_DEMO_
