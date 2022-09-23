#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "ifr_face_detect_w.h"
#include "ifr_face_align_w.h"
#include "ifr_face_verify_w.h"
#include "ifr_anti_spoofing_w.h"
#include "ifr_params.h"
#include "ifr_types.h"
#include "ifr_face_w.h"

#include "image_640x480.h"
#include "image_88x66.h"
#include <zephyr.h>

#define TEST_FACE_DETECT		0
#define TEST_FACE_ALIGN			0
#define TEST_ANTI_SPOOFING		0
#define TEST_FACE_VERIFY		0
#define TEST_FACE_RECOGNIZE		1
#if defined(WIN32) || defined(linux)
#define TEST_FACE_RECOGNIZE_DB	0
#endif 

#define USE_CRC32_CHECK			1

#define USE_FLASH_RES 1
#define USE_PSRAM_RES 0
#define USE_PSRAM_RES_FOR_FACE_DETECT 0
#define USE_PSRAM_RES_FOR_FACE_ALIGN 0
#define USE_PSRAM_RES_FOR_ANTI_SPOOFING 0
#define USE_PSRAM_RES_FOR_FACE_VERIFY 0

#define PSRAM_SIZE  (200*1024)
#define SHARE_SIZE  (640*1024)
#define OUTPUT_SIZE (1024*1024)

#ifndef IV_ASSERT
#define IV_ASSERT assert
#endif 

#if USE_FLASH_RES 

#if CSK6001
#define g_flash_addr  0x68100000//0x18100000
#else 
#include "face_all.h"
#define g_flash_addr  face_all_resource
#endif 

#define model_face_detect_reslen  (683232)
#define model_face_detect_resource  (g_flash_addr)
#define model_face_align_reslen  (1547984)
#define model_face_align_resource  (model_face_detect_resource + model_face_detect_reslen)
#define model_anti_spoofing_reslen  (1028976)
#define model_anti_spoofing_resource (model_face_align_resource + model_face_align_reslen)
#define model_face_verify_reslen  (3911360)
#define model_face_verify_resource  (model_anti_spoofing_resource + model_anti_spoofing_reslen)

#if TEST_FACE_DETECT
#if USE_PSRAM_RES && USE_PSRAM_RES_FOR_FACE_DETECT
#define g_face_detect_buflen  (model_face_detect_reslen)
char g_face_detect_buf[g_face_detect_buflen];
#else 
#define g_face_detect_buflen	(model_face_detect_reslen)
#define g_face_detect_buf		(model_face_detect_resource)
#endif 
#endif 

#if TEST_FACE_ALIGN
#if USE_PSRAM_RES && USE_PSRAM_RES_FOR_FACE_ALIGN
#define g_face_align_buflen  (model_face_align_reslen)
char g_face_align_buf[g_face_align_buflen];
#else
#define g_face_align_buflen		(model_face_align_reslen)
#define g_face_align_buf		(model_face_align_resource)
#endif 
#endif 

#if TEST_ANTI_SPOOFING
#if USE_PSRAM_RES && USE_PSRAM_RES_FOR_ANTI_SPOOFING
#define g_anti_spoofing_buflen  (model_anti_spoofing_reslen)
char g_anti_spoofing_buf[g_anti_spoofing_buflen];
#else 
#define g_anti_spoofing_buflen	(model_anti_spoofing_reslen)
#define g_anti_spoofing_buf		(model_anti_spoofing_resource)
#endif 
#endif 

#if TEST_FACE_VERIFY
#if USE_PSRAM_RES && USE_PSRAM_RES_FOR_FACE_VERIFY
#define g_face_verify_buflen  (model_face_verify_reslen)
char g_face_verify_buf[g_face_verify_buflen];
#else 
#define g_face_verify_buflen	(model_face_verify_reslen)
#define g_face_verify_buf		(model_face_verify_resource)
#endif 
#endif 

#else 

#include "model_face_detect.h"
#include "model_face_align.h"
#include "model_anti_spoofing.h"
#include "model_face_verify.h"

#define g_face_detect_buflen	(model_face_detect_reslen)
#define g_face_detect_buf		(model_face_detect_resource)
#define g_face_align_buflen		(model_face_align_reslen)
#define g_face_align_buf		(model_face_align_resource)
#define g_anti_spoofing_buflen	(model_anti_spoofing_reslen)
#define g_anti_spoofing_buf		(model_anti_spoofing_resource)
#define g_face_verify_buflen	(model_face_verify_reslen)
#define g_face_verify_buf		(model_face_verify_resource)

#endif 

#if CSK6001
int8_t g_psram_buf[PSRAM_SIZE] __attribute__((aligned(32)));
int8_t* g_share_buf = (int8_t*)0x5fe00000;
#else 
int8_t g_psram_buf[PSRAM_SIZE];
int8_t g_share_buf[SHARE_SIZE];
#endif 

extern void start_counter();
extern uint32_t get_counter();

int printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	vprintk(fmt, ap);

	va_end(ap);
	return 0;
}

#if TEST_FACE_DETECT

