#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <zephyr.h>
#include <tts_playback.h>
#include <avf/framework/avf_topology_def.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_thread.h>

#include "lisa_tts.h"

#define TAG "lisa_tts"

typedef enum {
	/* not init*/
	TTS_STATUS_NONE,
	/* init status*/
	TTS_STATUS_INIT,
	TTS_STATUS_START,
	TTS_STATUS_STOP = TTS_STATUS_INIT,
	/* start status*/
} tts_status_e;

struct _lisa_tts_context_t {
	/* tts status*/
	tts_status_e tts_st;
	/* tts callback*/
	lisa_tts_cb ttscb;
	/*tts data*/
	atts_t *tts;
};
static lisa_tts_handler_t *g_tts_handler = NULL;

/**
 * @brief get tts status
 *
 * @retval   status
 */
static tts_status_e _tts_get_status(const lisa_tts_context_t *ctx)
{
	// need mutex TODO
	return ctx->tts_st;
}

/**
 * @brief set tts status
 *
 * @param st	status to set
 */
static void _tts_set_status(lisa_tts_context_t *ctx, tts_status_e st)
{
	// need mutex TODO
	ctx->tts_st = st;
}

static void _tts_alg_event_notify(void *priv, avf_ctl_notify_event_msg_t *msg)
{
	lisa_tts_context_t *ctx = (lisa_tts_context_t *)priv;
	uint32_t copylen;
	
	printf("msg->body:%s\n", msg->body);
	if (!strcmp(msg->body, "ok")) {
		ctx->ttscb(TTS_START, NULL, 0);
	} else if (!strcmp(msg->body, "done")) {
		ctx->ttscb(TTS_PLAY_OVER, NULL, 0);
	}
}

/**
 * @brief	tts resource create func
 *
 * @param params	user init params
 *
 * @retval  NULL failed  NOT resource handler
 */
lisa_tts_handler_t *lisa_tts_create(const lisa_tts_params_t * const params)
{
	if (g_tts_handler != NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa tts handler is already created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "tts create\n");

	g_tts_handler = lisa_mem_alloc(sizeof(lisa_tts_handler_t));
	if (g_tts_handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_handler_malloc;
	}

	lisa_tts_context_t *ctx = lisa_mem_alloc(sizeof(lisa_tts_context_t));
	if (ctx == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_ctx_malloc;
	}
	memset(ctx, 0, sizeof(lisa_tts_context_t));

	ctx->tts = atts_create();
	if (ctx->tts == NULL) {
		LISA_LOGE(TAG, "[%s %d]tts_create failed", __FUNCTION__, __LINE__);
		goto _err_tts_create;
	}

	ctx->ttscb = params->ttscb;
	// atts_set_fmt(ctx->tts,  ctx);


	_tts_set_status(ctx, TTS_STATUS_INIT);

	g_tts_handler->ctx = ctx;

	return g_tts_handler;

_err_tts_create:
	lisa_mem_free(ctx);
_err_ctx_malloc:
	lisa_mem_free(g_tts_handler);
_err_handler_malloc:
_err_handler:
	return NULL;
}

/**
 * @brief	start tts
 *
 * @param handler  tts handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_tts_play(lisa_tts_handler_t *handler, const char *text, atts_config_t *config)
{
	int ret = 0;

	if (handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa tts handler hasn't been created!\n", __FUNCTION__,
				__LINE__);
		return -1;
	}

	if (text == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa tts text is null!\n", __FUNCTION__,
				__LINE__);
		return -1;
	}

	LISA_LOGD(TAG, "Start tts: %s\n", text);

	lisa_tts_context_t *ctx = handler->ctx;
	if (_tts_get_status(ctx) != TTS_STATUS_INIT) {
		LISA_LOGE(TAG, "[%s %d]tts not init", __FUNCTION__, __LINE__);
		return -1;
	}
	atts_set_fmt(ctx->tts,  ctx);
	atts_start(ctx->tts);
	k_msleep(100);
#if 1
	atts_set_cmd(ctx->tts, config, sizeof(atts_config_t));
	k_msleep(50);
#endif
	ret = atts_play(ctx->tts, text, strlen(text));
	if (ret < 0) {
		LISA_LOGE(TAG, "tts %s failed,iret %d.", text, ret);
		return -1;
	}

	avf_ctl_notify_event_register(ctx->tts->avf->stream->platform,AVF_CTL_COMP_NAME_ATTS, AVF_CTL_MSGID_TTS, _tts_alg_event_notify, ctx); 
	_tts_set_status(ctx, TTS_STATUS_START);

	return 0;
}

/**
 * @brief	start tts
 *
 * @param handler  tts handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_tts_stop(lisa_tts_handler_t *handler)
{
	int ret = 0;

	if (handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa tts handler hasn't been created!\n", __FUNCTION__,
				__LINE__);
		return -1;
	}

	LISA_LOGD(TAG, "Stop tts\n");

	lisa_tts_context_t *ctx = handler->ctx;
	if (_tts_get_status(ctx) != TTS_STATUS_START) {
		LISA_LOGE(TAG, "[%s %d]tts not init", __FUNCTION__, __LINE__);
		return -1;
	}

	atts_stop(ctx->tts);

	_tts_set_status(ctx, TTS_STATUS_STOP);

	return 0;
}


/**
 * @brief	tts resource recycle func
 *
 * @param handler tts handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_tts_destroy(lisa_tts_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa tts handler hasn't been created!\n", __FUNCTION__,
				__LINE__);
		return -1;
	}
	LISA_LOGD(TAG, "tts destroy\n");

	lisa_tts_context_t *ctx = handler->ctx;

	if ((_tts_get_status(ctx) != TTS_STATUS_STOP) &&
		(_tts_get_status(ctx) != TTS_STATUS_INIT)) {
		LISA_LOGE(TAG, "[%s %d]tts not init", __FUNCTION__, __LINE__);
		return -1;
	}

	atts_destroy(ctx->tts);

	lisa_mem_free(ctx);
	lisa_mem_free(handler);

	g_tts_handler = NULL;

	return 0;
}
