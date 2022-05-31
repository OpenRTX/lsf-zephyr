#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "algo_lib.h"

#if CP_ZEPHYR_DEMO
#include "ivTTS.h"
#include "ivTTSInUse.h"

#include "istitch_platform.h"
#include "istitch_define.h"
#include "istitch_err.h"
#include "istitch_type.h"
#include "istitch.h"

#include "ls_cutline_define.h"
#include "ls_cutline_err.h"
#include "ls_cutline_type.h"
#include "ls_cutline.h"

#include "mlp_cutline_castor.h" 

#include "lsocr_define.h"
#include "lsocr_err.h"
#include "lsocr_type.h"
#include "lsocr.h"
#include "ocr.h"

#include "cjson/cJSON.h"
#include "cjson/cJSON_Utils.h"
#include "ls_translation_def.h"
#include "ls_translation_type.h"
#include "ls_translation.h"

#else
#include <sof/audio/cv/include/luna/luna.h>

#include <sof/audio/cv/include/tts/ivTTS.h>
#include <sof/audio/cv/include/tts/ivTTSInUse.h>

#include <sof/audio/cv/include/istitch/istitch_platform.h>
#include <sof/audio/cv/include/istitch/istitch_define.h>
#include <sof/audio/cv/include/istitch/istitch_err.h>
#include <sof/audio/cv/include/istitch/istitch_type.h>
#include <sof/audio/cv/include/istitch/istitch.h>

#include <sof/audio/cv/include/cutline/ls_cutline_define.h>
#include <sof/audio/cv/include/cutline/ls_cutline_err.h>
#include <sof/audio/cv/include/cutline/ls_cutline_type.h>
#include <sof/audio/cv/include/cutline/ls_cutline.h>
#include "mlp_cutline_castor.h" 

#include <sof/audio/cv/include/ocr/lsocr_define.h>
#include <sof/audio/cv/include/ocr/lsocr_err.h>
#include <sof/audio/cv/include/ocr/lsocr_type.h>
#include <sof/audio/cv/include/ocr/lsocr.h>
#include "ocr.h"

#include <sof/audio/cv/include/cjson/cJSON.h>
#include <sof/audio/cv/include/cjson/cJSON_Utils.h>

#include <sof/audio/cv/include/trans/ls_translation_def.h>
#include <sof/audio/cv/include/trans/ls_translation_type.h>
#include <sof/audio/cv/include/trans/ls_translation.h>
#endif

/* huge buffer */

static char __align_4__ g_PSRAM_buf[TOTAL_PSRAMSIZE];
#if RES_COPY_FROM_PSRAM
static char __align_4__ res_buffer[RES_SIZE];
#endif
static char __align_4__ ocr_buffer[OCR_BUFFER_SIZE];

typedef struct{
    uint32_t 	fuid;
    uint32_t    w;
    uint32_t    h;
	uint32_t	c;
	uint8_t 	data[IMG_SIZE]; 
} __packed__ last_frame_t;

/* all algorithm handle */
static ISTITCH_INST p_istitch;                  /* xy algorithm handle */
static cutline_inst_t p_cutline;                /* cutline algorithm handle */
static lsocr_handle_t p_lsocr;                  /* ocr algorithm handle */
static lstrans_inst p_trans;                    /* translate algorithm handle */


/* xy algorithm and crop logic */
static int shift[2] = {0};                      /* xy algorithm result */
static __align_4__ last_frame_t last_frame;     /* used by crop logic */
static uint16_t fuid = 0;                       /* protocol send frame id */


/* translate algorithm */
#define TRANS_DEBUG (0)                         /* translate algorithm debug mode */
#define MAX_TRANS_RES_SIZE (16 * 1024 *1024)    /* max translate algorithm resource */

static char info[1024] = { 0 };                 /* translate information */
#if TRANS_DEBUG
static char in_str1[] = "When you were born,you were crying and everyone around you was smiling.";
static char ou_str1[1024];
#endif
static const char* str_table[] =
{
	"null",
	"dict.bin",
	"emb.bin",
	"encoder.bin",
	"decoder.bin",
};


/* tts algorithm */
#define Audio_Size                          (100 * 1024)
#define RES_IN_FLASH_ADDR		            (0x68b00000)
#define ivTTS_HEAP_SIZE                     (1024 * 1024)
#define TTS_ROLE_ID                         ivTTS_ROLE_CATHERINE

static ivHTTS hTTS;
static bool tts_engine_run = false;
static bool break_in_flag = false;
static int16_t g_synth_voice_speed = ivTTS_SPEED_NORMAL;
static int16_t g_base_speed = TTS_BASE_SPEED_NORMAL; // TTS_BASE_SPEED_MIN-TTS_BASE_SPEED_MAX

// static ivByte __align_4__ TTS_Heap[ivTTS_HEAP_SIZE] = {0};
// __align_4__ char Audio_Date[Audio_Size] = {0};
int s_tts_out_size = 0;

