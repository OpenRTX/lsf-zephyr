/*
 * ================================================================
 *
 *			Filename:	lisa_translate.h
 *
 *		 Description:	lisa_translate header file
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
#ifndef __LISA_translate_H__
#define __LISA_translate_H__


struct _lisa_translate_context_t;
typedef struct _lisa_translate_context_t lisa_translate_context_t;

/**
 * @brief  lisa translate handler
 */
typedef struct {
	lisa_translate_context_t *ctx;
} lisa_translate_handler_t;

/**
 * @brief	translate resource create func 
 *
 * @param params	user init params
 *
 * @retval  NULL failed NOT resource handler
 */
lisa_translate_handler_t *lisa_translate_create(void);
/**
 * @brief	start translate
 *
 * @param handler  translate handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_translate_exec(lisa_translate_handler_t *handler, const char *text, uint32_t buffer,
		uint32_t size, uint32_t timoutms);
/**
 * @brief	stop translate
 *
 * @param handler  translate handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_translate_stop(lisa_translate_handler_t *handler);
/**
 * @brief	translate resource recycle func
 *
 * @param handler translate handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_translate_destroy(lisa_translate_handler_t *handler);

#endif //end of __LISA_translate_H__
