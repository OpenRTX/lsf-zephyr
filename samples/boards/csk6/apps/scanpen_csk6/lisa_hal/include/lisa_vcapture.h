/*
 * ================================================================
 *
 *			Filename:	lisa_vcapture.h
 *
 *		 Description:	lisa_vcapture header file
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
#ifndef __LISA_VCAPTUREER_H__
#define __LISA_VCAPTUREER_H__

/**
 * @brief lisa vcapture param enum
 */
typedef enum {
	/*set vcapture fmt*/
	LISA_AUDIO_FMT,
	/*set vcapture volume*/
	LISA_AUDIO_VOLUME,
	//TODO
} lisa_vcapture_param_e;

/**
 * @brief lisa arecord msg
 * These msg can be obtained through  lisa_vcapture_cb callback
 */
typedef enum {
	/*when vcapture start to record, callbak this msg*/
	VCAPTURE_START = 0,
	/*when record is stoped, callbak this msg*/
	VCAPTURE_STOP,
	/*when arecod is started , the record data callbak from this msg*/
	VCAPTURE_DATA,
	/*reserved*/
	VCAPTURE_ERR,
} lisa_vcapture_cbmsg_e;

/**
 * @brief lisa vcapture fmt struct
 */
typedef struct {
	/*vcapture sample channels*/
	uint8_t channels;
	/*vcapture sample bits*/
	uint8_t bits;
	/*vcapture sample rate*/
	uint16_t rate;
} lisa_vcapture_fmt_t;

/**
 * @brief 
 *
 * @param msg	callbak msg
 * @param data  record data pointer
 * @param len	record len
 *
 * @retval  null
 */
typedef void (*lisa_vcapture_cb)(lisa_vcapture_cbmsg_e msg, void *data, uint32_t len);

/**
 * @brief  lisat vcapture params struct
 */
typedef struct {
	/*log record data to file for debug*/
	char log_to_file; /*0-close 1-open*/
	char filename[64]; /*need set full path, default: /NAND/arecord_log.pcm*/
	/*set record frame size*/
	uint16_t periodsize; //default 1024
	/*vcapture msg process callbak*/
	lisa_vcapture_cb vcapturecb;
	/*set vcapture fmt*/
	lisa_vcapture_fmt_t fmt;
	//TODO
} lisa_vcapture_params_t;

/**
 * @brief err num ,reserved
 */
typedef enum {
	LISA_AUDIO_OK = 0,
	LISA_AUDIO_FAIL,
} lisa_vcapture_err_e;

struct _lisa_vcapture_context_t;
typedef struct _lisa_vcapture_context_t lisa_vcapture_context_t;

/**
 * @brief  lisa vcapture handler
 */
typedef struct {
	lisa_vcapture_params_t params;
	lisa_vcapture_context_t *ctx;
} lisa_vcapture_handler_t;

/**
 * @brief	vcapture resource create func 
 *
 * @param params	user init params
 *
 * @retval  NULL failed NOT resource handler
 */
lisa_vcapture_handler_t *lisa_vcapture_create(const lisa_vcapture_params_t *params);
/**
 * @brief	start vcapture
 *
 * @param handler  vcapture handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_vcapture_start(lisa_vcapture_handler_t *handler);
/**
 * @brief	vcapture stop record func
 *
 * @param handler	vcapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_vcapture_stop(lisa_vcapture_handler_t *handler);
/**
 * @brief	vcapture resource recycle func
 *
 * @param handler vcapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_vcapture_destroy(lisa_vcapture_handler_t *handler);

#endif //end of __LISA_VCAPTUREER_H__
