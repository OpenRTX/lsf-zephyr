#include <stdio.h>
#include "pdict.h"

/////////////////////////////////////////////////////////////////
// 用户自建词典                                                //
/////////////////////////////////////////////////////////////////

#define  PTEMP_MEM_SIZE			(256)
#define  PDICT_HEAD_MASK		(0xA5A5A5A5)
#define  PDICT_CNEN_MASK		(0xAAAA5555)
#define  PVERSION               "pdict.v1000.1.0.0"

typedef struct _private_dict_head {
	unsigned int dict_head_mask;
	unsigned int head_size;
	unsigned int word_num;
	unsigned int map_size;
	unsigned int dict_size;
}private_dict_head_t;

typedef struct _private_dict_inst {
	char temp_buff[((PTEMP_MEM_SIZE + 3) / 4) * 4];
	private_dict_head_t *dict_head;
	unsigned int word_num;
	unsigned int map_size;
	unsigned int dict_size;
	unsigned int map_addr;
	unsigned int map_sort_mask;
}_private_dict_inst_t;

typedef struct _hash_table {
	unsigned int pos;
	unsigned int len;
	unsigned int hash;
}hash_table_t, *hash_table_p;

typedef struct _hash_mask {
	unsigned int mask;
	hash_table_t hash_table;
}hash_mask_t;

static int get_per_hash(char * src)
{
	int	h, v;
	char *p;

	for (h = 0, p = (char *)src; *p; p++)
	{
		h = 5527 * h + 7 * (*p);
		v = h & 0x0000ffff;
		h ^= v*v;
	}

	return h;
}

static void ApiTableSplitArray(hash_table_t* arr, int low, int high, int *p)
{
	int i, j;
	unsigned int item;
	hash_table_t t;

	item = arr[low].hash;
	i = low;
	j = high;

	while (i < j)
	{
		/*  move from R to L in search of element < item */
		while (arr[j].hash> item)
			j = j - 1;

		/*  move from L to R in search of element > item */
		while (arr[i].hash <= item  &&  i < j)
			i = i + 1;

		if (i < j)
		{
			t = arr[i];
			arr[i] = arr[j];
			arr[j] = t;
		}
	}
	*p = j;
	t = arr[low];
	arr[low] = arr[*p];
	arr[*p] = t;
}

static void ApiTableQuickSort(hash_table_t* arr, int low, int high)
{
	int pos;

	if (low < high)
	{
		ApiTableSplitArray(arr, low, high, &pos);
		ApiTableQuickSort(arr, low, pos - 1);
		ApiTableQuickSort(arr, pos + 1, high);
	}
}

static int MatchInList(unsigned int hash, hash_table_t* hashlist, int list_max)
{
	int iMax = list_max - 1, iMin = 0;

	if (hash<hashlist[iMin].hash || hash>hashlist[iMax].hash)
		return -1;

	do {
		if (hash > hashlist[iMin + (iMax - iMin) / 2].hash)
			iMin = iMin + (iMax - iMin) / 2;
		else
			iMax = iMin + (iMax - iMin) / 2;

	} while ((iMax - iMin)>1);

	if (hash == hashlist[iMax].hash)
	{
		return iMax;
	}
	else if (hash == hashlist[iMin].hash)
	{
		return iMin;
	}
	else
	{
		return -1;
	}
}

static int utf8_cn_check(const char *key)
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
		if (((key[i] >> 6) & 0x03) != 0x02) //根据utf8编码规则 判定是否是10xx xxxx
		{
			return -1;
		}
	}
	return 0;
}

static int get_word_type(const char *key, unsigned int key_len, pditc_type_e *ditc_type)
{
	if (key[key_len - 1] != '\0')
		return -1;

	char key1 = key[0];
	if (key1 >> 7)
	{
		if (strlen(key) > 4 || strlen(key) < 3)//中文字 utf8最多占4字节
		{
			return -1;
		}
		int ret = utf8_cn_check(key);
		if (ret != 0)
		{
			return -1;
		}
		*ditc_type = PDICT_CN_DICT;
	}
	else
	{
#if 0
		if (key_len > 128 || strlen(key) > 128)
		{
			return -1;
		}
#endif
		for (unsigned int i = 0; i < key_len - 1; i++) //key_len - 1 最后一位是'\0'
		{
			if (!(key[i] >= 'A' && key[i] <= 'Z' || key[i] >= 'a' && key[i] <= 'z' || key[i] == '-'))
			{
				//return -1;
			}
		}
		*ditc_type = PDICT_EN_DICT;
	}

	return 0;
}