static output_pcm_t output_pcm_cb = NULL;

/* time cost debug */
#if COST_TIME_ENBALE
static uint32_t tick = 0;
#else
static uint32_t tick = 0;
static uint32_t tick_temp = 0;
#endif

uint32_t cost_calc_getus(uint32_t *last)
{
    uint32_t cur = k_cycle_get_32();
    uint32_t cp_clk = sys_clock_hw_cycles_per_sec();
    uint32_t u_cp_clk = cp_clk / 1000;
    
    uint32_t span = 1000.0f * (cur - *last) / u_cp_clk + 0.5f;

    *last = cur;
    return span;
}

int luna_printf(const char * fmt, ...)
{
	static char buf[1024] = { 0 };
	va_list ap;

	va_start(ap, fmt);
	uint32_t n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if(n > 0) CLOG("[luna]%s", buf);

	return n;
}


/* xy algorithm and crop logic */
static void algo_stitch_init(void)
{
	TIStitchInitParam params;

	memset((void *)&params, 0x00, sizeof(TIStitchInitParam));
	params.RamAddr = (uint8_t*)RAM_ADDR;
	params.RamTotal = RAMSIZE;
	params.PSRamAddr = NULL;
	params.PSRamTotal = 0;
	params.TempRamAddr = (uint8_t*)(RAM_ADDR + RAMSIZE);
	params.TempRamTotal = TEMPSIZE;

	ISTITCH_ERROR iStatus = ISTITCH_SUCCESS;
	iStatus = iStitchCreate(&p_istitch, &params);
	ASSERT(iStatus == ISTITCH_SUCCESS, "[CV]iStitchCreate");

	char *version = NULL;
	iStatus = iStitchGetVersion(p_istitch, &version);
	ASSERT(iStatus == ISTITCH_SUCCESS, "[CV]iStitchGetVersion");

	LOGI("[CV]iStitch version:%s", version);
	LOGI("[CV]TotalRam:%d,UsedRam:%d", params.RamTotal, params.RamUsed);
	LOGI("[CV]TotalPSRam:%d,UsedPSRam:%d", params.PSRamTotal, params.PSRamUsed);
	LOGI("[CV]TotalTempRam:%d,UsedRam:%d", params.TempRamTotal, params.TempRamUsed);
}

static ISTITCH_ERROR algo_stitch_proc(img_frame_t frame, uint32_t status)
{

    void *stitch_result = (void *)shift;

    static ISTITCH_FRAME iframe;
    unsigned int outSize = 0;
    ISTITCH_OSTATUS outStatus = 0;
    ISTITCH_ISTATUS inStatus = (ISTITCH_ISTATUS)status;
    ISTITCH_ERROR iStatus = ISTITCH_SUCCESS;

    memset(&iframe, 0x00, sizeof(ISTITCH_FRAME));
    iframe.data = frame.data;
    iframe.w = frame.w;
    iframe.h = frame.h;
    iframe.channel = DEFAULT_CHANNEL;
    LOGD("w:%d, h:%d, id:%d, data:%p", frame.w, frame.h, frame.fuid, frame.data);

    ivInt32 result_type = ISTITCH_RESULT_DATA_SHIFT;

#if COST_TIME_ENBALE
    cost_calc_getus(&tick);
#else
	// GPIO_PinWrite(GPIOA(), (1 << 10), 0);
	// cost_calc_getus(&tick);
#endif

    iStatus = iStitchInput(p_istitch, 0, &iframe, sizeof(ISTITCH_FRAME), inStatus);
    if(ISTITCH_SUCCESS != iStatus) {
        LOGE("[CV]iStitchInput %d", iStatus);
        return iStatus;
    }

    iStatus = iStitchGetResult(p_istitch, result_type, stitch_result, &outSize, &outStatus);
    if(ISTITCH_SUCCESS != iStatus) {
        LOGE("[CV]iStitchGetResult %d", iStatus);
        return iStatus;
    }

#if COST_TIME_ENBALE
    LOGI("2:%d", cost_calc_getus(&tick));
#else
	// tick_temp = tick;
	// cost_calc_getus(&tick);
	// LOGI("2.1:%u, %u, %u", tick_temp, tick, sys_clock_hw_cycles_per_sec());
	// GPIO_PinWrite(GPIOA(), (1 << 10), 1);

	cost_calc_getus(&tick);
	if((tick - tick_temp > 297000000) && (tick - tick_temp < 303000000)){
		LOGI("2.1:%u, %u, %u", tick_temp, tick, sys_clock_hw_cycles_per_sec());
		tick_temp = tick;
	}
#endif

    return iStatus;
}

