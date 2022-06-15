/*
 * ================================================================
 *
 *			Filename:	lisa_acapture.h
 *
 *		 Description:	lisa_acapture header file
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
#ifndef __LISA_ACAPTURE_H__
#define __LISA_ACAPTURE_H__

/**
 * @brief lisa acapture param enum
 */
typedef enum {
	/*set acapture fmt*/
	LISA_AUDIO_FMT,
	/*set acapture volume*/
	LISA_AUDIO_VOLUME,
	//TODO
} lisa_acapture_param_e;

/**
 * @brief lisa arecord msg
 * These msg can be obtained through  lisa_acapture_cb callback
 */
typedef enum {
	/*when acapture start to record, callbak this msg*/
	ACAPTURE_START = 0, 
	/*when record is stoped, callbak this msg*/
	ACAPTURE_STOP,
	/*when arecod is started , the record data callbak from this msg*/
	ACAPTURE_DATA,
	/*reserved*/
	ACAPTURE_ERR,
} lisa_acapture_cbmsg_e;

/**
 * @brief lisa acapture fmt struct
 */
typedef struct {
	/*acapture sample channels*/
    uint8_t channels;
	/*acapture sample bits*/
    uint8_t bits;
	/*acapture sample rate*/
    uint16_t rate;
} lisa_acapture_fmt_t;

/**
 * @brief 
 *
 * @param msg	callbak msg
 * @param data  record data pointer
 * @param len	record len
 *
 * @retval  null
 */
typedef void (*lisa_acapture_cb)(lisa_acapture_cbmsg_e msg, void * data, uint32_t len);

/**
 * @brief  lisat acapture params struct
 */
typedef struct {
/*log record data to file for debug*/
	char log_to_file; /*0-close 1-open*/
	char filename[64];  /*need set full path, default: /NAND/arecord_log.pcm*/
	/*set record frame size*/
	uint16_t periodsize;//default 1024
	/*acapture msg process callbak*/
	lisa_acapture_cb acapturecb;
	/*set acapture fmt*/
    lisa_acapture_fmt_t fmt;
	//TODO
} lisa_acapture_params_t;

/**
 * @brief err num ,reserved
 */
typedef enum {
	LISA_AUDIO_OK = 0,
	LISA_AUDIO_FAIL,
} lisa_acapture_err_e;


struct _lisa_acapture_context_t;
typedef struct _lisa_acapture_context_t lisa_acapture_context_t;

/**
 * @brief  lisa acapture handler
 */
typedef struct {
	lisa_acapture_params_t params;
	lisa_acapture_context_t *ctx;
} lisa_acapture_handler_t;

/**
 * @brief	acapture resource create func 
 *
 * @param params	user init params
 *
 * @retval  NULL failed NOT resource handler
 */
lisa_acapture_handler_t * lisa_acapture_create(const lisa_acapture_params_t *params);
/**
 * @brief	start acapture
 *
 * @param handler  acapture handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_start(lisa_acapture_handler_t *handler);
/**
 * @brief	acapture stop record func
 *
 * @param handler	acapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_stop(lisa_acapture_handler_t *handler);
/**
 * @brief	set acapture params
 *
 * @param handler	acapture handler pointer
 * @param param		type of param to set
 * @param data		data pointer to set
 *
 * @retval  -1 failed 0 success 
 */
int8_t lisa_acapture_setparam(lisa_acapture_handler_t *handler, const lisa_acapture_param_e param, const void *data);
/**
 * @brief	get acapture params
 *
 * @param handler	acapture handler pointer
 * @param param		type of param to get
 * @param data		data pointer to save for return
 *
 * @retval  -1 failed 0 success 
 */
int8_t lisa_acapture_getparam(lisa_acapture_handler_t *handler, const lisa_acapture_param_e param, void **data);
/**
 * @brief	acapture resource recycle func
 *
 * @param handler acapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_destroy(lisa_acapture_handler_t *handler);

#endif //end of __LISA_ACAPTURE_H__