int privateGetTotalHeadInfo(private_dict_info_t *dict_info, private_dict_head_t dict_head)
{
	dict_info->word_num = dict_head.word_num;
	dict_info->index_size = dict_head.map_size;
	dict_info->dict_size = dict_head.dict_size;

	return 0;
}

pdict_err_e ls_pdict_info(private_dict_info_t *info, getHeadCallback_t callback)
{
	pdict_err_e ret = PDICT_NO_ERR;
	if (info == NULL || callback == NULL)
	{
		return PDICT_NULL_ERR;
	}

	private_dict_info_t *dict_info = info;
	private_dict_head_t dict_head;
	unsigned int dict_head_addr = callback(sizeof(private_dict_head_t));

	memcpy(&dict_head, (const char *)dict_head_addr, sizeof(private_dict_head_t));

	if (dict_head.dict_head_mask != PDICT_HEAD_MASK)
	{
		memset(dict_info, 0, sizeof(private_dict_info_t));
	}
	privateGetTotalHeadInfo(dict_info, dict_head);
	dict_info->head_addr = dict_head_addr;
	dict_info->head_size = sizeof(private_dict_head_t);
	dict_info->inst_size = sizeof(_private_dict_inst_t);
	memcpy(dict_info->version, PVERSION, strlen(PVERSION) + 1);

	return ret;
}

pdict_err_e ls_pdict_init(private_dict_inst_t inst, unsigned int head_addr, unsigned int hash_table, setHeadCallback_t callback)
{
	pdict_err_e ret = PDICT_NO_ERR;
	if (inst == NULL)
	{
		return PDICT_NULL_ERR;
	}

	unsigned int dict_map = 0;
	_private_dict_inst_t *dict_inst = inst;
	private_dict_head_t *dict_head = (private_dict_head_t *)head_addr;
	if (dict_head->dict_head_mask != PDICT_HEAD_MASK)
	{
		memset(dict_head, 0, sizeof(private_dict_head_t));
		dict_head->head_size = sizeof(private_dict_head_t);
		dict_head->dict_head_mask = PDICT_HEAD_MASK;
		//回调抛出 外部首次保存头信息
		callback((char *)dict_head, sizeof(private_dict_head_t));
	}
	dict_inst->map_sort_mask = 0;
	if (dict_head->word_num > 0)
	{
		dict_map = hash_table + 4; //前4字节mask
		//初始化对hash表 按hash值大小进行重拍，二分查找
		ApiTableQuickSort((hash_table_t *)dict_map, 0, dict_head->word_num - 1);
		dict_inst->map_sort_mask = 1;
	}
	dict_inst->word_num = dict_head->word_num;
	dict_inst->dict_head = dict_head;
	dict_inst->map_addr = hash_table;

	return ret;
}

