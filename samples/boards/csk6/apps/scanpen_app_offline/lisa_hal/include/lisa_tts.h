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
