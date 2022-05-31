#ifndef _LSOCR_TYPE_H_
#define _LSOCR_TYPE_H_


#include "lsocr_define.h"


#define LSOCR_PIX_FMT_DEFAULT     0
#define LSOCR_PIX_FMT_GRAY8       1
#define LSOCR_PIX_FMT_RGB888      2
#define LSOCR_PIX_FMT_BGR888      3
#define LSOCR_PIX_FMT_RGBA8888    4
#define LSOCR_PIX_FMT_BGRA8888    5
#define LSOCR_PIX_FMT_NV21        6
#define LSOCR_PIX_FMT_NV12        7
#define LSOCR_PIX_FMT_I420        8
#define LSOCR_PIX_FMT_YV12        9

#define OCR_LOG_IMG (0)

typedef void * lsocr_handle_t;
typedef struct _lscor_init_param
{
	int8_t *ram_addr;
	int8_t *psram_addr;
	int8_t *temp_ram_addr;
	int8_t *res_addr;

	uint32_t ram_total;
	uint32_t ram_used;

	uint32_t psram_total;
	uint32_t psram_used;

	uint32_t temp_ram_total;
	uint32_t temp_ram_used;

	uint32_t  res_size;
#if OCR_LOG_IMG
	str_t   img_name;
#endif

}lscor_init_param_t;

typedef struct _lsocr_frame
{
	uint32_t    fmt;
	union
	{
		int32_t w;
		int32_t width;
	};
	union
	{
		int32_t h;
		int32_t height;
	};
	union
	{
		int32_t c;
		int32_t channel;
	};
	uint8_t * data;
} lsocr_frame_t;

typedef struct _lsocr_result
{
	ptr_t str;
	int32_t byte_size;
}lsocr_result_t;


typedef struct _lsocr_info
{
	int32_t share_size;
	int32_t psram_size;
	int32_t psram_temp_size;
	int32_t img_channel;
	int32_t img_width;
	int32_t img_height;
}lsocr_info_t;


typedef enum lsocr_param
{
	LSOCR_PARAM_ENCODING = 1,
}lsocr_param_t;

#define LSOCR_PARAM_ENCODING_UTF8 (1)
#define LSOCR_PARAM_ENCODING_UNICODE (2)

#endif
