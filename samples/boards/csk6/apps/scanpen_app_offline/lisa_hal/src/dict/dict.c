#include <stdio.h>
#include "b+tree_org.h"
#include "dict.h"

// #define  LS_DICT_ENCRYPT		0

#ifdef LS_DICT_ENCRYPT
#include "rsa_gen.h"
#include "rsa_encrypt.h"
#include "rsa_decrypt.h"
#endif

#define DICT_RTOS

#ifdef DICT_RTOS
#define  DICT_VALUE_SIZE		(4096)  
#else
#define  DICT_VALUE_SIZE		(200 * 1024)  
#endif

#define  DICT_BLOCK_SIZE		(64 * 1024)  
#define  DICT_HEAD_MASK			(0xAAAAAAAA)

#define  TEMP_MEM_SIZE			(4096 + DICT_VALUE_SIZE)

#define  VERSION                "dict.v1000.3.0.3"
#define  CN_STRUCTURE           "word|strokes|structure|radical|mean|"
#define  EN_STRUCTURE           "word|phonetic|mean|derive|tenses|phrase|"

#define  IDIOM_STRUCTURE        "word|spell|explanation|usage|correct|level|num|emotional|recognition|structure|riddle|reference|synonym|antonym|english_translation|discriminate|klondike|age|"
#define  POEM_STRUCTURE			"name|author|chaodai|cnt|type|fanyi_cnt|annotation|shangxi_cnt|background|"

#define  CN_FUZZY_QUERY			0
#define  CN_SOCRE_THRESHOLD		(0.75)

#define  EN_FUZZY_QUERY			1
#define  EN_SOCRE_THRESHOLD		(0.75)

/* field code */
typedef enum {
	FIELD_WORD,
	FIELD_PHONETIC,
	FIELD_TRANS,
	FIELD_DERIVE,
	FIELD_PHRASE,
} en2ch_field_e;

/* function code */
typedef enum {
	DICTGET_EN_ALL,
	DICTGET_CN_ALL,
	DICTGET_IDIOM_ALL,
	DICTGET_POEM_ALL,
	DICTGET_GIF_ALL,

} dict_fc_e;

typedef struct dict_head_ {
	unsigned int dict_head_mask;

	unsigned int dict_res;
	unsigned int dict_extract;
	unsigned int word_num;
	unsigned int map_offset;
	unsigned int head_offset;
	unsigned int dict_offset;
	unsigned int root_offset;
	unsigned int dict_size;

}dict_head_t;



/* Dictionary information */
typedef struct dict_inst_ {
	char temp_buff[((TEMP_MEM_SIZE + 3) / 4) * 4];
	//dict_head_t *dict_head;
	//unsigned int dict_res_addr;

	void* cn_res_addr;
	unsigned int cn_res_size;
	void* en_res_addr;
	unsigned int en_res_size;
	void* idiom_res_addr;
	unsigned int idiom_res_size;
	void* poem_res_addr;
	unsigned int poem_res_size;
	void* gif_res_addr;
	unsigned int gif_res_size;

	unsigned int cn_word_num;
	unsigned int cn_dict_extract;
	void* cn_dict_addr;
	void* cn_map_addr;
	void* cn_head_addr;
	unsigned int cn_root_offset;

	unsigned int en_word_num;
	unsigned int en_dict_extract;
	void* en_dict_addr;
	void* en_map_addr;
	void* en_head_addr;
	unsigned int en_root_offset;

	unsigned int idiom_word_num;
	unsigned int idiom_dict_extract;
	void* idiom_dict_addr;
	void* idiom_map_addr;
	void* idiom_head_addr;
	unsigned int idiom_root_offset;

	unsigned int poem_word_num;
	unsigned int poem_dict_extract;
	void* poem_dict_addr;
	void* poem_map_addr;
	void* poem_head_addr;
	unsigned int poem_root_offset;

	unsigned int gif_word_num;
	unsigned int gif_dict_extract;
	void* gif_dict_addr;
	void* gif_map_addr;
	void* gif_head_addr;
	unsigned int gif_root_offset;

}_dict_inst_t;


dict_err_e pos_char(char* result, char t, int order, short* pos, short* len)
{
	dict_err_e ret = CD_NO_ERR;
	if (result == NULL)
	{
		return CD_NULL_ERR;
	}
	int tpos = 0;
	int torder = 0;
	unsigned int i = 0;
	for (i = 0; i < strlen(result); i++)
	{
		if (result[i] == t)
		{
			torder++;
			if (torder == order)
			{
				*pos = i;
			}
			else if (torder == order + 1)
			{
				*len = i - *pos - 1;
				break;
			}
		}
		tpos++;
	}
	if (i == strlen(result))
	{
		//not found
		return CD_NOT_FOUND;
	}

	return ret;
}

dict_err_e split_result(char* result, en2ch_field_e field, char* value)
{
	dict_err_e ret = CD_NO_ERR;
	if (result == NULL || value == NULL)
	{
		return CD_NULL_ERR;
	}
	short pos = 0;
	short len = 0;

	ret = pos_char(result, '|', field, &pos, &len);
	if (len == 0)
	{
		return CD_FIELD_NULL;
	}
	memcpy(value, result + pos + 1, len);

	return ret;
}

