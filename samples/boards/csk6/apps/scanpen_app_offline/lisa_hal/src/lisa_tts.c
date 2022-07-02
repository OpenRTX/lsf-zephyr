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

#define SOUND_ATTS_EVENT_THRES (1024)
#define SND_ATTS_DATA_MSGQ_DEPTH (128)
#define SOUND_ATTS_NUMBER (1)

typedef enum {
	/* not init*/
	TTS_STATUS_NONE,
	/* init status*/
	TTS_STATUS_INIT,
	TTS_STATUS_START,
	TTS_STATUS_STOP = TTS_STATUS_INIT,
	/* start status*/
} tts_status_e;

typedef struct {
	/*msg queue data pointer*/
	void *pdata;
	/*msg queue data length*/
	int datalen;
} snd_atts_msgq_data_t;

struct _lisa_tts_context_t {
	/*exit flag of acapture process thread*/
	volatile uint32_t isruning;
	/* tts status*/
	tts_status_e tts_st;
	/* tts callback*/
	lisa_tts_cb ttscb;
	/*pid fo acapture process thread*/
	lisa_thread_t *tid;
	/*msg queue for atts data*/
	struct k_msgq msgq;
	snd_atts_msgq_data_t *msgq_databuf;
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
	
	printk("msg->len:%d\n", msg->len);
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

	ctx->msgq_databuf =
			lisa_mem_calloc(SND_ATTS_DATA_MSGQ_DEPTH, sizeof(snd_atts_msgq_data_t));
	if (ctx->msgq_databuf == NULL) {
		LISA_LOGE(TAG, "[%s %d]msgq_databuf malloc failed", __FUNCTION__, __LINE__);
		goto _err_tts_create;
	}
	k_msgq_init(&ctx->msgq, ctx->msgq_databuf, sizeof(snd_atts_msgq_data_t),
			SND_ATTS_DATA_MSGQ_DEPTH);

	ctx->tts = atts_create();
	if (ctx->tts == NULL) {
		LISA_LOGE(TAG, "[%s %d]tts_create failed", __FUNCTION__, __LINE__);
		goto _err_tts_create;
	}

	ctx->ttscb = params->ttscb;
	atts_set_fmt(ctx->tts,  ctx);

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
 * @brief atts data process for ACAP EVENT callback
 *
 * @param priv user data
 * @param msg one of msgqueue
 */
static void _atts_data_process(void *priv, atts_event_msg_t msg)
{
	lisa_tts_context_t *ctx = (lisa_tts_context_t *)priv;
	snd_atts_msgq_data_t msgq_data = {0};
	int ret = 0;
	if (msg.event_flags & AVF_STREAM_EVENT_THRES_NOTIFY) {
		// if(msg.value.value > 32*1024) return;
		const uint32_t avail = msg.value.value;
		msgq_data.pdata = lisa_mem_alloc(sizeof(char) * avail);
		if (msgq_data.pdata == NULL) {
			LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
			return;
		}

		const uint32_t readlen = atts_get_datai(ctx->tts, msgq_data.pdata, avail);
		if (readlen > 0) {
			msgq_data.datalen = readlen;
			ret = k_msgq_put(&ctx->msgq, &msgq_data, K_NO_WAIT);
			if (ret) {
				LISA_LOGE(TAG, "[%s %d]msgq message dropped\n", __FUNCTION__, __LINE__);
			}
		}
	}
}

/**
 * @brief	atts process main thread
 *
 * @param arg1	user data pointer1
 * @param arg2	user data pointer2
 * @param arg3	user data pointer3
 */
static void _atts_thread_process(void *arg1, void *arg2, void *arg3)
{
	lisa_tts_context_t *ctx = (lisa_tts_context_t *)arg1;
	lisa_thread_t *tid = ctx->tid;
	snd_atts_msgq_data_t msgq;
	int32_t ret;

	ctx->isruning = 1;

	while (ctx->isruning) {
		ret = k_msgq_get(&ctx->msgq, &msgq, K_MSEC(500));
		if (ret != 0) {

			break;
		}

		ctx->ttscb(TTS_AUDIO, msgq.pdata, msgq.datalen);

		lisa_mem_free(msgq.pdata);
	}

	// ctx->ttscb(TTS_PLAY_OVER, NULL, 0);

	k_thread_abort(tid);
}

/**
 * @brief	start tts
 *
 * @param handler  tts handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_tts_start(lisa_tts_handler_t *handler, const char *text)
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

	ret = atts_event_register(ctx->tts, AVF_STREAM_EVENT_THRES_NOTIFY,
			_atts_data_process, ctx, SOUND_ATTS_EVENT_THRES);
	if (ret != 0) {
		LISA_LOGE(TAG, "[%s %d]atts_event_register failed\n", __FUNCTION__, __LINE__);
		goto _err_atts_event;
	}

	atts_start(ctx->tts);
	k_msleep(100);
	ret = atts_play(ctx->tts, text, strlen(text));
	if (ret < 0) {
		LISA_LOGE(TAG, "tts %s failed,iret %d.", text, ret);
		return -1;
	}

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "_acapture_thread_process";
	thread_attr.stack_size = 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL
	ctx->tid = lisa_thread_create(&thread_attr, _atts_thread_process, ctx);
	if (ctx->tid == NULL) {
		LISA_LOGE(TAG, "[%s %d]k_thread_create acapture thread failed\n", __FUNCTION__, __LINE__);
		goto _err_thread_cre;
	}

	avf_ctl_notify_event_register(ctx->tts->avf->stream->platform,AVF_CTL_COMP_NAME_ATTS, AVF_CTL_MSGID_TTS, _tts_alg_event_notify, ctx);
	_tts_set_status(ctx, TTS_STATUS_START);

	return 0;
_err_thread_cre:
	if (ctx->tts) atts_stop(ctx->tts);
_err_atts_start:
	if (ctx->tts) atts_event_unregister(ctx->tts, AVF_STREAM_EVENT_THRES_NOTIFY);
_err_atts_event:
_err_status:
_err_handler:
	return -1;
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

	ctx->isruning = 0;
	k_thread_join(ctx->tid, K_FOREVER);

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