static void algo_stitch_deinit(void)
{
    ISTITCH_ERROR iStatus = ISTITCH_SUCCESS;
    iStatus = iStitchDestroy(p_istitch);
    ASSERT(iStatus == ISTITCH_SUCCESS, "[CV]iStitchDestroy");
}

static void cv_crop_image(int x_shift, int y_shift, last_frame_t* frame, cv_image_send_t *send)
{
    /* crop image */
    int x_off = x_shift;
    if(x_off > 0) LOGW("abnormal x_shift: %d", x_off);

    int x_offset = abs(x_shift);

    int i, j;
    for (i = 0; i < frame->h; i++) {
        for (j = 0; j < x_offset; j++) {
			send->data[i * x_offset + j] = frame->data[i * frame->w + j];
		}
	}

    LOGV("cv crop image ok");

    return;
}

static uint16_t data_pack_check(void *dat, uint32_t len)
{
	if(NULL == dat) {
		LOGE("image pack check null");
		return 0;
	}
	uint16_t check = 0;
	uint8_t *data = (uint8_t *)dat;
	while (len--)
		check += *data++;
	return check;
}

static void make_send_image_pack(cv_image_send_t *send)
{
    /* fit send structure */
    send->tag = CV_IMAGE_SEND_TAG;
    send->version = CV_IMAGE_SEND_VERSION;
    send->fuid = fuid++;
    send->type = CV_IMAGE_TYPE_INCRE;
    send->format = CV_IMAGE_FORMAT_ONLY_Y;
    send->x_shift = shift[0] & 0xff;
    send->y_shift = shift[1] & 0xff;
    send->width = abs(shift[0]);
    send->heigth = IMG_HEIGHT;
    send->depth = CV_IMAGE_SEND__PIXEL_DEPHT;

    /* check sum */
    send->check_sum = data_pack_check(send->data, send->width * send->heigth);

    LOGD("[send]: x: %d, y: %d, w:%d, h:%d", send->x_shift, send->y_shift, send->width, send->heigth);
}

void cv_xy_crop_init(void)
{
    fuid = 0;
    memset(&shift, 0, sizeof(shift));
    memset(&last_frame, 0, sizeof(last_frame));
#if COST_TIME_ENBALE == 0
	IOMuxManager_PinConfigure(0, 10, 1);
	GPIO_SetDir(GPIOA(), (1 << 10), 1);
	GPIO_PinWrite(GPIOA(), (1 << 10), 1);

	cost_calc_getus(&tick);
	tick_temp = tick;
	LOGI("2.2:%u", tick_temp);
#endif
    /* ai init */
    algo_stitch_init();
}

void cv_xy_crop_proc(img_frame_t i_frame, cv_image_send_t *o_frame)
{
    /* xy process */
    int ret = algo_stitch_proc(i_frame, 1);
    if(ISTITCH_SUCCESS != ret) {
        return;
    }

    /* crop image and fill send package */
    if ((shift[0] != 0) || (shift[1] != 0)){
        /* crop image */
        cv_crop_image(shift[0], shift[1], &last_frame, o_frame);

        /* fill send package */
        make_send_image_pack(o_frame);
    }

    /* store image */
    last_frame.fuid = i_frame.fuid;
    last_frame.w = i_frame.w;
    last_frame.h = i_frame.h;
    memcpy(last_frame.data, i_frame.data, IMG_SIZE);

    return;
}

void cv_xy_crop_deinit(void)
{
    /* ai deinit */
    algo_stitch_deinit();
}


/* cutline algorithm */
static void algo_cutline_init(void)
{
    cutline_init_param params;
    cutline_err_t status = CUTLINE_SUCCESS;

    memset((void *)&params, 0x00, sizeof(cutline_init_param));
    params.ram_addr = NULL;
    params.ram_total = 0;
    params.psram_addr = (u32_t *)g_PSRAM_buf;
    params.psram_total = PSRAMSIZE;
    params.temp_ram_addr = (u32_t *)RAM_ADDR;
    params.temp_ram_total = TOTAL_RAMSIZE;

    // load resource of main keyword
#if RES_COPY_FROM_PSRAM
    memset(res_buffer, 0x00, sizeof(res_buffer));
    memcpy(res_buffer, mlp_cutline_castor_resource, sizeof(mlp_cutline_castor_resource));
    LOGD("res: %p, size:%d", mlp_cutline_castor_resource, sizeof(mlp_cutline_castor_resource));

    params.res_addr = (u32_t *)res_buffer;
    params.res_size = sizeof(mlp_cutline_castor_resource);
#else
#if RES_ON_FLASH == 0
    params.res_addr = (u32_t *)mlp_cutline_castor_resource;
    params.res_size = sizeof(mlp_cutline_castor_resource);
#else
    params.res_addr = (uint32_t *)(CONFIG_SOF_AI_CUTLINE_RES_ADDR);
    params.res_size = CONFIG_SOF_AI_CUTLINE_RES_SIZE;
#endif
#endif
    status = ls_cutline_create(&p_cutline, &params);
    ASSERT(status == CUTLINE_SUCCESS, "[CV]ls_cutline_create[%d]", status);

    int8_t *version = NULL;
    ls_cutline_get_version(p_cutline, &version);
    LOGI("[CV]The engine version: %s", version);

	LOGD("[CV]TotalRam:%d,UsedRam:%d", params.ram_total, params.ram_used);
	LOGD("[CV]TotalPSRam:%d,UsedPSRam:%d", params.psram_total, params.psram_used);
	LOGD("[CV]TotalTempRam:%d,UsedRam:%d", params.temp_ram_total, params.temp_ram_used);
}