int test_face_detect(int argc, char *argv[])
{
	ifr_face_detect_t *p_detect;
	ifr_face_detect_result_t detect_result[100];
	uint32_t use_psram_size = PSRAM_SIZE;
	uint32_t use_share_size = SHARE_SIZE;
	uint32_t g_counter_total = 0;
	uint32_t g_counter_num = 0;
	uint32_t g_crc32 = 0;
	int detect_count = 100;
	int ret = 0;
	int crc32 = 0;

	int out_w;
	int out_h;
	int out_fmt;
	unsigned char* out_data;
	float param_value;
	int param_value_int;

	printf("\n\n===========>>>> %s\n", __FUNCTION__);

	ret = ifr_face_detect_alloc(&p_detect, g_psram_buf, &use_psram_size, g_share_buf, &use_share_size, g_face_detect_buf, g_face_detect_buflen);
	IV_ASSERT(ret == 0);
	printf("use_psram_size = %d, use_share_size = %d\n", use_psram_size, use_share_size);

#ifdef WIN32_OFF
	out_data = g_psram_buf + use_psram_size;
	//ret = iv_parse_bmp(image_640x480_resource, &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	ret = iv_load_bmp("image_640x480.bmp", &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	IV_ASSERT(ret > 0);
	ret = iv_save_bin("image_640x480.bin", out_data, 3 * out_h*out_w);
	IV_ASSERT(ret == 0);
#else 
	out_w = 640;
	out_h = 480;
	out_fmt = 3;
	out_data = image_640x480_resource;
#endif 


	param_value = 0.592667f;
	ret = ifr_face_detect_set_param(p_detect, IFR_PARAM_FACE_DETECT_THRES, &param_value);
	IV_ASSERT(ret == 0);
	param_value = 15;
	ret = ifr_face_detect_set_param(p_detect, IFR_PARAM_FACE_DETECT_PIXESIZE, &param_value);
	IV_ASSERT(ret == 0);
	param_value = 0.592667f;
	ret = ifr_face_detect_set_param(p_detect, IFR_PARAM_FACE_DETECT_PROBTHRES, &param_value);
	IV_ASSERT(ret == 0);
	param_value = 0.6f;
	ret = ifr_face_detect_set_param(p_detect, IFR_PARAM_FACE_DETECT_NSMTHRES, &param_value);
	IV_ASSERT(ret == 0);
// 	param_value_int = 1;
	//ret = ifr_face_detect_set_param(p_detect, IFR_PARAM_FACE_DETECT_DEBUG, &param_value_int);
	IV_ASSERT(ret == 0);
	ret = ifr_face_detect_get_param(p_detect, IFR_PARAM_FACE_DETECT_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);
	ret = ifr_face_detect_get_param(p_detect, IFR_PARAM_FACE_DETECT_THRES, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("thres:%f\n", param_value);
	ret = ifr_face_detect_get_param(p_detect, IFR_PARAM_FACE_DETECT_PIXESIZE, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("pixesize:%f\n", param_value);
	ret = ifr_face_detect_get_param(p_detect, IFR_PARAM_FACE_DETECT_PROBTHRES, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("probthres:%f\n", param_value);
	ret = ifr_face_detect_get_param(p_detect, IFR_PARAM_FACE_DETECT_NSMTHRES, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("nsmthres:%f\n", param_value);

	start_counter();
	ret = ifr_face_detect_process(p_detect, IFR_PIX_FMT_BGR888, out_data, out_w, out_h, detect_result, &detect_count);
	g_counter_total += get_counter();
	g_counter_num += 1;
	IV_ASSERT(ret == 0);
	ifr_face_detect_destory(p_detect);

	printf("counter_total = %f(M), counter_num = %f, counter_avg = %f(M)\n", (float)(g_counter_total / 1000.0 / 1000.0), (float)(g_counter_num),
		g_counter_num == 0 ? .0 : (float)(g_counter_total / 1000.0 / 1000.0) / g_counter_num);
	g_crc32 = iv_crc32_calc(detect_result, detect_count * sizeof(detect_result[0]), 0);
	printf("dectect crc32 = 0x%x\n", g_crc32);
#if USE_CRC32_CHECK
	IV_ASSERT(g_crc32 == 0x281d15c3);
#endif 
	for (int i = 0; i < detect_count; i++)
	{
		printf("face count = %d\t x = %d\t y=%d\t w=%d\t h=%d\t score=%f\r\n",
			detect_count, detect_result[i].rect.x, detect_result[i].rect.y,
			detect_result[i].rect.w, detect_result[i].rect.h, detect_result[i].score);
	}

	return 0;
}


#endif 


#if TEST_FACE_ALIGN

int test_face_align(int argc, char *argv[])
{
	ifr_face_align_t *p_detect;
	ifr_face_align_result_t detect_result[10];
	uint32_t use_psram_size = PSRAM_SIZE;
	uint32_t use_share_size = SHARE_SIZE;
	uint32_t g_counter_total = 0;
	uint32_t g_counter_num = 0;
	uint32_t g_crc32 = 0;
	int detect_count = 10;
	int ret = 0;
	int crc32 = 0;

	int out_w;
	int out_h;
	int out_fmt;
	unsigned char* out_data;
	float param_value;
	int param_value_int;

	printf("\n\n===========>>>> %s\n", __FUNCTION__);

	ret = ifr_face_align_alloc(&p_detect, g_psram_buf, &use_psram_size, g_share_buf, &use_share_size, g_face_align_buf, g_face_align_buflen);
	IV_ASSERT(ret == 0);
	printf("use_psram_size = %d, use_share_size = %d\n", use_psram_size, use_share_size);

#ifdef WIN32_OFF
	out_data = g_psram_buf + use_psram_size;
	//ret = iv_parse_bmp(image_88x66_resource, &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	ret = iv_load_bmp("image_88x66.bmp", &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	IV_ASSERT(ret > 0);
	ret = iv_save_bin("image_88x66.bin", out_data, 3 * out_h*out_w);
	IV_ASSERT(ret == 0);
#else 
	out_w = 640;
	out_h = 480;
	out_fmt = 3;
	out_data = image_640x480_resource;
#endif 
	param_value = 30.0f;
	ret = ifr_face_align_set_param(p_detect, IFR_PARAM_FACE_ALIGN_YAWTHRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_align_set_param(p_detect, IFR_PARAM_FACE_ALIGN_PITCHTHRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_align_set_param(p_detect, IFR_PARAM_FACE_ALIGN_YAWTHRES, &param_value);
	ret = ifr_face_align_get_param(p_detect, IFR_PARAM_FACE_ALIGN_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);

	ifr_rect_t roi;
	roi.x = 251;
	roi.y = 49;
	roi.w = 140;
	roi.h = 150;
	start_counter();
	ret = ifr_face_align_process(p_detect, IFR_PIX_FMT_BGR888, out_data, out_w, out_h, &roi, detect_result, &detect_count);
	g_counter_total += get_counter();
	g_counter_num += 1;
	IV_ASSERT(ret == 0);
	ifr_face_align_destory(p_detect);

	printf("counter_total = %f(M), counter_num = %f, counter_avg = %f(M)\n", (float)(g_counter_total / 1000.0 / 1000.0), (float)(g_counter_num),
		g_counter_num == 0 ? .0 : (float)(g_counter_total / 1000.0 / 1000.0) / g_counter_num);
	g_crc32 = iv_crc32_calc(detect_result, detect_count * sizeof(detect_result[0]), 0); 
	printf("dectect crc32 = 0x%x\n", g_crc32);
#if USE_CRC32_CHECK
	IV_ASSERT(g_crc32 == 0x9a2b6e25);
#endif 
	printf("head_yaw = %f, head_pitch = %f, head_roll = %f\n", detect_result->head_yaw, detect_result->head_pitch, detect_result->head_roll);

	return 0;
}

#endif 


#if TEST_ANTI_SPOOFING

int test_anti_spoofing(int argc, char *argv[])
{
	ifr_anti_spoofing_t *p_detect;
	ifr_anti_spoofing_result_t detect_result[1];
	uint32_t use_psram_size = PSRAM_SIZE;
	uint32_t use_share_size = SHARE_SIZE;
	uint32_t g_counter_total = 0;
	uint32_t g_counter_num = 0;
	uint32_t g_crc32 = 0;
	int detect_count = 1;
	int ret = 0;
	int crc32 = 0;

	int out_w;
	int out_h;
	int out_fmt;
	unsigned char* out_data;
	float param_value;
	int param_value_int;

	printf("\n\n===========>>>> %s\n", __FUNCTION__);

	ret = ifr_anti_spoofing_alloc(&p_detect, g_psram_buf, &use_psram_size, g_share_buf, &use_share_size, g_anti_spoofing_buf, g_anti_spoofing_buflen);
	IV_ASSERT(ret == 0);
	printf("use_psram_size = %d, use_share_size = %d\n", use_psram_size, use_share_size);

#ifdef WIN32_OFF
	out_data = g_psram_buf + use_psram_size;
	//ret = iv_parse_bmp(image_640x480_resource, &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	ret = iv_load_bmp("image_640x480.bmp", &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	IV_ASSERT(ret > 0);
	ret = iv_save_bin("image_640x480.bin", out_data, 3 * out_h*out_w);
	IV_ASSERT(ret == 0);
#else 
	out_w = 640;
	out_h = 480;
	out_fmt = 3;
	out_data = image_640x480_resource;
#endif 

	param_value = 0.2f;
	ret = ifr_anti_spoofing_set_param(p_detect, IFR_PARAM_ANTI_SPOOFING_ENLARGE_SCALE, &param_value);
	IV_ASSERT(ret == 0);
	//param_value_int = 1;
	//ret = ifr_anti_spoofing_set_param(p_detect, IFR_PARAM_ANTI_SPOOFING_DEBUG, &param_value_int);
	//IV_ASSERT(ret == 0);
	param_value = 0.1;
	ret = ifr_anti_spoofing_set_param(p_detect, IFR_PARAM_ANTI_SPOOFING_THRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_anti_spoofing_get_param(p_detect, IFR_PARAM_ANTI_SPOOFING_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);
	ret = ifr_anti_spoofing_get_param(p_detect, IFR_PARAM_ANTI_SPOOFING_ENLARGE_SCALE, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("enlarge_scale:%f\n", param_value);

	//x = 258         y=68    w=116   h=158
	ifr_rect_t roi;
	roi.x = 258;
	roi.y = 68;
	roi.w = 116;
	roi.h = 158;
	start_counter();
	ret = ifr_anti_spoofing_process(p_detect, IFR_PIX_FMT_BGR888, out_data, out_w, out_h, &roi, detect_result, &detect_count);
	g_counter_total += get_counter();
	g_counter_num += 1;
	IV_ASSERT(ret == 0);
	ifr_anti_spoofing_destory(p_detect);

	printf("counter_total = %f(M), counter_num = %f, counter_avg = %f(M)\n", (float)(g_counter_total / 1000.0 / 1000.0), (float)(g_counter_num),
		g_counter_num == 0 ? .0 : (float)(g_counter_total / 1000.0 / 1000.0) / g_counter_num);
	g_crc32 = iv_crc32_calc(detect_result, detect_count * sizeof(detect_result[0]), 0);
	printf("dectect crc32 = 0x%x\n", g_crc32);
#if USE_CRC32_CHECK
	IV_ASSERT(g_crc32 == 0xdbca555a);
#endif 
	for (int i = 0; i < detect_count; i++)
	{
		for (int j = 0; j < IFR_ANTI_SPOOFING_COUNT; j++)
		{
			printf("type = %d, scores = %f\n", j, detect_result[i].anti_spoofing_scores[j]);
		}
	}

	return 0;
}

#endif 

#if TEST_FACE_VERIFY

static float keypoints[68 * 2] =
{
	22.9,23.5625,
	22.9,30.0625,
	23.55,36.5625,
	24.2,43.0625,
	26.8,48.75,
	30.05,53.625,
	35.25,56.875,
	39.8,60.125,
	45,60.9375,
	50.2,60.125,
	55.4,57.6875,
	60.6,55.25,
	64.5,51.1875,
	67.1,45.5,
	68.4,39,
	69.7,32.5,
	70.35,25.1875,
	26.8,17.0625,
	29.4,13,
	33.95,12.1875,
	38.5,13,
	42.4,15.4375,
	49.55,15.4375,
	53.45,13.8125,
	58.65,13,
	62.55,14.625,
	66.45,18.6875,
	45.65,21.125,
	45.65,25.1875,
	45.65,28.4375,
	45.65,31.6875,
	41.1,36.5625,
	43.05,37.375,
	45.65,38.1875,
	48.25,37.375,
	50.85,37.375,
	31.35,22.75,
	33.95,20.3125,
	36.55,21.125,
	39.15,22.75,
	36.55,23.5625,
	33.95,23.5625,
	52.8,23.5625,
	55.4,21.9375,
	58,21.9375,
	60.6,23.5625,
	58,24.375,
	55.4,23.5625,
	37.2,46.3125,
	40.45,43.875,
	43.7,42.25,
	45.65,43.0625,
	48.25,43.0625,
	51.5,43.875,
	54.75,46.3125,
	51.5,49.5625,
	47.6,51.1875,
	45.65,51.1875,
	43.05,50.375,
	40.45,49.5625,
	38.5,46.3125,
	43.7,44.6875,
	45.65,45.5,
	47.6,45.5,
	53.45,46.3125,
	47.6,47.125,
	45.65,47.9375,
	43.7,47.125,
};

int test_face_verify(int argc, char *argv[])
{
	ifr_face_verify_t *p_detect;
	float detect_result[384];
	uint32_t use_psram_size = PSRAM_SIZE;
	uint32_t use_share_size = SHARE_SIZE;
	uint32_t g_counter_total = 0;
	uint32_t g_counter_num = 0;
	uint32_t g_crc32 = 0;
	int detect_count = 384;
	int ret = 0;
	int crc32 = 0;

	int out_w;
	int out_h;
	int out_fmt;
	unsigned char* out_data;
	float param_value;
	int param_value_int;
	float similar;

	printf("\n\n===========>>>> %s\n", __FUNCTION__);

	ret = ifr_face_verify_alloc(&p_detect, g_psram_buf, &use_psram_size, g_share_buf, &use_share_size, g_face_verify_buf, g_face_verify_buflen);
	IV_ASSERT(ret == 0);
	printf("use_psram_size = %d, use_share_size = %d\n", use_psram_size, use_share_size);

#ifdef WIN32_OFF
	out_data = g_psram_buf + use_psram_size;
	//ret = iv_parse_bmp(image_88x66_resource, &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	ret = iv_load_bmp("image_88x66.bmp", &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	IV_ASSERT(ret > 0);
	ret = iv_save_bin("image_88x66.bin", out_data, 3 * out_h*out_w);
	IV_ASSERT(ret == 0);
#else 
	out_w = 88;
	out_h = 66;
	out_fmt = 3;
	out_data = image_88x66_resource;
#endif 
	param_value_int = 1;
	//ret = ifr_face_verify_set_param(p_detect, IFR_PARAM_FACE_VERIFY_DEBUG, &param_value_int);
	IV_ASSERT(ret == 0);
	ret = ifr_face_verify_get_param(p_detect, IFR_PARAM_FACE_VERIFY_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);
	ret = ifr_face_verify_get_param(p_detect, IFR_PARAM_FACE_VERIFY_REGU_A, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("regu_a:%f\n", param_value);
	ret = ifr_face_verify_get_param(p_detect, IFR_PARAM_FACE_VERIFY_REGU_B, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("regu_b:%f\n", param_value);

	ifr_rect_t roi;
	roi.x = 23;
	roi.y = 2;
	roi.w = 44;
	roi.h = 59;
	ifr_point_t points[68];
	for (int i = 0; i < 68; i++)
	{
		points[i].x = keypoints[2 * i];
		points[i].y = keypoints[2 * i + 1];
	}
	start_counter();
	ret = ifr_face_verify_process(p_detect, IFR_PIX_FMT_BGR888, out_data, out_w, out_h, &roi, points, 68, detect_result, &detect_count);
	g_counter_total += get_counter();
	g_counter_num += 1;
	IV_ASSERT(ret == 0);

	ret = ifr_face_verify_compare_feature(p_detect, detect_result, detect_result, detect_count, &similar);
	IV_ASSERT(ret == 0);
	printf("score = %f\n", similar);

	ifr_face_verify_destory(p_detect);

	printf("counter_total = %f(M), counter_num = %f, counter_avg = %f(M)\n", (float)(g_counter_total / 1000.0 / 1000.0), (float)(g_counter_num),
		g_counter_num == 0 ? .0 : (float)(g_counter_total / 1000.0 / 1000.0) / g_counter_num);
	g_crc32 = iv_crc32_calc(detect_result, detect_count * sizeof(detect_result[0]), 0);
	printf("dectect crc32 = 0x%x\n", g_crc32); 
#if USE_CRC32_CHECK
	IV_ASSERT(g_crc32 == 0xf8346d14);
#endif

	return 0;
}

#endif 


#if TEST_FACE_RECOGNIZE

int test_face_recognize(int argc, char *argv[])
{
	ifr_face_t *p_detect;
	ifr_face_recog_result_t detect_result[MAX_DETECT_RESULT];
	uint32_t use_psram_size = PSRAM_SIZE;
	uint32_t use_share_size = SHARE_SIZE;
	uint32_t g_counter_total = 0;
	uint32_t g_counter_num = 0;
	uint32_t g_crc32 = 0;
	int detect_count = MAX_DETECT_RESULT;
	int ret = 0;
	int crc32 = 0;
	float similar = 0;
	int loop = 24 * 86400;

	int out_w;
	int out_h;
	int out_fmt;
	unsigned char* out_data;
	float param_value;
	int param_value_int;
	uint32_t start = 0;

	ifr_face_param_t init_param;
	ifr_face_info_t face_info;
	printf("\n\n===========>>>> %s\n", __FUNCTION__);

	ifr_face_getinfo(&face_info);
	printf("psram_size = %d\n", face_info.psram_size);
	printf("share_size = %d\n", face_info.share_size);

	init_param.psram_addr = g_psram_buf;
	init_param.psram_size = use_psram_size;
	init_param.share_addr = g_share_buf;
	init_param.share_size = use_share_size;
	init_param.res_data[IFR_MODULE_FACE_DETECT] = model_face_detect_resource;
	init_param.res_data[IFR_MODULE_FACE_ALIGN] = model_face_align_resource;
	init_param.res_data[IFR_MODULE_FACE_VERIFY] = model_face_verify_resource;
	init_param.res_data[IFR_MODULE_ANTI_SPOOFING] = model_anti_spoofing_resource;

	init_param.res_size[IFR_MODULE_FACE_DETECT] = model_face_detect_reslen;
	init_param.res_size[IFR_MODULE_FACE_ALIGN] = model_face_align_reslen;
	init_param.res_size[IFR_MODULE_FACE_VERIFY] = model_face_verify_reslen;
	init_param.res_size[IFR_MODULE_ANTI_SPOOFING] = model_anti_spoofing_reslen;

	ret = ifr_face_alloc(&p_detect, &init_param);
	IV_ASSERT(ret == 0);
	printf("use_psram_size = %d, use_share_size = %d\n", init_param.psram_size_used, init_param.share_size_used);
#if 1
	//
	param_value = 0.592667f;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_DETECT_THRES, &param_value);
	IV_ASSERT(ret == 0);
	param_value = 15;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_DETECT_PIXESIZE, &param_value);
	IV_ASSERT(ret == 0);
	param_value = 0.592667f;
	printf("param_value = %f\n", param_value);
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_DETECT_PROBTHRES, &param_value);
	IV_ASSERT(ret == 0);
	param_value = 0.6f;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_DETECT_NSMTHRES, &param_value);
	IV_ASSERT(ret == 0);
	// 	param_value_int = 1;
	//ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_DETECT_DEBUG, &param_value_int);
	IV_ASSERT(ret == 0);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_DETECT_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_DETECT_THRES, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("thres:%f\n", param_value);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_DETECT_PIXESIZE, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("pixesize:%f\n", param_value);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_DETECT_PROBTHRES, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("probthres:%f\n", param_value);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_DETECT_NSMTHRES, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("nsmthres:%f\n", param_value);
	//
	param_value = 30.0f;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_ALIGN_YAWTHRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_ALIGN_PITCHTHRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_ALIGN_YAWTHRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_ALIGN_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);
	//
	param_value_int = 1;
	//ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_VERIFY_DEBUG, &param_value_int);
	IV_ASSERT(ret == 0);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_VERIFY_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_VERIFY_REGU_A, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("regu_a:%f\n", param_value);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_VERIFY_REGU_B, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("regu_b:%f\n", param_value);
	//
	param_value = 0.2f;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_ANTI_SPOOFING_ENLARGE_SCALE, &param_value);
	IV_ASSERT(ret == 0);
	//param_value_int = 1;
	//ret = ifr_face_set_param(p_detect, IFR_PARAM_ANTI_SPOOFING_DEBUG, &param_value_int);
	//IV_ASSERT(ret == 0);
	param_value = 0.1;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_ANTI_SPOOFING_THRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_ANTI_SPOOFING_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_ANTI_SPOOFING_ENLARGE_SCALE, &param_value, sizeof(param_value));
	IV_ASSERT(ret == 0);
	printf("enlarge_scale:%f\n", param_value);
	//
#endif 

	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);

#ifdef WIN32_OFF
	out_data = g_psram_buf + init_param.psram_size_used;
	//ret = iv_parse_bmp(image_192x128_resource, &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	ret = iv_load_bmp("image_640x480_new.bmp", &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
	IV_ASSERT(ret > 0);
	ret = iv_save_bin("image_640x480_new.bin", out_data, 3 * out_h*out_w);
	IV_ASSERT(ret == 0);
#else 
	out_w = 640;
	out_h = 480;
	out_fmt = 3;
	out_data = image_640x480_resource;
#endif 

	for (int k = 0; k < loop; k++)
	{
		printf("loop:[%d/%d]\n", k, loop);
		start = k_uptime_get_32();
		ret = ifr_face_recognize(p_detect, IFR_PIX_FMT_BGR888, out_data, out_w, out_h, detect_result, &detect_count);
		g_counter_total = (k_uptime_get_32() - start);
		g_counter_num += 1;
		IV_ASSERT(ret == 0);
		printf("detect_count:%d cost_time %d\n", detect_count, g_counter_total);

		// printf("counter_total = %f(M), counter_num = %f, counter_avg = %f(M)\n", (float)(g_counter_total / 1000.0 / 1000.0), (float)(g_counter_num),
		// 	g_counter_num == 0 ? .0 : (float)(g_counter_total / 1000.0 / 1000.0) / g_counter_num);
		g_crc32 = iv_crc32_calc(detect_result, detect_count * sizeof(detect_result[0]), 0);
		printf("dectect crc32 = 0x%x\n", g_crc32);
#if USE_CRC32_CHECK
	IV_ASSERT(g_crc32 == 0xa6432297);
#endif 
		for (int i = 0; i < detect_count; i++)
		{
			printf("face_rect count = %d\t x = %d\t y=%d\t w=%d\t h=%d\t score=%f\r\n",
				detect_count, detect_result[i].detect_rect.x, detect_result[i].detect_rect.y,
				detect_result[i].detect_rect.w, detect_result[i].detect_rect.h, detect_result[i].detect_score);
			printf("z:%f\n", detect_result[i].anti_spoofing_score);
			printf("head_yaw = %f, head_pitch = %f, head_roll = %f\n", detect_result[i].head_yaw, detect_result[i].head_pitch, detect_result[i].head_roll);
			ret = ifr_face_compare_feature(p_detect, detect_result[i].feature, detect_result[i].feature, detect_result[i].feature_dim, &similar);
			IV_ASSERT(ret == 0);
			printf("similar:%f\n", similar);
	// 		printf("face_align:\n");
	// 		for (int j = 0; j < detect_result[i].align_points_size; j++)
	// 		{
	// 			printf("x = %f, y = %f\n", detect_result[i].align_points[j].x, detect_result[i].align_points[j].y);
	// 		}
	// 		printf("feature:\n");
	// 		for (int j = 0; j < detect_result[i].feature_dim; j++)
	// 		{
	// 			printf("%f\n", detect_result[i].feature[j]);
	// 		}
		}
		if (k % 10 == 0)
		{
			g_counter_total = 0;
			g_counter_num = 0;
		}
	}
	ifr_face_destory(p_detect);

	return 0;
}

#endif


#if TEST_FACE_RECOGNIZE_DB 

int test_face_recognize_db(int argc, char *argv[])
{
	ifr_face_t *p_detect;
	ifr_face_recog_result_t detect_result[MAX_DETECT_RESULT];
	ifr_face_recog_result_t* p_recog_result;

	uint32_t use_psram_size = PSRAM_SIZE;
	uint32_t use_share_size = SHARE_SIZE;
	uint32_t person_num = 12;
	uint32_t query_num = 3;
	const char* dblist_path[12] = {
		"./face/0.bmp",
		"./face/1.bmp",
		"./face/2.bmp",
		"./face/3.bmp",
		"./face/5.bmp",
		"./face/6.bmp",
		"./face/7.bmp",
		"./face/8.bmp",
		"./face/9.bmp",
		"./face/10.bmp",
		"./face/hongwai.bmp",
		"./face/yswang_1.bmp"
	};
	const char* querylist_path[3] = {
		"./face/0.bmp",
		"./face/hongwai.bmp",
		"./face/yswang_2.bmp"
	};
	float featuredb_data[MAX_FEATURE_DIMS * 20];
	float featuredb_similar[20];
	int featuredb_index[20];
	char save_path[1024];
	char text[1024];
	int detect_count = MAX_DETECT_RESULT;
	int ret = 0;

	int out_w;
	int out_h;
	int out_fmt;
	unsigned char* out_data;
	float param_value;
	int param_value_int;

	ifr_face_param_t init_param;
	ifr_face_info_t face_info;
	printf("\n\n===========>>>> %s\n", __FUNCTION__);

	ifr_face_getinfo(&face_info);
	printf("psram_size = %d\n", face_info.psram_size);
	printf("share_size = %d\n", face_info.share_size);

	init_param.psram_addr = g_psram_buf;
	init_param.psram_size = use_psram_size;
	init_param.share_addr = g_share_buf;
	init_param.share_size = use_share_size;
	init_param.res_data[IFR_MODULE_FACE_DETECT] = g_face_detect_buf;
	init_param.res_data[IFR_MODULE_FACE_ALIGN] = g_face_align_buf;
	init_param.res_data[IFR_MODULE_FACE_VERIFY] = g_face_verify_buf;
	init_param.res_data[IFR_MODULE_ANTI_SPOOFING] = g_anti_spoofing_buf;

	init_param.res_size[IFR_MODULE_FACE_DETECT] = g_face_detect_buflen;
	init_param.res_size[IFR_MODULE_FACE_ALIGN] = g_face_align_buflen;
	init_param.res_size[IFR_MODULE_FACE_VERIFY] = g_face_verify_buflen;
	init_param.res_size[IFR_MODULE_ANTI_SPOOFING] = g_anti_spoofing_buflen;

	ret = ifr_face_alloc(&p_detect, &init_param);
	IV_ASSERT(ret == 0);
	printf("use_psram_size = %d, use_share_size = %d\n", init_param.psram_size_used, init_param.share_size_used);
	ret = ifr_face_get_param(p_detect, IFR_PARAM_FACE_VERSION, &param_value_int, sizeof(param_value_int));
	IV_ASSERT(ret == 0);
	printf("version:0x%x\n", param_value_int);

	param_value = 30.0f;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_ALIGN_YAWTHRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_ALIGN_PITCHTHRES, &param_value);
	IV_ASSERT(ret == 0);
	ret = ifr_face_set_param(p_detect, IFR_PARAM_FACE_ALIGN_YAWTHRES, &param_value);
	IV_ASSERT(ret == 0);
	param_value = 0.1;
	ret = ifr_face_set_param(p_detect, IFR_PARAM_ANTI_SPOOFING_THRES, &param_value);
	IV_ASSERT(ret == 0);

	out_data = malloc(1024*1024);

	printf("create feature db:\n");
	for (int i = 0; i < person_num; i++)
	{
		ret = iv_load_bmp(dblist_path[i], &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
		IV_ASSERT(ret > 0);
		ret = ifr_face_recognize(p_detect, IFR_PIX_FMT_BGR888, out_data, out_w, out_h, detect_result, &detect_count);
		IV_ASSERT(ret == 0);
		// use max detect
		p_recog_result = &(detect_result[0]);
		printf("i = %d, path = %s\n", i, dblist_path[i]);
		printf("face_rect count = %d\t x = %d\t y=%d\t w=%d\t h=%d\t score=%f\r\n",
			detect_count, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y,
			p_recog_result->detect_rect.w, p_recog_result->detect_rect.h, p_recog_result->detect_score);
		printf("anti_spoofing_score:%f\n", p_recog_result->anti_spoofing_score);
		printf("head_yaw = %f, head_pitch = %f, head_roll = %f\n", p_recog_result->head_yaw, p_recog_result->head_pitch, p_recog_result->head_roll);
		IV_ASSERT(ret == 0);
		memcpy(featuredb_data + i*p_recog_result->feature_dim, p_recog_result->feature, p_recog_result->feature_dim * sizeof(float));

		{
			sprintf(save_path, "db_%d.bmp", i);
			iv_draw_rect(out_fmt, out_w, out_h, out_data, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y,
				p_recog_result->detect_rect.w, p_recog_result->detect_rect.h, 255);
			iv_draw_points_f32(out_fmt, out_w, out_h, out_data, (float *)(p_recog_result->align_points), p_recog_result->align_points_size, 255);
			snprintf(text, 1024, "[%d,%d,%d,%d,%.05f]", p_recog_result->detect_rect.x, p_recog_result->detect_rect.y,
				p_recog_result->detect_rect.w, p_recog_result->detect_rect.h, p_recog_result->detect_score);
			iv_draw_text(out_fmt, out_w, out_h, out_data, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y - 20, text, 255 << 16);
			snprintf(text, 1024, "[%.05f,%.05f,%.05f]-[%.05f]", p_recog_result->head_yaw, p_recog_result->head_pitch, p_recog_result->head_roll,
				p_recog_result->anti_spoofing_score);
			iv_draw_text(out_fmt, out_w, out_h, out_data, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y - 40, text, 255 << 16);
			ret = iv_save_bmp(save_path, out_fmt, out_w, out_h, out_data);
			IV_ASSERT(ret > 0);
		}
	}
	printf("save feature db:\n");
	printf("load feature db:\n");
	printf("search feature db:\n");
	for (int i = 0; i < query_num; i++)
	{
		ret = iv_load_bmp(querylist_path[i], &out_fmt, &out_w, &out_h, out_data, 1024 * 1024);
		IV_ASSERT(ret > 0);
		ret = ifr_face_recognize(p_detect, IFR_PIX_FMT_BGR888, out_data, out_w, out_h, detect_result, &detect_count);
		IV_ASSERT(ret == 0);
		if (detect_count <= 0)
		{
			printf("recognize empty\n");
			continue;
		}
		// use max detect
		ifr_face_recog_result_t* p_recog_result = &(detect_result[0]);
		printf("i = %d, path = %s\n", i, querylist_path[i]);
		printf("face_rect count = %d\t x = %d\t y=%d\t w=%d\t h=%d\t score=%f\r\n",
			detect_count, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y,
			p_recog_result->detect_rect.w, p_recog_result->detect_rect.h, p_recog_result->detect_score);
		printf("anti_spoofing_score:%f\n", p_recog_result->anti_spoofing_score);
		printf("head_yaw = %f, head_pitch = %f, head_roll = %f\n", p_recog_result->head_yaw, p_recog_result->head_pitch, p_recog_result->head_roll);
		IV_ASSERT(ret == 0);
		printf("search featuredb:\n");
		ifr_face_search_featuredb(p_detect, featuredb_data, p_recog_result->feature, person_num, p_recog_result->feature_dim, person_num, featuredb_index, featuredb_similar);
		printf("topk:\n");
		for (int j = 0; j < person_num; j++)
		{
			printf("person index = %d, similar = %f, dblist_path = %s\n", featuredb_index[j], featuredb_similar[j], dblist_path[featuredb_index[j]]);
		}

		{
			sprintf(save_path, "query_%d.bmp", i);
			iv_draw_rect(out_fmt, out_w, out_h, out_data, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y,
				p_recog_result->detect_rect.w, p_recog_result->detect_rect.h, 255);
			iv_draw_points_f32(out_fmt, out_w, out_h, out_data, (float *)(p_recog_result->align_points), p_recog_result->align_points_size, 255);
			snprintf(text, 1024, "[%d,%d,%d,%d,%.05f]", p_recog_result->detect_rect.x, p_recog_result->detect_rect.y,
				p_recog_result->detect_rect.w, p_recog_result->detect_rect.h, p_recog_result->detect_score);
			iv_draw_text(out_fmt, out_w, out_h, out_data, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y - 20, text, 255 << 16);
			snprintf(text, 1024, "[%.05f,%.05f,%.05f]-[%.05f]", p_recog_result->head_yaw, p_recog_result->head_pitch, p_recog_result->head_roll,
				p_recog_result->anti_spoofing_score);
			iv_draw_text(out_fmt, out_w, out_h, out_data, p_recog_result->detect_rect.x, p_recog_result->detect_rect.y - 40, text, 255 << 16);
			ret = iv_save_bmp(save_path, out_fmt, out_w, out_h, out_data);
			IV_ASSERT(ret > 0);
		}
	}

	printf("search feature db:\n");

	ifr_face_destory(p_detect);

	free(out_data);

	return 0;
}

#endif

extern void luna_set_check_enable(int enable);
extern void luna_set_sharedmem(void* addr, uint32_t size);

void model_res_init()
{
	printf("model_res_init 1\n");

#if TEST_FACE_DETECT && USE_PSRAM_RES && USE_PSRAM_RES_FOR_FACE_DETECT
	memcpy(g_face_detect_buf, model_face_detect_resource, g_face_detect_buflen);
#endif 
#if TEST_FACE_ALIGN && USE_PSRAM_RES && USE_PSRAM_RES_FOR_FACE_ALIGN
	memcpy(g_face_align_buf, model_face_align_resource, g_face_align_buflen);
#endif 
#if TEST_ANTI_SPOOFING && USE_PSRAM_RES && USE_PSRAM_RES_FOR_ANTI_SPOOFING
	memcpy(g_anti_spoofing_buf, model_anti_spoofing_resource, g_anti_spoofing_buflen);
#endif 
#if TEST_FACE_VERIFY && USE_PSRAM_RES && USE_PSRAM_RES_FOR_FACE_VERIFY
	memcpy(g_face_verify_buf, model_face_verify_resource, g_face_verify_buflen);
#endif 


#if TEST_FACE_DETECT
	printf("g_face_detect_buf crc32 = 0x%x\n", iv_crc32_calc(g_face_detect_buf, g_face_detect_buflen, 0));
#endif 
#if TEST_FACE_ALIGN
	printf("g_face_align_buf crc32 = 0x%x\n", iv_crc32_calc(g_face_align_buf, g_face_align_buflen, 0));
#endif 
#if TEST_ANTI_SPOOFING
	printf("g_anti_spoofing_buf crc32 = 0x%x\n", iv_crc32_calc(g_anti_spoofing_buf, g_anti_spoofing_buflen, 0));
#endif 
#if TEST_FACE_VERIFY
	printf("g_face_verify_buf crc32 = 0x%x\n", iv_crc32_calc(g_face_verify_buf, g_face_verify_buflen, 0));
#endif 

	printf("model_res_init 2\n");
}

int main_app(int argc, char *argv[])
{
// 	luna_set_check_enable(1);
// 	luna_set_sharedmem(g_share_buf, SHARE_SIZE);

	model_res_init();

#if TEST_FACE_DETECT
  	test_face_detect(argc, argv);
#endif 
#if TEST_FACE_ALIGN
	test_face_align(argc, argv);
#endif 
#if TEST_ANTI_SPOOFING
	test_anti_spoofing(argc, argv);
#endif 
#if TEST_FACE_VERIFY
	test_face_verify(argc, argv);
#endif 
#if TEST_FACE_RECOGNIZE
	test_face_recognize(argc, argv);
#endif 
#if TEST_FACE_RECOGNIZE_DB
	test_face_recognize_db(argc, argv);
#endif 
	return 0;
}

