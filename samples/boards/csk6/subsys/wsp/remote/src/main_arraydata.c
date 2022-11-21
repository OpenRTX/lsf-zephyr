#include <stdio.h>
#include <string.h>
#include "csp_defines.h"
#include "param_module.h"
#include "ls_csp.h"
#include "opi_psram_cpy.h"
#include "error_code.h"
#include "wsp_error_code.h"
#include "wsp_defines.h"
#include "param_module.h"
#include "ls_wsp.h"
// #define SHAREMEM_ADDR		(0x5FE00000)
// #define SHAREMEM_SIZE		(640*1024)
// #define ENCODER_ADDR        (0x68100000)
// #define DECODER_ADDR        (0x68500000)
// #define ENCODER_SIZE        (3582464)
// #define DECODER_SIZE        (1649616)

// #define SHAREMEM_ADDR		(0x5FE00000)
// #define SHAREMEM_SIZE		(640*1024)
// #define ENCODER_ADDR        (0x68000000)
// #define DECODER_ADDR        (0x68400000)
// #define DICT_ADDR			(0x68500000)
// #define WAV_ADDR			(0x68600000)

// #define ENCODER_SIZE        (3590768)
// #define DECODER_SIZE        (283812)

#define SHAREMEM_ADDR		(0x5FE00000)
#define SHAREMEM_SIZE		(640*1024)
#define ENCODER_ADDR        (0x68850000)
#define DECODER_ADDR        (0x68C50000)
#define DICT_ADDR			(0x68500000)
#define WAV_ADDR			(0x68D00000)