pdict_err_e ls_pdict_search(private_dict_inst_t inst, private_dict_param_t *dict_param)
{
	if (inst == NULL)
	{
		return PDICT_NULL_ERR;
	}

	pdict_err_e ret = PDICT_NO_ERR;
	pditc_type_e ditc_type;
	_private_dict_inst_t *dict_inst = inst;
	if (dict_inst->dict_head->dict_head_mask != PDICT_HEAD_MASK)
	{
		return PDICT_HEAD_ERR;
	}

	char *temp_buff = dict_inst->temp_buff;
	const char *key = dict_param->key;
	unsigned int key_len = dict_param->key_len;
	unsigned int *value_pos = dict_param->value_pos;
	unsigned int *value_len = dict_param->value_len;
	pditc_type_e* ditc_type_ = dict_param->ditc_type;
	if (key == NULL || value_pos == NULL || value_len == NULL)
	{
		return PDICT_NULL_ERR;
	}

	*value_pos = 0;
	*value_len = 0;
	int ret_ = get_word_type(key, key_len, &ditc_type);
	if (ret_)
	{
		return PDICT_KEY_ERR;
	}

	int index = 0;
	int count = 0;
	unsigned int table_addr = 0;
	unsigned int hashvalue = get_per_hash((char *)key);

	if (ditc_type == PDICT_CN_DICT || ditc_type == PDICT_EN_DICT)
	{
		unsigned int *mask = (unsigned int *)dict_inst->map_addr;
		if (*mask != PDICT_CNEN_MASK)
		{
			return PDICT_MAP_ERR;
		}
		table_addr = dict_inst->map_addr + 4; //前4字节标记信息
		count = dict_inst->dict_head->word_num;
	}
	else
	{
		return PDICT_KEY_ERR;
	}

	if (count > 0) //自建词典中没有单词
	{
		hash_table_t *hash_table = (hash_table_t *)table_addr;
		if (dict_inst->map_sort_mask == 0)
		{
			ApiTableQuickSort((hash_table_t *)hash_table, 0, dict_inst->dict_head->word_num - 1);
			dict_inst->map_sort_mask = 1;
		}
		index = MatchInList(hashvalue, hash_table, count);
		if (index >= 0)
		{
			*value_pos = hash_table[index].pos;
			*value_len = hash_table[index].len;
			*ditc_type_ = ditc_type;
		}
		else
		{
			return  PDICT_NOT_FOUND;
		}
	}
	else
	{
		return  PDICT_NOT_FOUND;
	}
	return ret;
}

pdict_err_e ls_pdict_add(private_dict_inst_t inst, const char *key, unsigned int value_pos, unsigned int value_len
	, setHeadCallback_t hcallback, setIndexCallback_t icallback)
{
	if (inst == NULL || key == NULL)
	{
		return PDICT_NULL_ERR;
	}

	pdict_err_e ditc_type;
	int ret_ = get_word_type(key, strlen(key) + 1, &ditc_type);
	if (ret_)
	{
		return PDICT_KEY_ERR;
	}

	pdict_err_e ret = PDICT_NO_ERR;
	_private_dict_inst_t *dict_inst = inst;

	hash_table_t hash_table;
	int hash_value = get_per_hash((char *)key);
	
	hash_table.hash = hash_value;
	hash_table.len = value_len;
	hash_table.pos = value_pos;

	//update hash table
	if (dict_inst->dict_head->word_num == 0)
	{
		hash_mask_t hash_mask;
		hash_mask.mask = PDICT_CNEN_MASK;
		hash_mask.hash_table = hash_table;

		//回调 外部首次保存hash表 和标记
		icallback(dict_inst->dict_head->head_size, dict_inst->dict_head->map_size,
			(char *)&hash_mask, sizeof(hash_mask_t));

		dict_inst->dict_head->map_size += sizeof(hash_mask_t);
	}
	else
	{
		unsigned int count = dict_inst->dict_head->word_num;
		unsigned int table_addr = dict_inst->map_addr + 4;
		hash_table_t *hash_table_p = (hash_table_t *)table_addr;
		int index = MatchInList(hash_value, hash_table_p, dict_inst->dict_head->word_num);
		if (index >= 0) //保存的单词已存在
		{
			printf("%s\n",key);
			//return PDICT_ADDREP_ERR;
		}
		//回调 外部追加保存hash表
		icallback(dict_inst->dict_head->head_size, dict_inst->dict_head->map_size, 
			(char *)&hash_table, sizeof(hash_table_t));

		dict_inst->dict_head->map_size += sizeof(hash_table_t);
		if (dict_inst->map_sort_mask == 1)
		{
			//ApiTableQuickSort((hash_table_t *)hash_table, 0, dict_inst->dict_head->word_num - 1);
			dict_inst->map_sort_mask = 0;
		}
	}

	//update dict head
	if (ditc_type == PDICT_CN_DICT || ditc_type == PDICT_EN_DICT)
	{
		dict_inst->dict_head->dict_size += value_len;
		//dict_inst->dict_head->map_size += sizeof(hash_table_t);
		dict_inst->dict_head->word_num++;
	}
	else
	{
		return PDICT_KEY_ERR;
	}
	dict_inst->dict_head->dict_head_mask = 0xA5A5A5A5;
	//回调 外部更新头信息
	hcallback((char *)dict_inst->dict_head, sizeof(private_dict_head_t));

	return ret;
}