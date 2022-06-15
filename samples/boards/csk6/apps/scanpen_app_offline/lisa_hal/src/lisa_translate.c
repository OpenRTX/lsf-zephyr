#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <zephyr.h>
#include <translate.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_thread.h>

#include "lisa_translate.h"

#define TAG "lisa_translate"

typedef enum {
	/* not init*/
	TRANSLATE_STATUS_NONE,
	/* init status*/
	TRANSLATE_STATUS_INIT
	/* start status*/
} translate_status_e;

struct _lisa_translate_context_t {
	/* translate status*/
	translate_status_e translate_st;
	/* translate callback*/
	//	lisa_translate_cb translatecb;
	/*translate data*/
	translate_t *translate;
};
static lisa_translate_handler_t *g_translate_handler = NULL;

/**
 * @brief get translate status
 *
 * @retval   status
 */
static translate_status_e _translate_get_status(const lisa_translate_context_t *ctx)
{
	// need mutex TODO
	return ctx->translate_st;
}

/**
 * @brief set translate status
 *
 * @param st	status to set
 */
static void _translate_set_status(lisa_translate_context_t *ctx, translate_status_e st)
{
	// need mutex TODO
	ctx->translate_st = st;
}

/**
 * @brief	translate resource create func
 *
 * @param params	user init params
 *
 * @retval  NULL failed  NOT resource handler
 */
lisa_translate_handler_t *lisa_translate_create(void)
{
	if (g_translate_handler != NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa translate handler is already created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "translate create\n");

	g_translate_handler = lisa_mem_alloc(sizeof(lisa_translate_handler_t));
	if (g_translate_handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_handler_malloc;
	}

	lisa_translate_context_t *ctx = lisa_mem_alloc(sizeof(lisa_translate_context_t));
	if (ctx == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_ctx_malloc;
	}
	memset(ctx, 0, sizeof(lisa_translate_context_t));

	ctx->translate = translate_create();
	if (ctx->translate == NULL) {
		LISA_LOGE(TAG, "[%s %d]trans_create failed", __FUNCTION__, __LINE__);
		goto _err_trans_create;
	}

	_translate_set_status(ctx, TRANSLATE_STATUS_INIT);

	g_translate_handler->ctx = ctx;

	return g_translate_handler;

_err_trans_create:
	lisa_mem_free(ctx);
_err_ctx_malloc:
	lisa_mem_free(g_translate_handler);
_err_handler_malloc:
_err_handler:
	return NULL;
}

/**
 * @brief	start translate
 *
 * @param handler  translate handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_translate_exec(lisa_translate_handler_t *handler, const char *text, uint32_t buffer,
		uint32_t size, uint32_t timoutms)
{
	int ret = 0;

	if (handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa translate handler hasn't been created!\n", __FUNCTION__,
				__LINE__);
		return -1;
	}
	LISA_LOGD(TAG, "Start translate\n");

	lisa_translate_context_t *ctx = handler->ctx;
	if (_translate_get_status(ctx) != TRANSLATE_STATUS_INIT) {
		LISA_LOGE(TAG, "[%s %d]translate not init", __FUNCTION__, __LINE__);
		return -1;
	}

	ret = translate_exec(ctx->translate, text, buffer, size, timoutms);
	if (ret < 0) {
		LISA_LOGE(TAG, "translate %s failed,iret %d.", text, ret);
		return -1;
	}

	LISA_LOGD(TAG, "translate success\n");
	LISA_LOGD(TAG, "[src]: %s\n", text);
	LISA_LOGD(TAG, "[dest]: %s\n", buffer);

	return 0;
}

/**
 * @brief	translate resource recycle func
 *
 * @param handler translate handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_translate_destroy(lisa_translate_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa translate handler hasn't been created!\n", __FUNCTION__,
				__LINE__);
		return -1;
	}
	LISA_LOGD(TAG, "Translage stop\n");

	lisa_translate_context_t *ctx = handler->ctx;

	if (_translate_get_status(ctx) != TRANSLATE_STATUS_INIT) {
		LISA_LOGE(TAG, "[%s %d]translate not init", __FUNCTION__, __LINE__);
		return -1;
	}

	translate_destroy(ctx->translate);

	lisa_mem_free(ctx);
	lisa_mem_free(handler);

	g_translate_handler = NULL;

	return 0;
}