static int dict_min(int a, int b, int c)
{
	int min = a;
	if (min > b)
		min = b;
	if (min > c)
		min = c;
	return min;
}

int dif[30][30] = { 0 };
static float levenshtein(const char* str1, const char* str2)
{
	if (str1 == NULL || str2 == NULL)
	{
		return 0;
	}

	int len1 = strlen(str1);
	int len2 = strlen(str2);

	//int dif[30][30] = { 0 };
	//
	for (int a = 0; a <= len1; a++) {
		dif[a][0] = a;
	}
	for (int a = 0; a <= len2; a++) {
		dif[0][a] = a;
	}
	//
	int temp;
	for (int i = 1; i <= len1; i++) {
		for (int j = 1; j <= len2; j++) {
			if (str1[i - 1] == str2[j - 1]) {
				temp = 0;
			}
			else {
				temp = 1;
			}
			//
			dif[i][j] = dict_min(dif[i - 1][j - 1] + temp, dif[i][j - 1] + 1,
				dif[i - 1][j] + 1);
		}
	}

	//
	float temp_f = dif[len1][len2] / 1.0;
	float similarity = 1 - temp_f / (len1 > len2 ? len1 : len2);
	return similarity;
}

static int traverse_node_key(const char* key, mem_info* add_info, node* temp_leaf, node* root, float* score)
{
	if (key == NULL || add_info == NULL || temp_leaf == NULL || root == NULL)
	{
		return -1;
	}

	int ret = -1;

	int* keys = NULL;
	char* score_key = NULL;
	char* addr = add_info->org_addr;

	float score_max = 0;
	float similarity[DEFAULT_ORDER] = { 0 };

	keys = (int*)FSAT_ADDR(addr, temp_leaf->keys);

	for (int i = 0; i < temp_leaf->num_keys; i++)
	{
		similarity[i] = levenshtein(key, (char*)FSAT_ADDR(addr, keys[i]));
		//printf("[%s]---%.5f---[%s]\n", key, similarity[i], (char *)FSAT_ADDR(addr, keys[i]));
		if (score_max < similarity[i])
		{
			score_max = similarity[i];
			score_key = (char*)FSAT_ADDR(addr, keys[i]);
		}
	}
	if (score_max >= EN_SOCRE_THRESHOLD)
	{
		*score = score_max;
		return find(add_info, root, score_key, false, NULL);
	}

	return ret;
}

static int traverse_parent_node(const char* key, mem_info* add_info, node* leaf, node* root, float* score)
{
	if (key == NULL || add_info == NULL || leaf == NULL || root == NULL)
	{
		return -1;
	}
	int ret = -1;

	char* addr = add_info->org_addr;
	node* temp_leaf = leaf;
	int record_offset = 0;

	while (temp_leaf != root)
	{
		//PRINT("----------Traverse parent node---------- \n");
		record_offset = traverse_node_key(key, add_info, temp_leaf, root, score);
		if (record_offset >= 0)
			return record_offset;

		temp_leaf = (node*)FSAT_ADDR(addr, temp_leaf->parent);
	}

	//traverse root node
	record_offset = traverse_node_key(key, add_info, root, root, score);
	if (record_offset >= 0)
		return record_offset;

	return ret;
}

static int fuzzy_query(const char* key, mem_info* add_info, node* leaf, node* root, float* score)
{
	if (key == NULL || add_info == NULL || leaf == NULL || root == NULL)
	{
		return -1;
	}

	int ret = -1;
	char* addr = add_info->org_addr;

	int record_offset = traverse_node_key(key, add_info, leaf, root, score);
	if (record_offset >= 0)
		return record_offset;
	else
	{
		//find right leaf
		int i, index = 0;
		node* parent = (node*)FSAT_ADDR(addr, leaf->parent);
		int* pointers = (int*)FSAT_ADDR(addr, parent->pointers);
		//PRINT("parent leaf num: %d \n", parent->num_keys);

		for (i = 0; i < parent->num_keys; i++)
		{
			if (leaf == (node*)FSAT_ADDR(addr, pointers[i]))
			{
				//PRINT("location leaf num: %d \n", i);
				index = i;
				break;
			}
		}

		if (i == parent->num_keys || index == parent->num_keys - 1)
		{
			return traverse_parent_node(key, add_info, parent, root, score);
		}
		//Right node must exist
		node* right_leaf = (node*)FSAT_ADDR(addr, pointers[index + 1]);
		int record_offset = traverse_node_key(key, add_info, right_leaf, root, score);
		if (record_offset >= 0)
			return record_offset;

	}
	return ret;
}

record* cn_dict_search(_dict_inst_t* inst, unsigned int root_offset, const char* key, float* score)
{
	node* root = NULL;
	node* leaf = NULL;
	record* record_pointer = NULL;
	_dict_inst_t* dict_inst = inst;
	if (!dict_inst->cn_res_addr) {
		return NULL;
	}
	mem_info cn_add_info;