#define ENCODER_SIZE        (3590768)
#define DECODER_SIZE        (283812)
#define CHECK_FUNC_IF_SUCCESS(exp,func) if(exp!=LS_CSP_SUCCESS)\
{\
	printk("%s Failed! ret = %d\n",#func, exp);\
	return -1;\
}

///////////////////////////////////
#define AUDIO_CHANNEL 		(1)
#define IVW_ONCE_FRAME_SIZE (1)
//////////////////////////////////
//#define IVW_ONCE_FRAME_SIZE 16
#define max_buf	(320 * IVW_ONCE_FRAME_SIZE)

#include "phone_dict.h"
//#include "encoder_model.h"
//#include "decoder_model.h"
#include "apple.h"

// static int32_t init_res_mgr(PT_csp_res p_res_mgr, const void* mlp_encoder_res, int size_encoder, const void* mlp_decoder_res,  int size_decoder,const void* phone_dict_res)
// {
// 	int32_t ret = LS_CSP_SUCCESS;

// 	if(mlp_encoder_res == NULL || mlp_decoder_res == NULL || size_encoder == 0 || phone_dict_res == NULL || size_decoder == 0)
// 	{
// 		return LS_CSP_ERROR_INVALID_PARA_VALUE;
// 	}

// 	p_res_mgr->res_buf_arr_[CSP_INDEX_MLP_ENCODER]        = (char*)mlp_encoder_res;
// 	p_res_mgr->res_size_arr_[CSP_INDEX_MLP_ENCODER]       = size_encoder;
// 	p_res_mgr->res_buf_arr_[CSP_INDEX_MLP_DECODER]        = (char*)mlp_decoder_res;
// 	p_res_mgr->res_size_arr_[CSP_INDEX_MLP_DECODER]       = size_decoder;
// 	p_res_mgr->res_buf_arr_[CSP_INDEX_PHONE_DICT] 		  = (char*)phone_dict_res;

// 	return ret;
// }

//typedef char bool;
// #define true 1
// #define false 0
// #define NONE -1
// #define CHANNEL0 0
// #define CHANNEL1 1

// static int64_t csp_inst_psram[250 * 1024 / 8];
// static int64_t csp_res_psram[5 *1024 * 1024 / 8];
// static char * csp_rlt[1024] = {0};
// static int16_t tempPcmBuf[max_buf * AUDIO_CHANNEL/2] = {0};
// static T_csp_res res_mgr;

// int csp_test(void)
// {
// 	LS_CSP_HANDLE h_csp_inst = NULL;
// 	int nFileSize[AUDIO_CHANNEL] = { 0 };
// 	char *pBuf[AUDIO_CHANNEL] = {NULL};
	
// 	ivInt ret;
// 	int i, j;
// 	int inst_size;
// 	int mem_pool_size;
// 	int audio_jump_size = 0;
// 	ivInt value_param = 0;

// 	unsigned long long cnt_each = 0, cnt_total = 0;

// 	char * encoder_psram_addr =	csp_res_psram;
// 	char * decoder_psram_addr =	(char *)csp_res_psram + (ENCODER_SIZE + 31) / 32 * 32;

// 	memset(csp_inst_psram, 0, sizeof(csp_inst_psram));
// 	memcpy(encoder_psram_addr, ENCODER_ADDR, ENCODER_SIZE);
// 	memcpy(decoder_psram_addr, DECODER_ADDR, DECODER_SIZE);
// 	// init resource mgr
// 	init_res_mgr(&res_mgr, encoder_psram_addr, ENCODER_SIZE, decoder_psram_addr, DECODER_SIZE, phone_dict_resource);

// 	// get callsp engine infomation
// 	T_csp_info t_csp_info;
// 	T_csp_init_param t_csp_init_param;

// 	ret = ls_csp_get_info(&t_csp_info, &res_mgr);
// 	// CHECK_FUNC_IF_SUCCESS(ret, ls_csp_get_info);
// 	mem_pool_size = t_csp_info.share_mem_size;

// 	//init callsp engine inst
// 	t_csp_init_param.inst_mem_addr = (char *)csp_inst_psram;
// 	t_csp_init_param.inst_mem_size = t_csp_info.inst_mem_size;
// 	t_csp_init_param.res = res_mgr;

// 	inst_size = t_csp_info.inst_mem_size;
// 	printk("inst_size: %d \n\r", inst_size);
// 	h_csp_inst = NULL;

// 	printk("mem_pool_size: %d \n\r", mem_pool_size);
// 	t_csp_init_param.share_mem_addr = (char *)(SHAREMEM_ADDR);
// 	t_csp_init_param.share_mem_size = t_csp_info.share_mem_size;

// 	printk("version: %s \n\r", t_csp_info.version);

// 	ret = ls_csp_create(&h_csp_inst, &t_csp_init_param);
// 	// CHECK_FUNC_IF_SUCCESS(ret, ls_csp_create);

// 	nFileSize[0] = sizeof(apple_resource);
// 	pBuf[0] = (char *)apple_resource;
// 	GPIO_SetDir(GPIOA(), (1 << 8), 1);
// 	for (i = 1;i < AUDIO_CHANNEL;i++)
// 	{
// 		if (nFileSize[i] != nFileSize[0])
// 		{
// 			printk("wav data size not equal between 0 and %d channel!\n", i);
// 			return -1;
// 		}
// 	}
	
// 	int run_count = 0;
// 	for(run_count= 0; run_count < 5; run_count++)
// 	{
// 		GPIO_PinWrite(GPIOA(), (1 << 8), 0);
// 		//SetParameter
// 		ret |= ls_csp_set_parameter(h_csp_inst, PARAM_LS_FEA_USE_MVA, 1);
// 		ret |= ls_csp_set_parameter(h_csp_inst, PARAM_LS_DEC_PHONE_THRESHOLD, 50);
// 		// CHECK_FUNC_IF_SUCCESS(ret, ls_csp_set_parameter);

// 		for (i=audio_jump_size; i < nFileSize[0]; i += max_buf)
// 		{
// 			if(i + max_buf > nFileSize[0])
// 			{
// 				{
// 					memset(tempPcmBuf, 0, sizeof(tempPcmBuf));
// 					memcpy(tempPcmBuf, (const short *)(pBuf[0]+i), nFileSize[0] - i);
// 					//memset((char*)tempPcmBuf + nFileSize[0] - i, 0, max_buf - len);
// 					ret = ls_csp_write(h_csp_inst, (const short*)tempPcmBuf, max_buf);
// 					if (0 != ret)
// 					{
// 						printk("ls_csp_write fail | ret:%d \n\r", ret);
// 					}
// 				}
// 				break;
// 			}

// 			start_counter();

// 			ret = ls_csp_write(h_csp_inst, (const short *)(pBuf[0]+i), max_buf * AUDIO_CHANNEL);
// 			if (LS_SUCCESS != ret)
// 			{
// 				printk("ls_csp_write fail | ret:%d \n\r", ret);
// 				break;
// 			}

// 			cnt_each = get_counter();
// 			cnt_total += cnt_each;
// 		}

// 		start_counter();

// 		{
// 			int len = 1024;
// 			memset(csp_rlt, 0, len);
// 			ret = ls_csp_get_rlt(h_csp_inst, "æ p l", csp_rlt, &len);
// 			// CHECK_FUNC_IF_SUCCESS(ret, ls_csp_get_rlt);

// 			printk("csp_rlt:%s\n", csp_rlt);
// 		}

// 		cnt_each = get_counter();
// 		cnt_total += cnt_each;

// 		printk("counter:%lld, average_clk_cost:%f \r\n", cnt_total, cnt_total / ((nFileSize[0] * 1.0f) / 32768.f) / (1000 * 1000.f));

// 		cnt_total = 0;
// 		GPIO_PinWrite(GPIOA(), (1 << 8), 1);
// 	}
	
// 	ret = ls_csp_destroy(h_csp_inst);
// 	// CHECK_FUNC_IF_SUCCESS(ret, ls_csp_destroy);

// 	return 0;
// }
typedef struct _file_info
{
	char	wav_name[32];
	int8_t *data;
	int32_t size;
	int32_t offset;

}file_info_t,*p_file_info;
///////////////////////////////////
#define MAX_WAV_NUM		(8)

//////////////////////////////////
//#define IVW_ONCE_FRAME_SIZE 16
#define FRAME_SIZE	(320)
static int64_t wsp_inst_psram[300 * 1024 / 8];
static int64_t wsp_res_psram[5 *1024 * 1024 / 8];
static char wsp_rlt[1024] = {0};
static int16_t tempPcmBuf[FRAME_SIZE / 2] = {0};
// static int16_t tempPcmBuf[FRAME_SIZE / 2] = {0};
static T_wsp_res res_mgr__;

static int32_t init_res_mgr_(PT_wsp_res p_res_mgr, const void* mlp_encoder_res, int size_encoder, const void* mlp_decoder_res,  int size_decoder)
{
	int32_t ret = LS_WSP_SUCCESS;

	if(mlp_encoder_res == NULL || size_encoder == 0 )
	{
		return LS_WSP_ERROR_INVALID_PARA_VALUE;
	}

	p_res_mgr->res_buf_arr_[WSP_INDEX_MLP_ENCODER]        = (char*)mlp_encoder_res;
	p_res_mgr->res_size_arr_[WSP_INDEX_MLP_ENCODER]       = size_encoder;
	p_res_mgr->res_buf_arr_[WSP_INDEX_MLP_DECODER]        = (char*)mlp_decoder_res;
	p_res_mgr->res_size_arr_[WSP_INDEX_MLP_DECODER]       = size_decoder;

	return ret;
}
static int32_t read_list(const char *name,file_info_t *files,int max_num)
{
	int32_t file_num = 0;
	int8_t *wav_addr = WAV_ADDR;
	strcpy(files[0].wav_name,"observatories.pcm");
	files[0].data = wav_addr;
	files[0].size = 393218;
	files[0].offset = 0;

	file_num++;
	wav_addr += files[0].size;

	strcpy(files[1].wav_name,"a-z.pcm");
	files[1].data = wav_addr;
	files[1].size = 708098;
	files[1].offset = 0;
	file_num++;

	return file_num;
}
#define VENUS 1
#if VENUS
int wsp_test()
#else
int wsp_test(int argc,char *argv[])
#endif
{
	LS_WSP_HANDLE h_wsp_inst = NULL;
	file_info_t wav_files[8] = {0};
	
	ivInt ret;
	int i, j;
	int inst_size;
	int mem_pool_size;
	int audio_jump_size = 0;
	ivInt value_param = 0;

	unsigned long long cnt_each = 0, cnt_total = 0;

	char * encoder_psram_addr =	wsp_res_psram;
	char * decoder_psram_addr =	(char *)wsp_res_psram + (ENCODER_SIZE + 31) / 32 * 32;
	char * share_mem_addr = NULL;
	int wav_files_num = 0;
	char *wav_list_name = NULL;

	GPIO_SetDir(GPIOA(), (1 << 8), 1);
	GPIO_PinWrite(GPIOA(), (1 << 8), 0);
#if VENUS
	memset(wsp_inst_psram, 0, sizeof(wsp_inst_psram));
	memcpy(encoder_psram_addr, ENCODER_ADDR, ENCODER_SIZE);
	memcpy(decoder_psram_addr, DECODER_ADDR, DECODER_SIZE);
	share_mem_addr = SHAREMEM_ADDR;
#else
	char *encoder_res_name = argv[1];
	char *decoder_res_name = argv[2];
	wav_list_name = argv[3];

	static int32_t g_share_buf [SHAREMEM_SIZE/sizeof(int32_t)] = {0};
	share_mem_addr = g_share_buf;

	load_bin_from_file(encoder_res_name,encoder_psram_addr,ENCODER_SIZE);
	load_bin_from_file(decoder_res_name,decoder_psram_addr,DECODER_SIZE);

#endif
	
	wav_files_num = read_list(wav_list_name,&wav_files,8);

	// init resource mgr
	init_res_mgr_(&res_mgr__, encoder_psram_addr, ENCODER_SIZE, decoder_psram_addr, DECODER_SIZE);

	// get callsp engine infomation
	T_wsp_info t_wsp_info;
	T_wsp_init_param t_wsp_init_param;

	ret = ls_wsp_get_info(&t_wsp_info, &res_mgr__);
	CHECK_FUNC_IF_SUCCESS(ret, ls_wsp_get_info);
	mem_pool_size = t_wsp_info.share_mem_size;
 
	//init callsp engine inst
	t_wsp_init_param.inst_mem_addr = (char *)wsp_inst_psram;
	t_wsp_init_param.inst_mem_size = t_wsp_info.inst_mem_size;
	t_wsp_init_param.res = res_mgr__;

	inst_size = t_wsp_info.inst_mem_size;
	printk("inst_size: %d \n\r", inst_size);
	h_wsp_inst = NULL;

	printk("mem_pool_size: %d \n\r", mem_pool_size);
	t_wsp_init_param.share_mem_addr = (char *)(share_mem_addr);
	t_wsp_init_param.share_mem_size = t_wsp_info.share_mem_size;

	printk("version: %s \n\r", t_wsp_info.version);

	ret = ls_wsp_create(&h_wsp_inst, &t_wsp_init_param);
	CHECK_FUNC_IF_SUCCESS(ret, ls_wsp_create);

	int fid = 0;
	
	for(fid= 0; fid < wav_files_num; fid++)
	{
		ret |= ls_wsp_start(h_wsp_inst);
		CHECK_FUNC_IF_SUCCESS(ret, ls_wsp_start);

		for (i=audio_jump_size; i < wav_files[fid].size; i += FRAME_SIZE)
		{
			// printk("ls_csp_write fail | ret:%d \n\r",i);
				if(i == 102720){
					printk("ls_csp_write fail | ret: \n\r");
					for(int iii =0;iii<16;iii++){
						printk("ls_csp_write fail | ret:%02x \n\r",wav_files[fid].data[i+iii]);
					}
				}
			printk("ls_csp_write fail | ret:%02x \n\r",wav_files[fid].data[i]);
			int32_t write_size = FRAME_SIZE;
			if(i + FRAME_SIZE > wav_files[fid].size)
			{
				write_size =  wav_files[fid].size - i;
			}
			memset(tempPcmBuf,0x00,FRAME_SIZE);
			memcpy(tempPcmBuf, (const short *)(wav_files[fid].data+i), write_size);
			//start_counter();
			ret = ls_wsp_write(h_wsp_inst, (const short *)(wav_files[fid].data+i), FRAME_SIZE);
			if (LS_SUCCESS != ret)
			{
				printk("ls_csp_write fail | ret:%d \n\r", ret);
				break;
			}
			
			//cnt_each = get_counter();
			//printk("frame:[%d],clock:[%f M]\n\r",i / FRAME_SIZE,cnt_each * 1.0f / (1000.0f *1000.0f));
			//cnt_total += cnt_each;
		}
		int len = 1024;
		memset(wsp_rlt, 0, len);
		//start_counter();
		GPIO_PinWrite(GPIOA(), (1 << 8), 1);
		ret |= ls_wsp_stop(h_wsp_inst);
		CHECK_FUNC_IF_SUCCESS(ret, ls_wsp_stop);
		
		ret = ls_wsp_get_rlt(h_wsp_inst, wsp_rlt, &len);
		CHECK_FUNC_IF_SUCCESS(ret, ls_wsp_get_rlt);
		//cnt_each = get_counter();
		GPIO_PinWrite(GPIOA(), (1 << 8), 0);
		printk("wav_name:[%s],wsp_rlt:[%s]\n", wav_files[fid].wav_name,wsp_rlt);
		//printk("last flush, clock:[%f] \r\n", cnt_each * 1.0f /(1000 * 1000.f));

		cnt_total = 0;
	}
	
	ret = ls_wsp_destroy(h_wsp_inst);
	CHECK_FUNC_IF_SUCCESS(ret, ls_wsp_destroy);

	return 0;
}


