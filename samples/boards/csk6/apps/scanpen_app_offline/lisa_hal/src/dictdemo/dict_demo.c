#include "dict_demo.h"
#include "xz/xz.h"
#include "dict_search.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <lisa_mem.h>
#include <fs/fs.h>

#define DICT_CN_ADDR (0x18000000 + 0x1ae0000)
#define DICT_EN_ADDR (0x18000000 + 0x1be0000)

typedef uint8_t *dict_header_t;

#define XZ_DICT_MAX (64 * 1024)  // xz 压缩参数 固定为64K
#define DECOMPRESS_BUFFER_SIZE (64 * 1024)  // 资源包每块解压后大小

char *g_deccompress_data = NULL;

dict_err_e g_dict_ret;
dict_info_t g_info;
dict_inst_t g_inst = NULL;
dict_res_t g_res_map;

// xz解压，xz需要开启crc64校验，不使用bootloader中的xz
int _xz_decompress(
		char *compress_data, uint32_t compress_len, char *decompress_data, uint32_t *decompress_len)
{
	xz_crc32_init();
	xz_crc64_init();
	int ret = -1;
	struct xz_dec *xz_dec_handle = NULL;
	struct xz_buf xz_buf;
	enum xz_ret xz_ret;

	xz_dec_handle = xz_dec_init(XZ_DYNALLOC, XZ_DICT_MAX);
	if (xz_dec_handle == NULL) {
		printk("xz decompress init fail");
		return -1;
	}
	xz_buf.in = (uint8_t *)compress_data;
	xz_buf.in_pos = 0;
	xz_buf.in_size = compress_len;
	xz_buf.out = (uint8_t *)decompress_data;
	xz_buf.out_pos = 0;
	xz_buf.out_size = 64 * 1024;
	int dec_size = 0;
	while (1) {
		xz_ret = xz_dec_run(xz_dec_handle, &xz_buf);
		if (xz_ret == XZ_OK || xz_ret == XZ_STREAM_END) {
			dec_size += xz_buf.out_pos;
			xz_buf.out = (uint8_t *)decompress_data + dec_size;
		}
		if (xz_ret == XZ_OK) {
			xz_buf.out_pos = 0;
		} else if (xz_ret == XZ_STREAM_END) {
			break;
		} else {
			goto XZ_DEC_OUT;
		}
	}

	*decompress_len = dec_size;
	ret = 0;
XZ_DEC_OUT:
	xz_dec_end(xz_dec_handle);
	return ret;
}

// 词典回调，根据参数确定是否需要解压
static uint64_t dictCallback(unsigned int *block_size, unsigned int block_addr, uint64_t offset,
		char spans, dict_dec_e extract, dict_type_e type)
{
	int ret = 0;
	if (extract == NEED_DEC) {  // need decompress
		void *block_buffer = (void *)block_addr;

		if (g_deccompress_data) {
			lisa_mem_free(g_deccompress_data);
		}
		g_deccompress_data = lisa_mem_alloc(DECOMPRESS_BUFFER_SIZE * spans);
		memset(g_deccompress_data, 0, DECOMPRESS_BUFFER_SIZE * spans);

		for (int i = 0; i < spans; i++) {
			block_addr += (i > 0 ? block_size[i - 1] : 0);
			_xz_decompress(block_addr, block_size[i],
					g_deccompress_data + (DECOMPRESS_BUFFER_SIZE * i), &ret);
		}
		return (uint64_t)(g_deccompress_data + offset);
	} else {  // not decompress
		printk("not need decompress\n");
		printk("block_addr: %p, offset: %ld, spans: %d \n", block_addr, offset, spans);
		return block_addr;
	}
	return ret;
}

// 获取资源头信息回调，回调三次，分别获取资源文件头信息，中/英文词典头信息
void *headCallback(unsigned int head_addr, unsigned int head_size, dict_type_e type)
{
	// printk("headCallback\n");
	// printk("head_addr：%d, head_size: %d\n", head_addr, head_size);

	unsigned int ret = 0;

	// //需要搬运到可直接访问内存，返回可用内存地址
	ret = head_addr;
	return ret;
}