static void algo_cutline_proc(img_frame_t i_frame, img_frame_t *o_frame)
{
    cutline_frame iframe;

    memset(&iframe, 0x00, sizeof(cutline_frame));
    iframe.data = i_frame.data;
    iframe.w = i_frame.w;
    iframe.h = i_frame.h;
    // iframe.channel = DEFAULT_CHANNEL;

    o_frame->data = i_frame.data;

    cutline_err_t status = CUTLINE_SUCCESS;
    status = ls_cutline_process(p_cutline, (cutline_frame *)&iframe, (cutline_frame *)o_frame);
    ASSERT(status == CUTLINE_SUCCESS, "[CV]ls_cutline_process[%d]", status);
}

static void algo_cutline_deinit(void)
{
    cutline_err_t status = CUTLINE_SUCCESS;
    status = ls_cutline_destroy(p_cutline);
    ASSERT(status == CUTLINE_SUCCESS, "[CV]ls_cutline_destroy[%d]", status);
}

void cv_cutline_proc(img_frame_t i_frame, img_frame_t *o_frame)
{

#if COST_TIME_ENBALE
    cost_calc_getus(&tick);
#endif
    algo_cutline_init();
#if COST_TIME_ENBALE
    LOGI("cutline init: %d", cost_calc_getus(&tick));
#endif

    algo_cutline_proc(i_frame, o_frame);
#if COST_TIME_ENBALE
    LOGI("cutline: %d", cost_calc_getus(&tick));
#endif

    algo_cutline_deinit();

    if ((0 == o_frame->w) && (0 == o_frame->h)) {
        /*cutline error output, so go to the first step*/
        LOGE("cutline error, output w/h 0");
    } else {
        /*cutline right output*/
        LOGI("cutline:oframe.w=%d, oframe.h=%d, len=%d", o_frame->w, o_frame->h, o_frame->w * o_frame->h);
    }
}


/* ocr algorithm */
static void algo_ocr_init(void)
{
    lscor_init_param_t params;
    lsocr_errcode_t iStatus = LSOCR_ERR_OK;

    memset((void *)&params, 0x00, sizeof(lscor_init_param_t));
    memset(g_PSRAM_buf, 0, sizeof(g_PSRAM_buf));

    params.ram_addr = (int8_t *)RAM_ADDR;
    params.ram_total = TOTAL_RAMSIZE;
    params.psram_addr = (int8_t *)g_PSRAM_buf;
    params.psram_total = OCR_PSRAMSIZE;
    params.temp_ram_addr = (int8_t *)&g_PSRAM_buf[OCR_PSRAMSIZE];
    params.temp_ram_total = OCR_TEMP_PSRAMSIZE;

#if RES_COPY_FROM_PSRAM
    // load resource of main keyword
    memset(res_buffer, 0x00, sizeof(res_buffer));
    memcpy(res_buffer, ocr_resource, sizeof(ocr_resource));
    LOGD("res: %p, size: %d, size_r:%d", res_buffer, sizeof(res_buffer), sizeof(ocr_resource));

    params.res_addr = (u32_t *)res_buffer;
    params.res_size = sizeof(ocr_resource);
#else
#if RES_ON_FLASH == 0
    params.res_addr = (u32_t *)ocr_resource;
    params.res_size = sizeof(ocr_resource);
#else
    params.res_addr = (char *)(CONFIG_SOF_AI_OCR_RES_ADDR);
    params.res_size = CONFIG_SOF_AI_OCR_RES_SIZE;
#endif
#endif

    iStatus = lsocr_create(&p_lsocr, &params);
    ASSERT(iStatus == LSOCR_ERR_OK, "[CV]lsocr_create[%d]", iStatus);

	char *version = NULL;
	iStatus = lsocr_get_version(p_lsocr, &version);
	ASSERT(iStatus == LSOCR_ERR_OK, "[CV]lsocr_get_version[%d]", iStatus);
	LOGI("[CV]LSOCR version:%s", version);

	LOGD("[CV]TotalRam:%d,UsedRam:%d", params.ram_total, params.ram_used);
	LOGD("[CV]TotalPSRam:%d,UsedPSRam:%d", params.psram_total, params.psram_used);
	LOGD("[CV]TotalTempRam:%d,UsedRam:%d", params.temp_ram_total, params.temp_ram_used);

	int32_t encoding = LSOCR_PARAM_ENCODING_UTF8;
	iStatus = lsocr_set_param(p_lsocr, LSOCR_PARAM_ENCODING, &encoding, sizeof(encoding));
    ASSERT(iStatus == LSOCR_ERR_OK, "[CV]lsocr_set_param[%d]", iStatus);

	int32_t encoding_ret = 0;
	int32_t value_len = sizeof(encoding_ret);
	iStatus = lsocr_get_param(p_lsocr, LSOCR_PARAM_ENCODING, &encoding_ret, (uint32_t *)&value_len);
    ASSERT(iStatus == LSOCR_ERR_OK, "[CV]lsocr_get_param[%d]", iStatus);

	LOGI("[CV]encoding:%d", encoding_ret);
}

