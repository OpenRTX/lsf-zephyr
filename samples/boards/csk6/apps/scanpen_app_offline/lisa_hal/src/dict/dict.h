#ifndef __LS_CONSULT_DICT__
#define __LS_CONSULT_DICT__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* decompress code */
typedef enum {
	FUZZY_QUERY_ENABLE,
	FUZZY_QUERY_DISABLE,
} dict_fq_e;

/* decompress code */
typedef enum {
	NOT_DEC,
	NEED_DEC,
} dict_dec_e;

/* error code */
typedef enum {
	CD_NO_ERR,
	CD_NULL_ERR,
	CD_FIELD_NULL,
	CD_NOT_FOUND,
	CD_INIT_FAILED,
	CD_SIZE_ERR,
	CD_HEAD_ERR,
	CD_KEY_ERR,
	CD_MAP_ERR,
	CD_CHECK_ERR,
} dict_err_e;

/* field code */
typedef enum
{
	CN_DICT = 0x10000000,
	EN_DICT,
	IDIOM_DICT,
	POEM_DICT,
	GIF_DICT,
	NULL_DICT,
} dict_type_e;

typedef void* dict_inst_t;

typedef void *(*headCallback_t)(void *from_addr, unsigned int len, dict_type_e type);
typedef uint64_t(*dictCallback_t)(unsigned int *block_size, unsigned int block_addr, 
	uint64_t offset, char spans, dict_dec_e extract, dict_type_e type);

/* structure for memory */
typedef struct dict_map_ {
	void *addr;
	unsigned int size;
}dict_addr_t;

typedef struct dict_res_ {
	dict_addr_t cn_dict;
	dict_addr_t en_dict;
	dict_addr_t idiom_dict;
	dict_addr_t poem_dict;
	dict_addr_t gif_dict;
}dict_res_t;

/* Dictionary information */
typedef struct dict_info_ {
	dict_type_e dict_type;					/*OUT* dict type */
	unsigned int cn_word_num;				/*OUT* Vocabulary */
	unsigned int en_word_num;				/*OUT* Vocabulary */
	unsigned int idiom_word_num;			/*OUT* Vocabulary */
	unsigned int poem_word_num;				/*OUT* Vocabulary */
	unsigned int gif_word_num;				/*OUT* Vocabulary */
	unsigned int block_size;				/*OUT* after decompress the block size */
	char version[32];                       /*OUT* Dictionary version number */
	char cn_structure[48];                  /*OUT* Structure of each word in the dictionary */
	char en_structure[48];                  /*OUT* Structure of each word in the dictionary */
	char idiom_structure[256];               /*OUT* Structure of each word in the dictionary */
	char poem_structure[256];               /*OUT* Structure of each word in the dictionary */
}dict_info_t;

typedef struct dict_param_ {
	const char *key;						/*IN* Query word must end with '\0' */
	unsigned int key_len;					/*IN* Query word len */
	char *value;							/*OUT* Query content returns buff */
	unsigned int value_len;					/*OUT* Query content returns len */
	char *gif_value;						/*OUT* Query gif returns buff */
	unsigned int gif_len;					/*OUT* Query gif returns buff */
	dict_fq_e fq_mode;						/*IN* Fuzzy query mode */
	float score;							/*OUT* Fuzzy query score */
	dictCallback_t callback;				/*IN* decompress callback function */
}dict_param_t;

unsigned int ls_dict_instsize(void);
dict_err_e ls_dict_get_authcode(void **auth_code, int *len);
dict_err_e ls_dict_create(dict_inst_t inst, dict_res_t res_map, headCallback_t headCallback, void* encryptcode, int encryptcode_size_byte);
dict_err_e ls_dict_info(dict_inst_t inst,dict_info_t *info, headCallback_t headCallback);
dict_err_e ls_dict_search(dict_inst_t inst, dict_param_t *dict_param);


#ifdef __cplusplus
};
#endif

#endif	/*__LS_CONSULT_DICT__*/