	int record_offset = 0;

	char* cn_addr = (char*)dict_inst->cn_map_addr;

	float similarity[DEFAULT_ORDER] = { 0 };
	cn_add_info.org_addr = cn_addr;
	cn_add_info.addr = 0;
	cn_add_info.size = 0;

	root = (node*)FSAT_ADDR(cn_addr, root_offset);
	//print_tree(&add_info, root);
	//find_and_print(&cn_add_info, root, key, 1);
	record_offset = find(&cn_add_info, root, key, false, &leaf);

	if (record_offset == -1)
	{
#if CN_FUZZY_QUERY
		int offset = fuzzy_query(key, &cn_add_info, leaf, root);
		if (offset >= 0)
		{
			record_pointer = (node*)FSAT_ADDR(cn_addr, offset);
			return record_pointer;
		}
		else
		{
			return NULL;
		}

#else
		return NULL;
#endif
	}
	else
	{
		*score = 1.0;
		record_pointer = (record*)FSAT_ADDR(cn_addr, record_offset);
		return record_pointer;
	}
}

record* idiom_dict_search(_dict_inst_t* inst, unsigned int root_offset, const char* key, float* score)
{
	node* root = NULL;
	node* leaf = NULL;
	record* record_pointer = NULL;
	_dict_inst_t* dict_inst = inst;
	if (!dict_inst->idiom_res_addr) {
		return NULL;
	}
	mem_info idiom_add_info;

	int record_offset = 0;

	char* idiom_addr = (char*)dict_inst->idiom_map_addr;

	float similarity[DEFAULT_ORDER] = { 0 };
	idiom_add_info.org_addr = idiom_addr;
	idiom_add_info.addr = 0;
	idiom_add_info.size = 0;

	root = (node*)FSAT_ADDR(idiom_addr, root_offset);
	//print_tree(&add_info, root);
	//find_and_print(&cn_add_info, root, key, 1);

	record_offset = find(&idiom_add_info, root, key, false, &leaf);

	if (record_offset == -1)
	{
#if CN_FUZZY_QUERY
		int offset = fuzzy_query(key, &cn_add_info, leaf, root);
		if (offset >= 0)
		{
			record_pointer = (node*)FSAT_ADDR(cn_addr, offset);
			return record_pointer;
		}
		else
		{
			return NULL;
		}

#else
		return NULL;
#endif
	}
	else
	{
		*score = 1.0;
		record_pointer = (record*)FSAT_ADDR(idiom_addr, record_offset);
		return record_pointer;
	}
}

record* poem_dict_search(_dict_inst_t* inst, unsigned int root_offset, const char* key, float* score)
{
	node* root = NULL;
	node* leaf = NULL;
	record* record_pointer = NULL;
	_dict_inst_t* dict_inst = inst;
	if (!dict_inst->poem_res_addr) {
		return NULL;
	}
	mem_info poem_add_info;

	int record_offset = 0;

	char* poem_addr = (char*)dict_inst->poem_map_addr;

	float similarity[DEFAULT_ORDER] = { 0 };
	poem_add_info.org_addr = poem_addr;
	poem_add_info.addr = 0;
	poem_add_info.size = 0;

	root = (node*)FSAT_ADDR(poem_addr, root_offset);
	//print_tree(&add_info, root);
	//find_and_print(&cn_add_info, root, key, 1);

	record_offset = find(&poem_add_info, root, key, false, &leaf);

	if (record_offset == -1)
	{
#if CN_FUZZY_QUERY
		int offset = fuzzy_query(key, &cn_add_info, leaf, root);
		if (offset >= 0)
		{
			record_pointer = (node*)FSAT_ADDR(cn_addr, offset);
			return record_pointer;
		}
		else
		{
			return NULL;
		}

#else
		return NULL;
#endif
	}
	else
	{
		*score = 1.0;
		record_pointer = (record*)FSAT_ADDR(poem_addr, record_offset);
		return record_pointer;
	}
}

record* gif_dict_search(_dict_inst_t* inst, unsigned int root_offset, const char* key, float* score)
{
	node* root = NULL;
	node* leaf = NULL;
	record* record_pointer = NULL;
	_dict_inst_t* dict_inst = inst;
	if (!dict_inst->gif_res_addr) {
		return NULL;
	}
	mem_info gif_add_info;

	int record_offset = 0;

	char* gif_addr = (char*)dict_inst->gif_map_addr;

	float similarity[DEFAULT_ORDER] = { 0 };
	gif_add_info.org_addr = gif_addr;
	gif_add_info.addr = 0;
	gif_add_info.size = 0;

	root = (node*)FSAT_ADDR(gif_addr, root_offset);
	//print_tree(&add_info, root);
	//find_and_print(&cn_add_info, root, key, 1);

	record_offset = find(&gif_add_info, root, key, false, &leaf);