static void algo_ocr_proc(img_frame_t i_frame, void* result, int* len)
{
    lsocr_errcode_t iStatus = LSOCR_ERR_OK;

    lsocr_frame_t iframe;
    memset(&iframe, 0, sizeof(lsocr_frame_t));
    iframe.fmt = ISTITCH_PIX_FMT_GRAY8;
    iframe.data = i_frame.data;
    iframe.w = i_frame.w;
    iframe.h = i_frame.h;
    iframe.c = DEFAULT_CHANNEL;

    lsocr_result_t ocr_result = {0};
    // memset(ocr_buffer, 0x00, sizeof(ocr_buffer));
    memset((char *)result, 0, *len);
    ocr_result.str = result;
    ocr_result.byte_size = *len;
    iStatus = lsocr_process(p_lsocr, iframe, &ocr_result);
    ASSERT(iStatus == LSOCR_ERR_OK, "[CV]lsocr_process[%d]", iStatus);

    LOGD("ocr_result[%d]: %s", ocr_result.byte_size, ocr_result.str);
    *len = ocr_result.byte_size;
    LOGI("ocr_result[%d]: %s", *len, result);
}

static void algo_ocr_deinit(void)
{
    lsocr_errcode_t iStatus = LSOCR_ERR_OK;
    iStatus = lsocr_destroy(p_lsocr);
    ASSERT(iStatus == LSOCR_ERR_OK, "[CV]lsocr_destroy[%d]", iStatus);
}

void cv_ocr_proc(img_frame_t i_frame, void *result, int *len)
{
#if COST_TIME_ENBALE
    cost_calc_getus(&tick);
#endif
    algo_ocr_init();
#if COST_TIME_ENBALE
    LOGI("ocr init: %d", cost_calc_getus(&tick));
#endif
    algo_ocr_proc(i_frame, (char *)result, len);
#if COST_TIME_ENBALE
    LOGI("ocr: %d", cost_calc_getus(&tick));
#endif

    algo_ocr_deinit();
}


/* translate algorithm */
static void cjson_hook_set(void)
{
	cJSON_Hooks hook = {
			.malloc_fn = k_malloc,
			.free_fn = k_free,
	};
	cJSON_InitHooks(&hook);
}

static int32_t lstrans_AsynResult_cb(const char* pParam, void *pUserParam)
{
	LOGI("AsynResult[%d]:{%s}", (int)strlen(pParam)+1, pParam);
	return 0;
}

