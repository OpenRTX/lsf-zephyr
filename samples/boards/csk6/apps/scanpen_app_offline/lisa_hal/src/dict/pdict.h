#ifndef __LS_CONSULT_PDICT__
#define __LS_CONSULT_PDICT__

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	PDICT_HEAD,
	PDICT_HASH,
} pditc_set_type_e;

/* error code */
typedef enum {
	PDICT_NO_ERR,
	PDICT_NULL_ERR,
	PDICT_FIELD_NULL,
	PDICT_NOT_FOUND,
	PDICT_INIT_FAILED,
	PDICT_SIZE_ERR,
	PDICT_HEAD_ERR,
	PDICT_KEY_ERR,
	PDICT_MAP_ERR,
	PDICT_ADDREP_ERR,
} pdict_err_e;

/* field code */
typedef enum
{
	PDICT_CN_DICT = 0x10000000,
	PDICT_EN_DICT,
	PDICT_CN_EN_DICT,
	PDICT_NULL_DICT,
} pditc_type_e;

/* Dictionary information */
typedef struct private_dict_info_ {
	unsigned int head_size;				    /*OUT* Dictionary head size */
	unsigned int head_addr;					/*OUT* Dictionary head addr */
	unsigned int inst_size;				    /*OUT* Inst size */
	unsigned int word_num;					/*OUT* Vocabulary */
	unsigned int index_size;				/*OUT* Chinese Dictionary index size */
	unsigned int dict_size;					/*OUT* Chinese Dictionary size */
	char version[32];                       /*OUT* Dictionary version number */
}private_dict_info_t;

typedef struct private_dict_param_ {
	const char *key;						/*IN* Query word must end with '\0' */
	unsigned int key_len;					/*IN* Query word len */
	unsigned int *value_pos;				/*OUT* Query content returns pos */
	unsigned int *value_len;				/*OUT* Query content returns len */
	pditc_type_e *ditc_type;					/*OUT* ditc type */
}private_dict_param_t;

typedef void* private_dict_inst_t;

typedef unsigned int(*getHeadCallback_t)(unsigned int len);
typedef unsigned int(*setHeadCallback_t)(char *date, unsigned int len);
typedef unsigned int(*setIndexCallback_t)(unsigned int head_size, unsigned int index_size, char *date, unsigned int len);

pdict_err_e ls_pdict_info(private_dict_info_t *info, getHeadCallback_t callback);
pdict_err_e ls_pdict_init(private_dict_inst_t inst, unsigned int head_addr, unsigned int hash_table, setHeadCallback_t callback);
pdict_err_e ls_pdict_search(private_dict_inst_t inst, private_dict_param_t *dict_param);
pdict_err_e ls_pdict_add(private_dict_inst_t inst, const char *key, unsigned int value_pos,
	unsigned int value_len, setHeadCallback_t hcallback, setIndexCallback_t icallback);

#ifdef __cplusplus
};
#endif

#endif	/*__LS_CONSULT_PDICT__*/