	if (record_offset == -1)
	{
#if CN_FUZZY_QUERY
		int offset = fuzzy_query(key, &cn_add_info, leaf, root);
		if (offset >= 0)
		{
			record_pointer = (node*)FSAT_ADDR(cn_addr, offset);
			return record_pointer;
		}
		else
		{
			return NULL;
		}

#else
		return NULL;
#endif
	}
	else
	{
		*score = 1.0;
		record_pointer = (record*)FSAT_ADDR(gif_addr, record_offset);
		return record_pointer;
	}
}

record* en_dict_search(_dict_inst_t* inst, unsigned int root_offset, const char* key, dict_fq_e fq_mode, float* score)
{
	node* root = NULL;
	node* leaf = NULL;
	record* record_pointer = NULL;
	_dict_inst_t* dict_inst = inst;
	if (!dict_inst->en_res_addr) {
		return NULL;
	}
	mem_info en_add_info;

	char* en_addr = (char*)dict_inst->en_map_addr;
	int record_offset = 0;

	en_add_info.org_addr = en_addr;
	en_add_info.addr = 0;
	en_add_info.size = 0;

	root = (node*)FSAT_ADDR(en_addr, root_offset);
	//print_tree(&add_info, root);
	//find_and_print(&en_add_info, root, key, 1);
	record_offset = find(&en_add_info, root, key, false, &leaf);