int trans_init(void)
{
	lstrans_errcode_e ret = LSTRANS_OK;

#if COST_TIME_ENBALE
    cost_calc_getus(&tick);
#endif

    LOGI("trans in");

	lstrans_mem_t mem = {
		.share = {(char*)RAM_ADDR,  (uint32_t)TOTAL_RAMSIZE},
		.psram = {(char*)g_PSRAM_buf,  (uint32_t)TOTAL_PSRAMSIZE},
		.res = {
			.dict		= {(char*)NULL,	0},
			.embedding	= {(char*)NULL, 0},
			.encoder	= {(char*)NULL,	0},
            .decoder	= {(char*)NULL,	0},
            // .decoder	= {(char*)g_res_decoder,	(uint32_t)DECO_BUFFER_SIZE},
		}
	};

	memset(RAM_ADDR, 0, TOTAL_RAMSIZE);
	memset((void *)g_PSRAM_buf, 0, TOTAL_PSRAMSIZE);
	// memset((void *)g_res_decoder, 0, DECO_BUFFER_SIZE);

	mem_addr_t res_addrs = {
		.addr = (char*)(CONFIG_SOF_AI_TRANS_RES_ADDR),
		.size = MAX_TRANS_RES_SIZE		
	};

	int len = 1024;

    cjson_hook_set();

	ret = lstrans_get_info(&res_addrs, info, &len);
    ASSERT(ret == LSTRANS_OK, "[TRANS]lstrans_get_info[%d]", ret);
	LOGD("info ret = %d, len %d:%s", ret, len, info);

	cJSON *root = NULL, *jsmodule = NULL, *jsub = NULL, *item = NULL;
	if (!(root = cJSON_Parse(info))) {
		LOGE("parse json error");
		return -1;
	}

	if (jsmodule = cJSON_GetObjectItem(root, "resource"))
	{
		for (int i = 1; i < sizeof(str_table) / sizeof(str_table[0]); i++)
		{
			if ((jsub = cJSON_GetObjectItem(jsmodule, str_table[i])))
			{
				int offset = 0,size = 0;
				if ((item = cJSON_GetObjectItem(jsub, "addr_offset")))
				{
					offset = item->valueint;
				}

				if ((item = cJSON_GetObjectItem(jsub, "size")))
				{
					size = item->valueint;
				}

				if (0 == strcmp(str_table[i], "dict.bin"))
				{
					mem.res.dict.addr = (char *)res_addrs.addr + offset;
					mem.res.dict.size = size;

					LOGI("dict addr: %p, len %d", mem.res.dict.addr, mem.res.dict.size);
				}
				else if (0 == strcmp(str_table[i], "emb.bin"))
				{
					mem.res.embedding.addr = (char *)res_addrs.addr + offset;
					mem.res.embedding.size = size;

					LOGI("emb addr: %p, len %d", mem.res.embedding.addr, mem.res.embedding.size);
				}
				else if (0 == strcmp(str_table[i], "encoder.bin"))
				{
					mem.res.encoder.addr = (char *)res_addrs.addr + offset;
					mem.res.encoder.size = size;

					LOGI("emb addr: %p, len %d", mem.res.encoder.addr, mem.res.encoder.size);
				}
				else if (0 == strcmp(str_table[i], "decoder.bin"))
				{
					// memcpy(mem.res.decoder.addr, res_addrs.addr + offset, size);
                    mem.res.decoder.addr = (char *)res_addrs.addr + offset;
					mem.res.decoder.size = size;

					LOGI("decoder addr: %p, len %d", mem.res.decoder.addr, mem.res.decoder.size);
				}
			}
		}	
	}

	cJSON_Delete(root);
	LOGI("parse done");

	/* init */
	ret = lstrans_create(&p_trans, &mem);
    ASSERT(ret == LSTRANS_OK, "[TRANS]lstrans_create[%d]", ret);

#if COST_TIME_ENBALE
    LOGI("lstrans proc: %d", cost_calc_getus(&tick));
#endif

	LOGI("lscreate done!");   

#if ASY_RESULT
	ret = lstrans_register_callback(p_inst, CallBackAsynResult, lstrans_AsynResult_cb,NULL);
    ASSERT(ret == LSTRANS_OK, "[TRANS]lstrans_register_callback[%d]", ret);
#endif

    return 0;
}

void trans_proc(char *src, int src_len, char *dest, int *len)
{
	data_t in_data;
	data_t result;

#if TRANS_DEBUG
	in_data.str = in_str1;
	in_data.size = strnlen(in_str1, sizeof(in_str1)) + 1;
	LOGI("input str : %s", in_data.str);

	memset(ou_str1, 0, sizeof(ou_str1));
	result.str = ou_str1;
	result.size = sizeof(ou_str1);
#else
	in_data.str = src;
	in_data.size = src_len;
	LOGD("input str [%d][%d]: %s", src_len, *len, in_data.str);

	memset(dest, 0, *len);
	result.str = dest;
	result.size = *len;
#endif

#if COST_TIME_ENBALE
    cost_calc_getus(&tick);
#endif

	/* process */	
	lstrans_errcode_e ret = lstrans_process(p_trans, &in_data, &result, NULL);
    ASSERT(ret == LSTRANS_OK, "[TRANS]lstrans_process[%d]", ret);

#if COST_TIME_ENBALE
    LOGI("lstrans proc: %d", cost_calc_getus(&tick));
#endif

	LOGI("result[%d]:{%s}", result.size, result.str);
    *len = result.size;

    return;
}

void trans_deinit(void)
{
    lstrans_errcode_e ret = lstrans_destroy(p_trans);

    return;
}


/* tts algorithm */
/* Message */
ivTTSErrID DoMessage()
{
	/* 获取消息，用户实现 */
	if (!break_in_flag) {
		/* 继续合成 */
		return ivTTS_ERR_OK;
	} else {
		break_in_flag = false;
		/* 退出合成 */
		return ivTTS_ERR_EXIT;
	}
}

