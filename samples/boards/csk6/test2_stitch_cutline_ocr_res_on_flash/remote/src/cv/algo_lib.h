
#ifndef __CSK_ALGO_LIB_HEADER__
#define __CSK_ALGO_LIB_HEADER__

#include <stdint.h>
#include <zephyr.h>
#include <sys/printk.h>

#define CP_ZEPHYR_DEMO		(1)		/* 1: use in zephyr, 0: use in sof */
#define COST_TIME_ENBALE 	(1)		/* time statistics */
#define RES_COPY_FROM_PSRAM	(0)		/*  */
#define RES_ON_FLASH		(1)		/* read resource on flash */

#define IMG_WIDTH           (128)
#define IMG_HEIGHT          (180)
#define IMG_SIZE            (IMG_WIDTH * IMG_HEIGHT)

#define __align_4__ 		__attribute__((aligned(4)))	
#define __packed__			__attribute__((packed))

#define CLOG(fmt, ...)   printk(fmt"\n", ##__VA_ARGS__)

#define CLOG_LEVEL_NONE     0
#define CLOG_LEVEL_ERROR    1
#define CLOG_LEVEL_WARN     2
#define CLOG_LEVEL_INFO     3
#define CLOG_LEVEL_DEBUG    4
#define CLOG_LEVEL_VERBOSE  5

#ifndef CLOG_LEVEL
#define CLOG_LEVEL   CLOG_LEVEL_INFO
#endif

#define LOGE(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_ERROR) CLOG(fmt,##__VA_ARGS__);} while(0)
#define LOGW(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_WARN) CLOG(fmt,##__VA_ARGS__);} while(0)
#define LOGI(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_INFO) CLOG(fmt,##__VA_ARGS__);} while(0)
#define LOGD(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_DEBUG) CLOG(fmt,##__VA_ARGS__);} while(0)
#define LOGV(fmt, ...)     do {if (CLOG_LEVEL >= CLOG_LEVEL_VERBOSE) CLOG(fmt,##__VA_ARGS__);} while(0)

/**
 * @brief 断言函数
 */
#define ASSERT(exp, fmt, ...)                                                                \
	do {                                                                                     \
		if (!(exp)) {                                                                        \
			printk("assert@%s:%d:%s: \n" fmt, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
		}                                                                                    \
	} while (0)


#define FCC_RAM_BASE        (0x5FE00000)		/* FCC information */
#define RAM_ADDR            (FCC_RAM_BASE)
#define TOTAL_RAMSIZE       (640 * 1024)

#define RAMSIZE             (200 * 1024)		/* FCC used by istitch algothrim persist */
#define TEMPSIZE            (290 * 1024)		/* FCC used by istitch algothrim temp */

#define PSRAMSIZE           (1340 * 1024)		/* PSRAM used by cutline algothrim */

#define OCR_PSRAMSIZE      	(4 * 1024)			/* PSRAM used by ocr algothrim */
#define OCR_TEMP_PSRAMSIZE 	(124 * 1024)		/* PSRAM used by ocr algothrim temp */

#define BUFFER_PSRAMSIZE	(1520 * 1024)		/* cutline algothrim input and output buffer, not used, used by outside */
#define TOTAL_PSRAMSIZE     (PSRAMSIZE)			/* max psram size used by istich, cutline, ocr */

// #define CUTLINE_RES_SIZE	(430632)
// #define OCR_RES_SIZE		(2523608)
#define RES_SIZE            (2500 * 1024) 		/* max cutline and ocr resource size */

#define OCR_BUFFER_SIZE     (1 * 1024)			/* ocr result size */
#define DEFAULT_CHANNEL     (1) 				/* ocr input frame channel options */

typedef struct {
    uint32_t 	fuid;
    uint32_t    w;
    uint32_t    h;
	uint32_t	c;
	uint8_t * 	data; 
} img_frame_t;

#define CV_IMAGE_SEND_TAG 			(0x5A46)
#define CV_IMAGE_SEND_VERSION 		(0x00)
#define CV_IMAGE_SEND__PIXEL_DEPHT	(1)

#define CV_IMAGE_SEND_HEADER_LEN	(16)

typedef enum {
	CV_IMAGE_TYPE_INCRE = 0,	// 增量
	CV_IMAGE_TYPE_ORIGINAL,		// 原始
	CV_IMAGE_TYPE_ADJUST,		// 校准
	CV_IMAGE_TYPE_INVALID,		// 
} image_type_e;

typedef enum {
	CV_IMAGE_FORMAT_ONLY_Y = 0,  		//灰度图
	CV_IMAGE_FORMAT_RGB888 = 0x10,  	
	CV_IMAGE_FORMAT_RGB565 = 0x11,  	
	CV_IMAGE_FORMAT_YUYV422 = 0x20,  	
	CV_IMAGE_FORMAT_YVYU422 = 0x21,  	
	CV_IMAGE_FORMAT_UYVY422 = 0x22,  	
	CV_IMAGE_FORMAT_INVALID,
} image_format_e;

typedef struct{
	uint16_t 	tag;
	uint8_t 	version;
	uint16_t 	fuid;
	uint8_t 	type;
	uint8_t 	format;
	signed char x_shift;
	signed char y_shift;
	uint16_t 	width;
	uint16_t 	heigth;
	uint8_t 	depth;
	uint16_t 	check_sum;
	uint8_t 	data[IMG_SIZE];
} __packed__ cv_image_send_t;

uint32_t cost_calc_getus(uint32_t *last);

void cv_xy_crop_init(void);
void cv_xy_crop_proc(img_frame_t i_frame, cv_image_send_t *o_frame);
void cv_xy_crop_deinit(void);

void cv_cutline_proc(img_frame_t i_frame, img_frame_t *o_frame);
void cv_ocr_proc(img_frame_t i_frame, void *result, int *len);

int trans_init(void);
void trans_proc(char *src, int src_len, char *dest, int *len);
void trans_deinit(void);

/* tts algorithm */
#define TTS_BLK_LEN                         (1024)
#define TTS_BASE_VOICE_SPEED_TYPE           (1)
#define TTS_VOICE_SPEED_TYPE                (2)

#define TTS_LANGUAGE_ENGLISH                0
#define TTS_LANGUAGE_CHINESE                1

#define TTS_BASE_SPEED_MIN                  50
#define TTS_BASE_SPEED_NORMAL               100
#define TTS_BASE_SPEED_MAX                  150
typedef enum {
	e_txt_type_none,
	e_txt_type_lineup,
	e_txt_type_break,
} txt_type_e;

typedef struct {
	uint8_t text[TTS_BLK_LEN];
} tts_blk_t;

typedef int (* output_pcm_t)(void*, int);

int tts_engine_init(output_pcm_t output_pcm);													/* engine init */
int tts_exec_synth_text(char *data, uint32_t len); 							/* synth */
int tts_stop_synth(void);													/* stop synth */
int tts_engine_deinit(void); 												/* deinit engine */
int set_tts_voice_speech(uint8_t type, uint8_t languages, uint8_t speed);	/* set voice speed */
int tts_get_role_id(void);
int16_t get_base_speed(void);
bool is_tts_synth_running(void);

#endif /*__CSK_APPLICATION_TASK_HEADER__*/