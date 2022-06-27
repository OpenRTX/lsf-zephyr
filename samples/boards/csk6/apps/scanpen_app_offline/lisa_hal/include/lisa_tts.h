/*
 * ================================================================
 *
 *			Filename:	lisa_tts.h
 *
 *		 Description:	lisa_tts header file
 *
 *			 Version:	1.0
 *			 Created:	03/10/2022 09:10:08
 *			Revision:	None
 *
 *			  Author:	dyye@listenai.com
 *			 Company:	www.listenai.com
 *
 * =================================================================
 */
#ifndef __LISA_TTS_H__
#define __LISA_TTS_H__

/**
 * @brief lisa arecord msg
 * These msg can be obtained through  lisa_tts_cb callback
 */
typedef enum {
	TTS_START = 0,
	/*when tts play over callbak this msg*/
	TTS_PLAY_OVER,
	/*reserved*/
	TTS_ERR,
} lisa_tts_cbmsg_e;

struct _lisa_tts_context_t;
typedef struct _lisa_tts_context_t lisa_tts_context_t;

/**
 * @brief 
 *
 * @param msg	callbak msg
 * @param data  record data pointer
 * @param len	record len
 *
 * @retval  null
 */
typedef void (*lisa_tts_cb)(lisa_tts_cbmsg_e msg, void *data, uint32_t len);

/**
 * @brief  lisat tts params struct
 */
typedef struct {
	lisa_tts_cb ttscb;
	/*set tts fmt*/
	//lisa_tts_fmt_t fmt;
} lisa_tts_params_t;

/**
 * @brief  lisa tts handler
 */
typedef struct {
	lisa_tts_params_t params;
	lisa_tts_context_t *ctx;
} lisa_tts_handler_t;

/* constants for values of parameter ivTTS_PARAM_SPEAKER */
#define ivTTS_ROLE_TIANCHANG			1			/* Tianchang (female, Chinese) */
#define ivTTS_ROLE_WENJING				2			/* Wenjing (female, Chinese) */
#define ivTTS_ROLE_XIAOYAN				3			/* Xiaoyan (female, Chinese) */
#define ivTTS_ROLE_YANPING				3			/* Xiaoyan (female, Chinese) */
#define ivTTS_ROLE_XIAOFENG				4			/* Xiaofeng (male, Chinese) */
#define ivTTS_ROLE_YUFENG				4			/* Xiaofeng (male, Chinese) */
#define ivTTS_ROLE_SHERRI				5			/* Sherri (female, US English) */
#define ivTTS_ROLE_XIAOJIN				6			/* Xiaojin (female, Chinese) */
#define ivTTS_ROLE_NANNAN				7			/* Nannan (child, Chinese) */
#define ivTTS_ROLE_JINGER				8			/* Jinger (female, Chinese) */
#define ivTTS_ROLE_JIAJIA				9			/* Jiajia (girl, Chinese) */
#define ivTTS_ROLE_YUER					10			/* Yuer (female, Chinese) */
#define ivTTS_ROLE_XIAOQIAN				11			/* Xiaoqian (female, Chinese Northeast) */
#define ivTTS_ROLE_LAOMA				12			/* Laoma (male, Chinese) */
#define ivTTS_ROLE_BUSH					13			/* Bush (male, US English) */
#define ivTTS_ROLE_XIAORONG				14			/* Xiaorong (female, Chinese Szechwan) */
#define ivTTS_ROLE_XIAOMEI				15			/* Xiaomei (female, Cantonese) */
#define ivTTS_ROLE_ANNI					16			/* Anni (female, Chinese) */
#define ivTTS_ROLE_JOHN					17			/* John (male, US English) */
#define ivTTS_ROLE_ANITA				18			/* Anita (female, British English) */
#define ivTTS_ROLE_TERRY				19			/* Terry (female, US English) */
#define ivTTS_ROLE_CATHERINE			20			/* Catherine (female, US English) */
#define ivTTS_ROLE_TERRYW				21			/* Terry (female, US English Word) */
#define ivTTS_ROLE_XIAOLIN				22			/* Xiaolin (female, Chinese) */
#define ivTTS_ROLE_XIAOMENG				23			/* Xiaomeng (female, Chinese) */
#define ivTTS_ROLE_XIAOQIANG			24			/* Xiaoqiang (male, Chinese) */
#define ivTTS_ROLE_XIAOKUN				25			/* XiaoKun (male, Chinese) */
#define ivTTS_ROLE_JIUXU				51			/* Jiu Xu (male, Chinese) */
#define ivTTS_ROLE_DUOXU				52			/* Duo Xu (male, Chinese) */
#define ivTTS_ROLE_XIAOPING				53			/* Xiaoping (female, Chinese) */
#define ivTTS_ROLE_DONALDDUCK			54			/* Donald Duck (male, Chinese) */
#define ivTTS_ROLE_BABYXU				55			/* Baby Xu (child, Chinese) */
#define ivTTS_ROLE_DALONG				56			/* Dalong (male, Cantonese) */
#define ivTTS_ROLE_TOM					57			/* Tom (male, US English) */
#define ivTTS_ROLE_USER					99			/* user defined */

/* constants for values of parameter ivTTS_PARAM_LANGUAGE */
#define ivTTS_LANGUAGE_AUTO             0           /* Detect language automatically */
#define ivTTS_LANGUAGE_CHINESE			1			/* Chinese (with English) */
#define ivTTS_LANGUAGE_ENGLISH			2			/* English */

/* constants for values of parameter ivTTS_PARAM_VOICE_SPEED */
/* the range of voice speed value is from -32768 to +32767 */
#define ivTTS_SPEED_MIN					-32768		/* slowest voice speed */
#define ivTTS_SPEED_NORMAL				0			/* normal voice speed (default) */
#define ivTTS_SPEED_MAX					+32767		/* fastest voice speed */

/* constants for values of parameter ivTTS_PARAM_VOLUME */
/* the range of volume value is from -32768 to +32767 */
#define ivTTS_VOLUME_MIN				-32768		/* minimized volume */
#define ivTTS_VOLUME_NORMAL				0			/* normal volume */
#define ivTTS_VOLUME_MAX				+32767		/* maximized volume (default) */

/**
 * @brief lisa tts config
 * 
 */
typedef struct{
    uint8_t role;
    uint8_t language;
    int16_t speed;
    int16_t volume;
}lisa_tts_config_t;


/**
 * @brief	tts resource create func 
 *
 * @param params	user init params
 *
 * @retval  NULL failed NOT resource handler
 */
lisa_tts_handler_t *lisa_tts_create(const lisa_tts_params_t *params);
/**
 * @brief	start tts
 *
 * @param handler  tts handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_tts_exec(lisa_tts_handler_t *handler, const char *text);
int8_t lisa_tts_stop(lisa_tts_handler_t *handler);
/**
 * @brief	tts resource recycle func
 *
 * @param handler tts handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_tts_destroy(lisa_tts_handler_t *handler);

#endif //end of __LISA_TTS_H__