/* output callback */
ivTTSErrID OnOutput(ivUInt16 nCode,	ivCPointer pcData, ivSize nSize)		 
{
#if 0
	// printf("%d\n", nSize);
	//spi_audio_proto_send(pcData, nSize);
	if((s_tts_out_size + nSize) > Audio_Size){
		printk("the tts out is over 100K bytes,return \n");
	} else {
		memcpy(Audio_Date+s_tts_out_size,pcData,nSize);
	}
	s_tts_out_size += nSize;
	return ivTTS_ERR_OK;
#else
	s_tts_out_size += output_pcm_cb(pcData, nSize);

	return ivTTS_ERR_OK;
#endif
}

/* read resource callback */
static ivBool ivCall ReadResCB(ivPointer pParameter, ivPointer pBuffer,	ivResAddress iPos, ivResSize nSize)		
{
	//printf("ReadResCB:%0x %0x %d %d\n",pParameter, pBuffer, iPos, nSize);
	memcpy(pBuffer, (char *)pParameter + iPos, nSize);
	return ivTrue;
}

/* output callback */
static ivTTSErrID ivCall OutputCB(ivPointer pParameter, ivUInt16 nCode,	ivCPointer pcData, ivSize nSize)		  
{
	/* 获取线程消息，是否退出合成 */
	ivTTSErrID tErr = DoMessage();
	if (tErr != ivTTS_ERR_OK)
		return tErr;

	/* 把语音数据送去播音 */
	return OnOutput(nCode, pcData, nSize);
	return ivTTS_ERR_OK;
}

/* parameter change callback */
static ivTTSErrID ivCall ParamChangeCB(ivPointer pParameter, ivUInt32 nParamID,	ivUInt32 nParamValue) 
{
	return ivTTS_ERR_OK;
}

/* progress callback */
static ivTTSErrID ivCall ProgressCB(ivPointer pParameter, ivUInt32 iProcPos, ivUInt32 nProcLen)
{
	return ivTTS_ERR_OK;
}

int tts_engine_init(output_pcm_t output_pcm)
{
	LOGI("call:%s short=%d ", __FUNCTION__,sizeof(short));

	if(tts_engine_run) {
		LOGW("tts has been initialized");
		return -2;
	}

    output_pcm_cb = output_pcm;
    s_tts_out_size = 0;

	/* 分配堆 */
	tts_engine_run = true;
    ivTTSErrID ivReturn = ivTTS_ERR_FAILED;

	/* LOG信息输出 */
	// static ivTTTsUserInfo tUserInfo;
	// tUserInfo.pCBLogParameter = NULL;
	// tUserInfo.pfnLog = NULL;

	/* 初始化资源, 可以有多个资源包，可以分包*/
    static ivTResPackDescExt tResPackDesc = {
        .pCBParam = CONFIG_SOF_AI_TTS_RES_ADDR,
        .pfnRead = ReadResCB,
        .pfnMap = NULL,
        .nSize = 0,

        /* TTS内部使用 */
        .pCacheBlockIndex = NULL,
        .pCacheBuffer = NULL,
        .nCacheBlockSize = 0,
        .nCacheBlockCount = 0,
        .nCacheBlockExt = 0,
    };

    memset(g_PSRAM_buf, 0, sizeof(g_PSRAM_buf));

	/* 创建 TTS 实例 */
	ivReturn = ivTTS_Create(&hTTS, (ivPointer)g_PSRAM_buf, TOTAL_PSRAMSIZE,
							NULL, (ivPResPackDescExt)&tResPackDesc, (ivSize)1, NULL);
	ASSERT(ivTTS_ERR_OK == ivReturn, "create ivReturn(0x%x)", ivReturn);

    /* 获取版本号 */
	uint8_t version[4] = {0};
	ivReturn = ivTTS_GetVersion(&version[0], &version[1], (uint16_t *)&version[2]);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_GetVersion ivReturn(0x%x)", ivReturn);
	LOGI("ivTTS version %d.%d.%d", version[0], version[1], version[2] | version[3] >> 8);

	/* 设置音频输出回调 */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_OUTPUT_CALLBACK, (ivSize)OutputCB);
	LOGV("ivTTS_PARAM_OUTPUT_CALLBACK ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_OUTPUT_CALLBACK ivReturn(0x%x)", ivReturn);

	/* 设置参数改变回调 */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_PARAMCH_CALLBACK, (ivSize)ParamChangeCB);
	LOGV("ivTTS_PARAM_PARAMCH_CALLBACK ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_PARAMCH_CALLBACK ivReturn(0x%x)", ivReturn);

	/* 设置处理进度回调 */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_PROGRESS_CALLBACK, (ivSize)ProgressCB);
	LOGV("ivTTS_PARAM_PROGRESS_CALLBACK ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_PROGRESS_CALLBACK ivReturn(0x%x)", ivReturn);

	/* 设置输入文本代码页 */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_INPUT_CODEPAGE, ivTTS_CODEPAGE_UTF8);
	LOGV("ivTTS_PARAM_INPUT_CODEPAGE ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_INPUT_CODEPAGE ivReturn(0x%x)", ivReturn);

	/* 设置语种 */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_LANGUAGE, ivTTS_LANGUAGE_AUTO);
	LOGV("ivTTS_PARAM_LANGUAGE ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_LANGUAGE ivReturn(0x%x)", ivReturn);

	/* 设置音量 */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_VOLUME, ivTTS_VOLUME_NORMAL);
	LOGV("ivTTS_PARAM_VOLUME ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_VOLUME ivReturn(0x%x)", ivReturn);

	/* 设置发音人为 XIAOYAN */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_ROLE, TTS_ROLE_ID);
	LOGV("ivTTS_PARAM_ROLE ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_ROLE ivReturn(0x%x)", ivReturn);

	/* 设置语速 */
	ivReturn = ivTTS_SetParam(hTTS, ivTTS_PARAM_VOICE_SPEED, g_synth_voice_speed);
	LOGV("ivTTS_PARAM_VOICE_SPEED ivReturn(0x%x)", ivReturn);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_PARAM_VOICE_SPEED ivReturn(0x%x)", ivReturn);

	LOGI("exit:%s", __FUNCTION__);
	return ivReturn;
}

