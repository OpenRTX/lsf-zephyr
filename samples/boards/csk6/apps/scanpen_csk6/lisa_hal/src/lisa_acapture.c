#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <zephyr.h>
#include <fs/fs.h>
#include <acapture.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_thread.h>

#include "lisa_acapture.h"

#define TAG "lisa_acapture"

#define DEFAULT_CAPTURE_FILE_NAME "/NAND:/acapture_demo.pcm"

#define SOUND_CAPTURE_FILE_MAXSIZE (160000)
#define SOUND_CAPTURE_EVENT_THRES (1024)
#define SND_ACAPTURE_DATA_MSGQ_DEPTH (128)
#define SOUND_CAPTURE_NUMBER (1)

typedef enum {
	/* not init*/
	ACAPTURE_STATUS_NONE,
	/* init status*/
	ACAPTURE_STATUS_INIT,
	/* start status*/
	ACAPTURE_STATUS_START,
	/* stop status*/
	ACAPTURE_STATUS_STOP = ACAPTURE_STATUS_INIT,
} acapture_status_e;

typedef struct {
	/*msg queue data pointer*/
	void *pdata;
	/*msg queue data length*/
	int datalen;
} snd_acapture_msgq_data_t;

struct _lisa_acapture_context_t {
	/* acapture status*/
	acapture_status_e acapture_st;
	/* acapture format*/
	amedia_fmt_t fmt;
	/* acapture callback*/
	lisa_acapture_cb acapturecb;
	/*log record data to file for debug*/
	uint8_t log_to_file; /*0-close 1-open*/
	char filename[64]; /*need set full path, default: /NAND/arerocd_log.pcm*/
	/*pid fo acapture process thread*/
	lisa_thread_t *tid;
	/*exit flag of acapture process thread*/
	volatile uint32_t isruning;
	/*msg queue for record data*/
	struct k_msgq msgq;
	snd_acapture_msgq_data_t *msgq_databuf;
	/*acapture data*/
	acapture_t *acapture;
};

static lisa_acapture_handler_t *g_acapture_handler = NULL;
static K_THREAD_STACK_DEFINE(g_thread_stack, 2048);
static struct k_thread g_lisa_acapture_thread;

/**
 * @brief dump acapture params only for debug
 *
 * @param param handler pointer
 */
static void _dump_params(const lisa_acapture_context_t *ctx)
{
	printk("rate: %d\n", ctx->fmt.rate);
	printk("bits: %d\n", ctx->fmt.bits);
	printk("channels: %d\n", ctx->fmt.channels);
	printk("log_to_file: %d\n", ctx->log_to_file);
	printk("filename: %s\n", ctx->filename);
}

/**
 * @brief get acapture status
 *
 * @retval   status
 */
static acapture_status_e _acapture_get_status(const lisa_acapture_context_t *ctx)
{
	// need mutex TODO
	return ctx->acapture_st;
}

/**
 * @brief set acapture status
 *
 * @param st	status to set
 */
static void _acapture_set_status(lisa_acapture_context_t *ctx, acapture_status_e st)
{
	// need mutex TODO
	ctx->acapture_st = st;
}

/**
 * @brief acap data process for ACAP EVENT callback
 *
 * @param priv user data
 * @param msg one of msgqueue
 */