static uint32_t dict_offset = 0;

int dict_init(dict_type_e type)
{
	memset(&g_info, 0, sizeof(dict_info_t));

	// dict init
	char *authcode;
	int authcode_len = 0;

	char *encode_data = NULL;
	int encode_len = 0;

	g_res_map.cn_dict.addr = (char*)DICT_CN_ADDR;
	g_res_map.gif_dict.addr = NULL;
	g_res_map.en_dict.addr = (char*)DICT_EN_ADDR;
	g_res_map.idiom_dict.addr = NULL;
	g_res_map.poem_dict.addr = NULL;

	unsigned int instSize = ls_dict_instsize();
	g_inst = lisa_mem_alloc(instSize);
	// printk("instSize = %d\n", instSize);
	g_dict_ret = ls_dict_create(
			g_inst, g_res_map, (headCallback_t)headCallback, encode_data, encode_len);

	if (CD_NO_ERR != g_dict_ret) {
		printk("ls_dict_createerror, ret:%d \n", g_dict_ret);
		return -1;
	}
	// get dict g_info
	ls_dict_info(g_inst, &g_info, headCallback);
	// printk("after decompress block size: %d \n", g_info.block_size);
	printk("version %s \n", g_info.version);

	return 0;
}

void dict_test(const char *const in_word, char *out, char *gif_buf, int *gif_len)
{
	dict_param_t dict_param;
	dict_param.callback = (dictCallback_t)dictCallback;
	dict_param.value = out;
	dict_param.value_len = 0;
	dict_param.key = in_word;
	dict_param.fq_mode = FUZZY_QUERY_DISABLE;
	dict_param.key_len = strlen(in_word) + 1;
	dict_param.gif_value = gif_buf;
	dict_param.gif_len = 0;

	g_dict_ret = ls_dict_search(g_inst, &dict_param);
	if (g_deccompress_data) {
		lisa_mem_free(g_deccompress_data);
		g_deccompress_data = NULL;
	}
	if (g_dict_ret != CD_NO_ERR) {
		printk("Dictionary is temporarily not supported[%s] \n", in_word);
	} else {
		printk("[%s]: \n", in_word);
		printk("%s \n", out);
		*gif_len = dict_param.gif_len;
		printk("dict_param.gif_len = %d\n", dict_param.gif_len);
	}
}

int rule_dict_test(int in_size, const char *const in_word, char *out)
{
	int ret = dict_search(
			in_size, in_word, out, g_inst, (dictCallback_t)dictCallback, g_deccompress_data);
	if (g_deccompress_data) {
		lisa_mem_free(g_deccompress_data);
	}
	g_deccompress_data = NULL;
	return ret;
}

void dict_deinit()
{
	if (g_inst) {
		lisa_mem_free(g_inst);
		g_res_map.cn_dict.addr = NULL;
		g_res_map.gif_dict.addr = NULL;
		g_res_map.en_dict.addr = NULL;
		g_res_map.idiom_dict.addr = NULL;
		g_res_map.poem_dict.addr = NULL;
	}
}

bool is_chinese_text(char *p, int len)
{
	for (int i = 0; i < len; i++) {
		if (*(p + i) & 0x80) {
			// printk("chinese %x\n",*(p+i));
			return true;
		}

		// printk("english %x\n",*(p+i));
	}
	return false;
}

char *get_word_type(const char *key, unsigned int key_len, dict_type_e *dict_type)
{
	if (true == is_chinese_text(key, key_len)) {
		if (strlen(key) == strlen("啊")) {
			*dict_type = CN_DICT;
			return "CN_DICT";
		} else {
			*dict_type = IDIOM_DICT;
			return "IDIOM_DICT | POEM_DICT";
		}

	} else {
		*dict_type = EN_DICT;
		return "EN_DICT";
	}
}