int tts_exec_synth_text(char *data, uint32_t len)
{
	ivTTSErrID ivReturn = ivTTS_ERR_FAILED;
	LOGD("call:%s, ivReturn addr=%x", __FUNCTION__,&ivReturn);
	break_in_flag = false;
	ivReturn = ivTTS_SynthText(hTTS, data, -1);
    ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_SynthText ivReturn(0x%x)", ivReturn);

	LOGD("exit:%s,tts out size=%d", __FUNCTION__,s_tts_out_size);
	return ivReturn;
}

int tts_stop_synth(void)
{
	// LOGV("call:%s", __FUNCTION__);
	if(tts_engine_run) {
		ivTTS_Exit(hTTS);
	}
	// LOGV("exit:%s", __FUNCTION__);
	return 0;
}

int tts_engine_deinit(void)
{
	LOGV("call:%s", __FUNCTION__);
	if(!tts_engine_run) {
		LOGW("has been deinit");
		return -1;
	}
	ivTTSErrID ivReturn = ivTTS_ERR_FAILED;
	ivReturn = ivTTS_Destroy(hTTS);
	ASSERT(ivTTS_ERR_OK == ivReturn, "ivTTS_Destroy ivReturn(0x%x)", ivReturn);

	tts_engine_run = false;
	LOGV("exit:%s", __FUNCTION__);
	return ivReturn;
}

static int16_t base_speed_to_voice_speed(int16_t speed)
{
	int16_t voice_speed = ivTTS_SPEED_NORMAL;
	LOGV("enter speed:%d", speed);
	if (speed < TTS_BASE_SPEED_MIN) {
		speed = TTS_BASE_SPEED_MIN;
	}
	if (speed > TTS_BASE_SPEED_MAX) {
		speed = TTS_BASE_SPEED_MAX;
	}
	int rate = (speed - TTS_BASE_SPEED_MIN) * 100/ (TTS_BASE_SPEED_MAX - TTS_BASE_SPEED_MIN);
	voice_speed = (ivTTS_SPEED_MAX - ivTTS_SPEED_MIN) * rate / 100 + ivTTS_SPEED_MIN;
	LOGV("voice speed:%d, rate:%d, base speed:%d", voice_speed, rate, speed);
	return voice_speed;
}

int set_tts_voice_speech(uint8_t type, uint8_t languages, uint8_t speed)
{
	LOGV("type:%d, languages:%d, speed:%d", type, languages, speed);
	int16_t _tts_speed = 0;
	if(TTS_LANGUAGE_ENGLISH == languages) {
		if(TTS_BASE_VOICE_SPEED_TYPE == type) {
			g_synth_voice_speed = base_speed_to_voice_speed(speed);
			g_base_speed = speed;
		} else if(TTS_VOICE_SPEED_TYPE == type) {
			switch (speed) {
			case 0: // 慢速
				_tts_speed = g_base_speed * 6 / 10;
				break;
			case 1: // 中速
				_tts_speed = g_base_speed * 7 / 10;
				break;
			case 2: // 快速
				_tts_speed = g_base_speed * 1;
				break;
			default:
				break;
			}
			g_synth_voice_speed = base_speed_to_voice_speed(_tts_speed);
		} else {
			LOGE("not support type(%d)", type);
			return -1;
		}
	} else {
		LOGE("not support languages(%d)", languages);
		return -1;
	}
	return 0;
}

int tts_get_role_id(void)
{
	return TTS_ROLE_ID;
}

int16_t get_base_speed(void)
{
	LOGV("g_base_speed: %d, g_synth_voice_speed: %d", g_base_speed, g_synth_voice_speed);
	return g_base_speed;
}

bool is_tts_synth_running(void)
{
	return tts_engine_run;
}