static void _acapture_data_process(void *priv, acapture_event_msg_t msg)
{
	lisa_acapture_context_t *ctx = (lisa_acapture_context_t *)priv;
	snd_acapture_msgq_data_t msgq_data = {0};
	int ret = 0;

	if (msg.event_flags & AVF_STREAM_EVENT_THRES_NOTIFY) {
		const uint32_t avail = msg.value.value;
		msgq_data.pdata = lisa_mem_alloc(sizeof(char) * avail);
		if (msgq_data.pdata == NULL) {
			LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
			return;
		}

		const uint32_t readlen = acap_get_datai(ctx->acapture, msgq_data.pdata, avail);
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
 * @brief	acapture process main thread
 *
 * @param arg1	user data pointer1
 * @param arg2	user data pointer2
 * @param arg3	user data pointer3
 */
static void _acapture_thread_process(void *arg1, void *arg2, void *arg3)
{
	lisa_acapture_context_t *ctx = (lisa_acapture_context_t *)arg1;
	lisa_thread_t *tid = ctx->tid;
	struct fs_file_t file;
	snd_acapture_msgq_data_t msgq;
	int32_t ret;
	int32_t rc;
	uint32_t u32cnt;
	char filename[64];
	struct fs_dirent dirent;

	ctx->isruning = 1;

	_dump_params(ctx);

	if (ctx->log_to_file) {
		strcpy(filename, ctx->filename);
		fs_file_t_init(&file);
		if (0 >= fs_stat(filename, &dirent)) {
			fs_unlink(filename);
		}

		rc = fs_open(&file, filename, FS_O_RDWR | FS_O_CREATE);
		if (rc < 0) {
			LISA_LOGE(TAG, "%s:%d, failed to open file '%s'", __func__, __LINE__, filename);
			// goto CAPTURE_END;
		}
		LISA_LOGD(TAG, "[%s %d]Sound capture %d start,save to file %s!\n", __FUNCTION__, __LINE__,
				u32cnt, filename);
	}

	while (ctx->isruning) {
		ret = k_msgq_get(&ctx->msgq, &msgq, K_MSEC(500));
		if (ret != 0) {
			break;
		}

		ctx->acapturecb(ACAPTURE_DATA, msgq.pdata, msgq.datalen);
		if (ctx->log_to_file) {
			rc = fs_write(&file, msgq.pdata, msgq.datalen);
			if (rc != msgq.datalen) {
				LISA_LOGE(
						TAG, "[%s %d]failed to write file '%s'", __FUNCTION__, __LINE__, filename);
				/*//break;*/
			}
		}

		/*
		 *        if ((g_acapture_handler->total_size >= SOUND_CAPTURE_FILE_MAXSIZE)&&(isruning ==
		 * 1)) {
		 *            acap_stop(priv_acapture);
		 *            isruning = 0;
		 *            LISA_LOGD(TAG,"[%s %d]Get data total sie %d,Stop
		 * Capture!\n",__FUNCTION__,__LINE__,_handler->total_size);
		 *
		 *        }
		 *
		 */
		lisa_mem_free(msgq.pdata);
	}

	if (ctx->log_to_file) {
		fs_close(&file);
	}

	LISA_LOGD(TAG, "[%s %d]Sound capture %d over,close %s file!\n", __FUNCTION__, __LINE__, u32cnt,
			filename);

	k_thread_abort(tid);
}

/**
 * @brief	acapture resource create func
 *
 * @param params	user init params
 *
 * @retval  NULL failed  NOT resource handler
 */
lisa_acapture_handler_t *lisa_acapture_create(const lisa_acapture_params_t *const params)
{
	if (g_acapture_handler != NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa acapture handler is already created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}

	if ((params->fmt.rate != 16000) || (params->fmt.channels != 1) || (params->fmt.bits != 16) ||
			(params->acapturecb == NULL)) {
		LISA_LOGE(TAG, "[%s %d]acapture fmt is unsupport! now only support 16000 1 16\n",
				__FUNCTION__, __LINE__);
		goto _err_fmt;
	}
	LISA_LOGD(TAG, "acapture create\n");

	g_acapture_handler = lisa_mem_alloc(sizeof(lisa_acapture_handler_t));
	if (g_acapture_handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_handler_malloc;
	}

	lisa_acapture_context_t *ctx = lisa_mem_alloc(sizeof(lisa_acapture_context_t));
	if (ctx == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_ctx_malloc;
	}
	memset(ctx, 0, sizeof(lisa_acapture_context_t));

	ctx->fmt.rate = params->fmt.rate;
	ctx->fmt.channels = params->fmt.channels;
	ctx->fmt.bits = params->fmt.bits;
	ctx->acapturecb = params->acapturecb;

	ctx->log_to_file = params->log_to_file;
	if (ctx->log_to_file == 1) {
		strcpy(ctx->filename,
				(params->filename == NULL) ? DEFAULT_CAPTURE_FILE_NAME : params->filename);
		LISA_LOGE(TAG, "[%s %d]log record file to %s", __FUNCTION__, __LINE__, ctx->filename);
	}

	//_dump_params(ctx);

	ctx->msgq_databuf =
			lisa_mem_calloc(SND_ACAPTURE_DATA_MSGQ_DEPTH, sizeof(snd_acapture_msgq_data_t));
	if (ctx->msgq_databuf == NULL) {
		LISA_LOGE(TAG, "[%s %d]msgq_databuf malloc failed", __FUNCTION__, __LINE__);
		goto _err_msgq_malloc;
	}
	k_msgq_init(&ctx->msgq, ctx->msgq_databuf, sizeof(snd_acapture_msgq_data_t),
			SND_ACAPTURE_DATA_MSGQ_DEPTH);

	ctx->acapture = acap_create();
	if (ctx->acapture == NULL) {
		LISA_LOGE(TAG, "[%s %d]acap_create failed", __FUNCTION__, __LINE__);
		goto _err_acap_create;
	}

	_acapture_set_status(ctx, ACAPTURE_STATUS_INIT);

	g_acapture_handler->ctx = ctx;

	return g_acapture_handler;

_err_acap_create:
	lisa_mem_free(ctx->msgq_databuf);
_err_msgq_malloc:
	lisa_mem_free(ctx);
_err_ctx_malloc:
	lisa_mem_free(g_acapture_handler);
_err_handler_malloc:
_err_handler:
_err_fmt:
	return NULL;
}

/**
 * @brief	start acapture
 *
 * @param handler  acapture handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_start(lisa_acapture_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa acapture handler hasn't been created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "Start acapture\n");

	lisa_acapture_context_t *ctx = handler->ctx;
	if (_acapture_get_status(ctx) != ACAPTURE_STATUS_INIT) {
		LISA_LOGE(TAG, "[%s %d]acapture not init", __FUNCTION__, __LINE__);
		goto _err_status;
	}

	int ret = acap_event_register(ctx->acapture, AVF_STREAM_EVENT_THRES_NOTIFY,
			_acapture_data_process, ctx, SOUND_CAPTURE_EVENT_THRES);
	if (ret != 0) {
		LISA_LOGE(TAG, "[%s %d]acap_event_register failed\n", __FUNCTION__, __LINE__);
		goto _err_acap_event;
	}
	acap_set_fmt(ctx->acapture, &ctx->fmt);

	if (acap_start(ctx->acapture) != 0) {
		LISA_LOGE(TAG, "[%s %d]acap_event_register failed\n", __FUNCTION__, __LINE__);
		goto _err_acap_start;
	}

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "_acapture_thread_process";
	thread_attr.stack_size = 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL
	ctx->tid = lisa_thread_create(&thread_attr, _acapture_thread_process, ctx);
	if (ctx->tid == NULL) {
		LISA_LOGE(TAG, "[%s %d]k_thread_create acapture thread failed\n", __FUNCTION__, __LINE__);
		goto _err_thread_cre;
	}

	_acapture_set_status(ctx, ACAPTURE_STATUS_START);
	ctx->acapturecb(ACAPTURE_START, NULL, 0);

	return 0;
_err_thread_cre:
	if (ctx->acapture) acap_stop(ctx->acapture);
_err_acap_start:
	if (ctx->acapture) acap_event_unregister(ctx->acapture, AVF_STREAM_EVENT_THRES_NOTIFY);
_err_acap_event:
_err_status:
_err_handler:
	return -1;
}

/**
 * @brief	acapture stop record func
 *
 * @param handler	acapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_stop(lisa_acapture_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa acapture handler hasn't been created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "Stop acapture\n");

	lisa_acapture_context_t *ctx = handler->ctx;
	if (_acapture_get_status(ctx) != ACAPTURE_STATUS_START) {
		LISA_LOGE(TAG, "[%s %d]acapture not start", __FUNCTION__, __LINE__);
		goto _err_status;
	}

	ctx->isruning = 0;
	k_thread_join(ctx->tid, K_FOREVER);

	acap_stop(ctx->acapture);
	acap_event_unregister(ctx->acapture, AVF_STREAM_EVENT_THRES_NOTIFY);

	_acapture_set_status(ctx, ACAPTURE_STATUS_STOP);
	ctx->acapturecb(ACAPTURE_STOP, NULL, 0);

	return 0;
_err_status:
_err_handler:
	return -1;
}

/**
 * @brief	set acapture params
 *
 * @param handler	acapture handler pointer
 * @param param		type of param to set
 * @param data		data pointer to set
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_setparam(
		lisa_acapture_handler_t *handler, const lisa_acapture_param_e param, const void *data)
{
	// TODO
	LISA_LOGE(TAG, "%s unsupport now\n", __FUNCTION__);
	return 0;
}

/**
 * @brief	get acapture params
 *
 * @param handler	acapture handler pointer
 * @param param		type of param to get
 * @param data		data pointer to save for return
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_getparam(
		lisa_acapture_handler_t *handler, const lisa_acapture_param_e param, void **data)
{
	// TODO
	LISA_LOGE(TAG, "%s unsupport now\n", __FUNCTION__);
	return 0;
}

/**
 * @brief	acapture resource recycle func
 *
 * @param handler acapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_acapture_destroy(lisa_acapture_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa acapture handler hasn't been created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "Start stop\n");

	lisa_acapture_context_t *ctx = handler->ctx;

	if ((_acapture_get_status(ctx) != ACAPTURE_STATUS_STOP) &&
			(_acapture_get_status(ctx) != ACAPTURE_STATUS_INIT)) {
		LISA_LOGE(TAG, "[%s %d]acapture not init", __FUNCTION__, __LINE__);
		goto _err_status;
	}

	acap_destroy(ctx->acapture);

	lisa_mem_free(ctx->msgq_databuf);
	lisa_mem_free(ctx);
	lisa_mem_free(handler);

	g_acapture_handler = NULL;

	return 0;
_err_status:
_err_handler:
	return -1;
}