	if (record_offset < 0)
	{
		if (fq_mode == FUZZY_QUERY_ENABLE)
		{
			int offset = fuzzy_query(key, &en_add_info, leaf, root, score);
			if (offset >= 0)
			{
				record_pointer = (record*)FSAT_ADDR(en_addr, offset);
				return record_pointer;
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		*score = 1.0;
		record_pointer = (record*)FSAT_ADDR(en_addr, record_offset);
		return record_pointer;
	}
}

char get_block_info(_dict_inst_t* inst, char* temp_buff, dict_type_e dict_type,
	record* record_pointer, unsigned int* block_size, unsigned int* block_addr)
{
	if (record_pointer == NULL || temp_buff == NULL || inst == NULL)
	{
		return -1;
	}
	char span = 1;
	unsigned short block_num = record_pointer->value;
	uint64_t offset = record_pointer->offset;
	unsigned int len = record_pointer->len;
	void* dict_addr = 0;
	void* head_addr = 0;
	unsigned int block_total = 0;
	unsigned int* head_buff = (unsigned int*)temp_buff;

	uint64_t end_offset = offset + len;
	span = (end_offset / DICT_BLOCK_SIZE);
	span += ((end_offset % DICT_BLOCK_SIZE) > 0 ? 1 : 0);

	if (dict_type == CN_DICT)
	{
		dict_addr = inst->cn_dict_addr;
		head_addr = inst->cn_head_addr;
		memcpy(&block_total, (const char*)head_addr, sizeof(int));
		memcpy(head_buff, (const char*)head_addr + 4, sizeof(int) * (block_total));
	}
	else if (dict_type == EN_DICT)
	{
		dict_addr = inst->en_dict_addr;
		head_addr = inst->en_head_addr;
		memcpy(&block_total, (const char*)head_addr, sizeof(int));
		memcpy(head_buff, (const char*)head_addr + 4, sizeof(int) * (block_total));
	}
	else if (dict_type == IDIOM_DICT)
	{
		dict_addr = inst->idiom_dict_addr;
		head_addr = inst->idiom_head_addr;
		memcpy(&block_total, (const char*)head_addr, sizeof(int));
		memcpy(head_buff, (const char*)head_addr + 4, sizeof(int) * (block_total));
	}
	else if (dict_type == POEM_DICT) {
		dict_addr = inst->poem_dict_addr;
		head_addr = inst->poem_head_addr;
		memcpy(&block_total, (const char*)head_addr, sizeof(int));
		memcpy(head_buff, (const char*)head_addr + 4, sizeof(int) * (block_total));
	}
	else if (dict_type == GIF_DICT) {
		span = 1;
		dict_addr = inst->gif_dict_addr;
		head_addr = inst->gif_head_addr;
		memcpy(&block_total, (const char*)head_addr, sizeof(int));
		memcpy(head_buff, (const char*)head_addr + 4, sizeof(int) * (block_total));
	}

	if (block_num >= block_total || (span > 1 && block_num >= block_total - 1))
	{
		return -1;
	}

	if (block_num == 0)
	{
		*block_addr = dict_addr;
		if (span > 1)
		{
			for (int i = 0; i < span; i++) {
				block_size[i] = head_buff[block_num + i] - (i > 0 ? head_buff[block_num + i - 1] : 0);
			}
		}
		else
		{
			block_size[0] = head_buff[block_num];
		}
	}
	else if (block_num > 0)
	{
		*block_addr = head_buff[block_num - 1] + dict_addr;
		if (span > 1)
		{
			for (int i = 0; i < span; i++) {
				block_size[i] = head_buff[block_num + i] - head_buff[block_num + i - 1];
			}
			// block_size[0] = head_buff[block_num] - head_buff[block_num - 1];
			// block_size[1] = head_buff[block_num + 1] - head_buff[block_num];
			//block_size = head_buff[block_num + 1] - head_buff[block_num - 1];
		}
		else
		{
			block_size[0] = head_buff[block_num] - head_buff[block_num - 1];
		}
	}

	//printf("block_num: %d, block_total: %d block_addr: %d block_size: %d, offset : %d\n", 
		//block_num, block_total, *block_addr, *block_size, offset);
	return span;
}

static int utf8_cn_check(const char* key)
{
	char key1 = key[0];

	int bytes = 0;
	int shift = 3;
	while (((key1 >> 4) & (1 << shift)) && shift >= 0)
	{
		shift--;
		bytes++;
	}
	if (bytes != 3 && bytes != 4)
	{
		return -1;
	}

	for (int i = 1; i < bytes; i++)
	{
		if (((key[i] >> 6) & 0x03) != 0x02) //����utf8������� �ж��Ƿ���10xx xxxx
		{
			return -1;
		}
	}
	return 0;
}

static int get_word_type(const char* key, unsigned int key_len, dict_type_e* dict_type)
{
	//if (key[key_len - 1] != '\0')
		//return -1;

	char key1 = key[0];
	if (key1 >> 7)
	{
		if (strlen(key) > 5)//������ʫ��
		{
			*dict_type = IDIOM_DICT;
		}
		else if (strlen(key) == 5 || strlen(key) == 4 || strlen(key) == 3)
		{
			*dict_type = CN_DICT;
		}
		else
		{
			return -1;
		}

		//int ret = utf8_cn_check(key);
		//if(ret != 0)
		//{
			//return -1;
		//}

	}
	else
	{
#if 0
		if (key_len > 128 || strlen(key) > 128)
		{
			return -1;
		}
#endif
		for (unsigned int i = 0; i < key_len - 1; i++) //key_len - 1 ���һλ��'\0'
		{
			if (!(key[i] >= 'A' && key[i] <= 'Z' || key[i] >= 'a' && key[i] <= 'z' || key[i] == '-'))
			{
				//return -1;
			}
		}
		*dict_type = EN_DICT;
	}

	return 0;
}


unsigned int ls_dict_instsize(void)
{
	return sizeof(_dict_inst_t);
}

dict_err_e ls_dict_create(dict_inst_t inst, dict_res_t res_map, headCallback_t headCallback,
	void* encryptcode, int encryptcode_size_byte)
{
#ifdef LS_DICT_ENCRYPT
	if (encryptcode == NULL)
	{
		return CD_CHECK_ERR;
	}
	int check_ret = check_authcode(encryptcode, encryptcode_size_byte);
	if (0 != check_ret)
	{
		return CD_CHECK_ERR;
	}
#endif

	dict_err_e ret = CD_NULL_ERR;

	if (inst == NULL || headCallback == NULL)
	{
		return CD_NULL_ERR;
	}

	dict_head_t dict_head;
	_dict_inst_t* dict_inst = inst;
	memset(inst, 0, sizeof(_dict_inst_t));
	dict_inst->cn_res_addr = NULL;
	dict_inst->en_res_addr = NULL;
	dict_inst->idiom_res_addr = NULL;
	dict_inst->poem_res_addr = NULL;
	dict_inst->gif_res_addr = NULL;
	//dict_inst->dict_head = NULL;

	dict_addr_t cn_dict;
	dict_addr_t en_dict;
	dict_addr_t idiom_dict;
	dict_addr_t poem_dict;
	dict_addr_t gif_dict;

	cn_dict.addr = res_map.cn_dict.addr;
	cn_dict.size = res_map.cn_dict.size;
	en_dict.addr = res_map.en_dict.addr;
	en_dict.size = res_map.en_dict.addr;
	idiom_dict.addr = res_map.idiom_dict.addr;
	idiom_dict.size = res_map.idiom_dict.size;
	poem_dict.addr = res_map.poem_dict.addr;
	poem_dict.size = res_map.poem_dict.size;
	gif_dict.addr = res_map.gif_dict.addr;
	gif_dict.size = res_map.gif_dict.size;

	if (!cn_dict.addr && (unsigned int)cn_dict.addr % 4 != 0) {
		return CD_INIT_FAILED;
	}

	if (!en_dict.addr && (unsigned int)en_dict.addr % 4 != 0) {
		return CD_INIT_FAILED;
	}

	if (idiom_dict.addr && (unsigned int)idiom_dict.addr % 4 != 0) {
		return CD_INIT_FAILED;
	}

	if (poem_dict.addr && (unsigned int)poem_dict.addr % 4 != 0) {
		return CD_INIT_FAILED;
	}

	if (gif_dict.addr && (unsigned int)gif_dict.addr % 4 != 0) {
		return CD_INIT_FAILED;
	}

	void* dict_head_addr = NULL;
	if (cn_dict.addr)
	{
		ret = CD_NO_ERR;
		dict_inst->cn_res_addr = cn_dict.addr;
		dict_inst->cn_res_size = cn_dict.size;
		dict_head_addr = headCallback(dict_inst->cn_res_addr, sizeof(dict_head_t), CN_DICT);
		memset(&dict_head, 0, sizeof(dict_head_t));
		memcpy(&dict_head, (const char*)dict_head_addr, sizeof(dict_head_t));
		if (dict_head.dict_head_mask != DICT_HEAD_MASK)
		{
			return CD_HEAD_ERR;
		}

		dict_inst->cn_map_addr = headCallback((const char*)dict_inst->cn_res_addr + dict_head.map_offset, \
			dict_head.dict_offset - dict_head.map_offset, CN_DICT);

		dict_inst->cn_head_addr = (const char*)dict_inst->cn_map_addr + (dict_head.head_offset - \
			dict_head.map_offset);

		dict_inst->cn_dict_extract = dict_head.dict_extract;
		dict_inst->cn_dict_addr = (const char*)dict_inst->cn_res_addr + dict_head.dict_offset;
		dict_inst->cn_root_offset = dict_head.root_offset;
		dict_inst->cn_word_num = dict_head.word_num;

	}
	if (en_dict.addr)
	{
		ret = CD_NO_ERR;
		dict_inst->en_res_addr = en_dict.addr;
		dict_inst->en_res_size = en_dict.size;
		dict_head_addr = headCallback(dict_inst->en_res_addr, sizeof(dict_head_t), EN_DICT);
		memset(&dict_head, 0, sizeof(dict_head_t));
		memcpy(&dict_head, (const char*)dict_head_addr, sizeof(dict_head_t));
		if (dict_head.dict_head_mask != DICT_HEAD_MASK)
		{
			return CD_HEAD_ERR;
		}

		dict_inst->en_map_addr = headCallback((const char*)dict_inst->en_res_addr + dict_head.map_offset, \
			dict_head.dict_offset - dict_head.map_offset, EN_DICT);

		dict_inst->en_head_addr = (const char*)dict_inst->en_map_addr + (dict_head.head_offset - \
			dict_head.map_offset);

		dict_inst->en_dict_extract = dict_head.dict_extract;
		dict_inst->en_dict_addr = (const char*)dict_inst->en_res_addr + dict_head.dict_offset;
		dict_inst->en_root_offset = dict_head.root_offset;
		dict_inst->en_word_num = dict_head.word_num;
	}
	if (idiom_dict.addr)
	{
		ret = CD_NO_ERR;
		dict_inst->idiom_res_addr = idiom_dict.addr;
		dict_inst->idiom_res_size = idiom_dict.size;
		dict_head_addr = headCallback(dict_inst->idiom_res_addr, sizeof(dict_head_t), IDIOM_DICT);
		memset(&dict_head, 0, sizeof(dict_head_t));
		memcpy(&dict_head, (const char*)dict_head_addr, sizeof(dict_head_t));
		if (dict_head.dict_head_mask != DICT_HEAD_MASK)
		{
			return CD_HEAD_ERR;
		}

		dict_inst->idiom_map_addr = headCallback((const char*)dict_inst->idiom_res_addr + dict_head.map_offset, \
			dict_head.dict_offset - dict_head.map_offset, IDIOM_DICT);

		dict_inst->idiom_head_addr = (const char*)dict_inst->idiom_map_addr + (dict_head.head_offset - \
			dict_head.map_offset);

		dict_inst->idiom_dict_extract = dict_head.dict_extract;
		dict_inst->idiom_dict_addr = (const char*)dict_inst->idiom_res_addr + dict_head.dict_offset;
		dict_inst->idiom_root_offset = dict_head.root_offset;
		dict_inst->idiom_word_num = dict_head.word_num;
	}
	if (poem_dict.addr)
	{
		ret = CD_NO_ERR;
		dict_inst->poem_res_addr = poem_dict.addr;
		dict_inst->poem_res_size = poem_dict.size;
		dict_head_addr = headCallback(dict_inst->poem_res_addr, sizeof(dict_head_t), POEM_DICT);
		memset(&dict_head, 0, sizeof(dict_head_t));
		memcpy(&dict_head, (const char*)dict_head_addr, sizeof(dict_head_t));
		if (dict_head.dict_head_mask != DICT_HEAD_MASK)
		{
			return CD_HEAD_ERR;
		}

		dict_inst->poem_map_addr = headCallback((const char*)dict_inst->poem_res_addr + dict_head.map_offset, \
			dict_head.dict_offset - dict_head.map_offset, POEM_DICT);

		dict_inst->poem_head_addr = (const char*)dict_inst->poem_map_addr + (dict_head.head_offset - \
			dict_head.map_offset);

		dict_inst->poem_dict_extract = dict_head.dict_extract;
		dict_inst->poem_dict_addr = (const char*)dict_inst->poem_res_addr + dict_head.dict_offset;
		dict_inst->poem_root_offset = dict_head.root_offset;
		dict_inst->poem_word_num = dict_head.word_num;
	}
	if (gif_dict.addr)
	{
		ret = CD_NO_ERR;
		dict_inst->gif_res_addr = gif_dict.addr;
		dict_inst->gif_res_size = gif_dict.size;
		dict_head_addr = headCallback(dict_inst->gif_res_addr, sizeof(dict_head_t), GIF_DICT);
		memset(&dict_head, 0, sizeof(dict_head_t));
		memcpy(&dict_head, (const char*)dict_head_addr, sizeof(dict_head_t));
		if (dict_head.dict_head_mask != DICT_HEAD_MASK)
		{
			return CD_HEAD_ERR;
		}

		dict_inst->gif_map_addr = headCallback((const char*)dict_inst->gif_res_addr + dict_head.map_offset, \
			dict_head.dict_offset - dict_head.map_offset, GIF_DICT);

		dict_inst->gif_head_addr = (const char*)dict_inst->gif_map_addr + (dict_head.head_offset - \
			dict_head.map_offset);

		dict_inst->gif_dict_extract = dict_head.dict_extract;
		dict_inst->gif_dict_addr = (const char*)dict_inst->gif_res_addr + dict_head.dict_offset;
		dict_inst->gif_root_offset = dict_head.root_offset;
		dict_inst->gif_word_num = dict_head.word_num;
	}
	return ret;
}

dict_err_e ls_dict_info(dict_inst_t inst, dict_info_t* info, headCallback_t totalHeadCallback)
{
	dict_err_e ret = CD_NO_ERR;
	if (info == NULL || totalHeadCallback == NULL)
	{
		return CD_NULL_ERR;
	}
	_dict_inst_t* dict_inst = inst;

	dict_info_t* dict_info = info;
	if (dict_inst)
	{
		dict_info->cn_word_num = dict_inst->cn_word_num;
		dict_info->en_word_num = dict_inst->en_word_num;
		dict_info->idiom_word_num = dict_inst->idiom_word_num;
		dict_info->poem_word_num = dict_inst->poem_word_num;
		dict_info->gif_word_num = dict_inst->gif_word_num;
	}
	//dict_info->temp_mem_size = TEMP_MEM_SIZE;
	dict_info->block_size = DICT_BLOCK_SIZE;
	memcpy(dict_info->version, VERSION, strlen(VERSION) + 1);
	memcpy(dict_info->cn_structure, CN_STRUCTURE, strlen(CN_STRUCTURE) + 1);
	memcpy(dict_info->en_structure, EN_STRUCTURE, strlen(EN_STRUCTURE) + 1);
	memcpy(dict_info->idiom_structure, IDIOM_STRUCTURE, strlen(IDIOM_STRUCTURE) + 1);
	memcpy(dict_info->poem_structure, POEM_STRUCTURE, strlen(POEM_STRUCTURE) + 1);

	return ret;
}

dict_err_e ls_dict_search(dict_inst_t inst, dict_param_t* dict_param)
{
	dict_err_e ret = CD_NO_ERR;
	if (inst == NULL || dict_param == NULL)
	{
		return CD_NULL_ERR;
	}
#ifdef LINUX_X86
	if (strcmp(dict_param->key, "净几明窗") == 0) {

	} else if (strcmp(dict_param->key, "将进酒") == 0) {

	} else if (strcmp(dict_param->key, "静夜思") == 0) {

	} else if (strcmp(dict_param->key, "春夜喜雨") == 0) {

	} else if (strcmp(dict_param->key, "哀江南赋并序") == 0) {

	} else if (strcmp(dict_param->key, "戏文·蔡伯喈琵琶记") == 0) {

	} else if (strcmp(dict_param->key, "龥") == 0) {

	} else if (strcmp(dict_param->key, "阿") == 0) {

	} else if (strcmp(dict_param->key, "book") == 0) {

	} else if (strcmp(dict_param->key, "the") == 0) {

	} else if (strcmp(dict_param->key, "countries") == 0) {

	} else if (strcmp(dict_param->key, "releasing") == 0) {

	} else {
		return CD_NULL_ERR;
	}
#endif
	_dict_inst_t* dict_inst = inst;

	const char* key = dict_param->key;
	unsigned int key_len = dict_param->key_len;
	char* value = dict_param->value;
	char* gif_value = dict_param->gif_value;
	dict_fq_e fq_mode = dict_param->fq_mode;
	float* score = &dict_param->score;
	unsigned int* value_len = &dict_param->value_len;
	unsigned int* gif_len = &dict_param->gif_len;
	dictCallback_t callback = dict_param->callback;
	if (key == NULL || value == NULL || callback == NULL)
	{
		return CD_NULL_ERR;
	}

	dict_fc_e fc;
	dict_dec_e extract;
	dict_type_e dict_type;
	record* record_pointer = NULL;
	record* gif_record_pointer = NULL;
	char* all_buf = dict_inst->temp_buff;
	char* temp_buff1 = all_buf + DICT_VALUE_SIZE;
	unsigned int root_offset = 0;
	uint64_t record_addr = 0;
	unsigned int block_addr = 0;
	unsigned int gif_block_addr = 0;
	unsigned int block_size[10] = { 0 };
	char spans = 1;

	int ret_ = get_word_type(key, key_len, &dict_type);
	if (ret_)
	{
		return CD_KEY_ERR;
	}

#if 1
	if (dict_type == CN_DICT)
	{
		fc = DICTGET_CN_ALL;
		extract = dict_inst->cn_dict_extract;
		
		record_pointer = cn_dict_search(dict_inst, dict_inst->cn_root_offset, key, score);
		if (record_pointer == NULL)
		{
			return CD_NOT_FOUND;
		}

		spans = get_block_info(dict_inst, temp_buff1, CN_DICT, record_pointer, (unsigned int*)block_size, &block_addr);
		
		gif_record_pointer = gif_dict_search(dict_inst, dict_inst->gif_root_offset, key, score);
		if (gif_record_pointer) {
			unsigned int gif_block_size[10] = { 0 };
			get_block_info(dict_inst, temp_buff1, GIF_DICT, gif_record_pointer, (unsigned int*)gif_block_size, &gif_block_addr);
		}
	}
	else if (dict_type == EN_DICT)
	{
		fc = DICTGET_EN_ALL;
		extract = dict_inst->en_dict_extract;

		record_pointer = en_dict_search(dict_inst, dict_inst->en_root_offset, key, fq_mode, score);
		if (record_pointer == NULL)
		{
			return CD_NOT_FOUND;
		}
		spans = get_block_info(dict_inst, temp_buff1, EN_DICT, record_pointer, (unsigned int*)block_size, &block_addr);
	}
	else if (dict_type == IDIOM_DICT)
	{
		fc = DICTGET_IDIOM_ALL;
		extract = dict_inst->idiom_dict_extract;

		record_pointer = idiom_dict_search(dict_inst, dict_inst->idiom_root_offset, key, score);
		if (record_pointer == NULL)
		{
			dict_type = POEM_DICT;
			fc = DICTGET_POEM_ALL;
			extract = dict_inst->poem_dict_extract;

			record_pointer = poem_dict_search(dict_inst, dict_inst->poem_root_offset, key, score);
			if (record_pointer == NULL) {
				return CD_NOT_FOUND;
			}
			spans = get_block_info(dict_inst, temp_buff1, POEM_DICT, record_pointer, (unsigned int*)block_size, &block_addr);
		}
		else {
			spans = get_block_info(dict_inst, temp_buff1, IDIOM_DICT, record_pointer, (unsigned int*)block_size, &block_addr);
		}
	}
	else
	{

	}
#endif

	//TODO:block ��ΧҲҪ����flash��head���ݽ����ж��Ϸ���
	if (record_pointer->len > DICT_VALUE_SIZE - 1)
	{
		return CD_SIZE_ERR;
	}
	else
	{
		// TODO gif !!!!!!!!!!!!!!!!!!!!
		memset(all_buf, 0, DICT_VALUE_SIZE);
		record_addr = callback((unsigned int*)block_size, block_addr, record_pointer->offset, spans, extract, dict_type);
		memcpy(all_buf, (const char*)record_addr, record_pointer->len - 2); // '\r\n'
		all_buf[record_pointer->len] = '\0';
		
		//PRINT("%s\n", all_buf);
		switch (fc)
		{
		case DICTGET_EN_ALL:
			if (value_len != NULL)
				*value_len = record_pointer->len + 1;
			memcpy(value, all_buf, record_pointer->len + 1);
			memset(all_buf, 0, 4096);

			break;
		case DICTGET_CN_ALL:
			if (value_len != NULL)
				*value_len = record_pointer->len + 1;
			memcpy(value, all_buf, record_pointer->len + 1);
			memset(all_buf, 0, 4096);

			if (gif_record_pointer) {
				memset(all_buf, 0, DICT_VALUE_SIZE);
				record_addr = callback((unsigned int*)block_size, gif_block_addr, gif_record_pointer->offset, 1, dict_inst->gif_dict_extract, GIF_DICT);
				memcpy(all_buf, (const char*)record_addr, gif_record_pointer->len);
				if (gif_len != NULL)
					*gif_len = gif_record_pointer->len;
				memcpy(gif_value, all_buf, gif_record_pointer->len);
				memset(all_buf, 0, 4096);
			}
			break;
		case DICTGET_IDIOM_ALL:
			if (value_len != NULL)
				*value_len = record_pointer->len + 1;
			memcpy(value, all_buf, record_pointer->len + 1);
			memset(all_buf, 0, 4096);
			break;
		case DICTGET_POEM_ALL:
			if (value_len != NULL)
				*value_len = record_pointer->len + 1;
			memcpy(value, all_buf, record_pointer->len + 1);
			memset(all_buf, 0, 4096);
			break;

		default:
			break;
		}

	}
	return ret;
}

dict_err_e ls_dict_get_authcode(void** auth_code, int* len)
{
	dict_err_e ret = CD_NO_ERR;
#ifdef LS_DICT_ENCRYPT	
	if (len == NULL)
	{
		return CD_NULL_ERR;
	}

	get_authcode(auth_code, len);
#endif
	return ret;
}
