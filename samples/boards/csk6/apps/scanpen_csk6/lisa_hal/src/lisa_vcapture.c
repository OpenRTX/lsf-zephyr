#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <zephyr.h>
#include <fs/fs.h>
#include <acapture.h>
#include <vcapture.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_thread.h>

#include "lisa_vcapture.h"

#define TAG "lisa_vcapture"

#define DEFAULT_CAPTURE_FILE_NAME "/NAND:/vcapture_demo.pcm"

#define IMAGE_CAPTURE_EVENT_THRES (1024)
#define IMAGE_VCAPTURE_DATA_MSGQ_DEPTH (1024)
#define VCAPTURE_MAX_PACKET_SIZE (1024)

typedef enum {
	/* not init*/
	VCAPTURE_STATUS_NONE,
	/* init status*/
	VCAPTURE_STATUS_INIT,
	/* start status*/
	VCAPTURE_STATUS_START,
	/* stop status*/
	VCAPTURE_STATUS_STOP = VCAPTURE_STATUS_INIT,
} vcapture_status_e;

typedef struct {
	/*msg queue data pointer*/
	void *pdata;
	/*msg queue data length*/
	int datalen;
} vcapture_msgq_data_t;

struct _lisa_vcapture_context_t {
	/* vcapture status*/
	vcapture_status_e vcapture_st;
	/* vcapture format*/
	amedia_fmt_t fmt;
	/* vcapture callback*/
	lisa_vcapture_cb vcapturecb;
	/*log record data to file for debug*/
	uint8_t log_to_file; /*0-close 1-open*/
	char filename[64]; /*need set full path, default: /NAND/arerocd_log.pcm*/
	/*pid fo vcapture process thread*/
	lisa_thread_t *tid;
	/*exit flag of vcapture process thread*/
	volatile uint32_t isruning;
	/*msg queue for record data*/
	struct k_msgq msgq;
	vcapture_msgq_data_t *msgq_databuf;
	/*vcapture data*/
	vcapture_t *vcapture;
};

static lisa_vcapture_handler_t *g_vcapture_handler = NULL;

/**
 * @brief dump vcapture params only for debug
 *
 * @param param handler pointer
 */
static void _dump_params(const lisa_vcapture_context_t *ctx)
{
	printk("rate: %d\n", ctx->fmt.rate);
	printk("bits: %d\n", ctx->fmt.bits);
	printk("channels: %d\n", ctx->fmt.channels);
	printk("log_to_file: %d\n", ctx->log_to_file);
	printk("filename: %s\n", ctx->filename);
}

/**
 * @brief get vcapture status
 *
 * @retval   status
 */
static vcapture_status_e _vcapture_get_status(const lisa_vcapture_context_t *ctx)
{
	// need mutex TODO
	return ctx->vcapture_st;
}

/**
 * @brief set vcapture status
 *
 * @param st	status to set
 */
static void _vcapture_set_status(lisa_vcapture_context_t *ctx, vcapture_status_e st)
{
	// need mutex TODO
	ctx->vcapture_st = st;
}

/**
 * @brief vcap data process for ACAP EVENT callback
 *
 * @param priv user data
 * @param msg one of msgqueue
 */
static void _vcapture_data_process(void *priv, vcapture_event_msg_t msg)
{
	lisa_vcapture_context_t *ctx = (lisa_vcapture_context_t *)priv;
	vcapture_msgq_data_t msgq_data = {0};
	int ret = 0;
	int32_t readlen = 0;
	uint32_t reqlen = 0;
	uint32_t avail = 0;

	if (msg.event_flags & AVF_STREAM_EVENT_THRES_NOTIFY) {
		avail = msg.value.value;
		while (avail > 0) {
			reqlen = avail > VCAPTURE_MAX_PACKET_SIZE ? VCAPTURE_MAX_PACKET_SIZE : avail;

			msgq_data.pdata = lisa_mem_alloc(sizeof(char) * reqlen);  //有泄露风险
			if (msgq_data.pdata == NULL) {
				LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
				return;
			}
			readlen = vcap_get_datai(ctx->vcapture, msgq_data.pdata, reqlen);

			if (readlen > 0) {
				msgq_data.datalen = readlen;
				ret = k_msgq_put(&ctx->msgq, &msgq_data, K_NO_WAIT);
				if (ret) {
					LISA_LOGE(TAG, "[%s %d]msgq message dropped\n", __FUNCTION__, __LINE__);
					break;
				}
			} else {
				lisa_mem_free(msgq_data.pdata);
				break;
			}

			avail -= readlen;
		}
	}
}

/**
 * @brief	vcapture process main thread
 *
 * @param arg1	user data pointer1
 * @param arg2	user data pointer2
 * @param arg3	user data pointer3
 */
static void _vcapture_thread_process(void *arg1, void *arg2, void *arg3)
{
	lisa_vcapture_context_t *ctx = (lisa_vcapture_context_t *)arg1;
	lisa_thread_t *tid = ctx->tid;
	struct fs_file_t file;
	vcapture_msgq_data_t msgq;
	int32_t ret;
	int32_t rc;
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
		LISA_LOGD(TAG, "[%s %d]Sound capture start,save to file %s!\n", __FUNCTION__, __LINE__,
				filename);
	}

	while (ctx->isruning) {
		ret = k_msgq_get(&ctx->msgq, &msgq, K_FOREVER);
		if (ret != 0) {
			break;
		}

		ctx->vcapturecb(VCAPTURE_DATA, msgq.pdata, msgq.datalen);
		if (ctx->log_to_file) {
			rc = fs_write(&file, msgq.pdata, msgq.datalen);
			if (rc != msgq.datalen) {
				LISA_LOGE(
						TAG, "[%s %d]failed to write file '%s'", __FUNCTION__, __LINE__, filename);
				/*//break;*/
			}
		}

		lisa_mem_free(msgq.pdata);
	}

	if (ctx->log_to_file) {
		fs_close(&file);
	}

	LISA_LOGD(TAG, "[%s %d]Sound capture over,close %s file!\n", __FUNCTION__, __LINE__, filename);

	/*k_thread_abort(tid);*/
}

/**
 * @brief	vcapture resource create func
 *
 * @param params	user init params
 *
 * @retval  NULL failed  NOT resource handler
 */
lisa_vcapture_handler_t *lisa_vcapture_create(const lisa_vcapture_params_t *const params)
{
	if (g_vcapture_handler != NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa vcapture handler is already created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}

	if ((params->fmt.rate != 16000) || (params->fmt.channels != 1) || (params->fmt.bits != 16) ||
			(params->vcapturecb == NULL)) {
		LISA_LOGE(TAG, "[%s %d]vcapture fmt is unsupport! now only support 16000 1 16\n",
				__FUNCTION__, __LINE__);
		goto _err_fmt;
	}
	LISA_LOGD(TAG, "vcapture create\n");

	g_vcapture_handler = lisa_mem_alloc(sizeof(lisa_vcapture_handler_t));
	if (g_vcapture_handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_handler_malloc;
	}

	lisa_vcapture_context_t *ctx = lisa_mem_alloc(sizeof(lisa_vcapture_context_t));
	if (ctx == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!\n", __FUNCTION__, __LINE__);
		goto _err_ctx_malloc;
	}
	memset(ctx, 0, sizeof(lisa_vcapture_context_t));

	ctx->fmt.rate = params->fmt.rate;
	ctx->fmt.channels = params->fmt.channels;
	ctx->fmt.bits = params->fmt.bits;
	ctx->vcapturecb = params->vcapturecb;

	ctx->log_to_file = params->log_to_file;
	if (ctx->log_to_file == 1) {
		strcpy(ctx->filename,
				(params->filename == NULL) ? DEFAULT_CAPTURE_FILE_NAME : params->filename);
		LISA_LOGE(TAG, "[%s %d]log record file to %s", __FUNCTION__, __LINE__, ctx->filename);
	}

	_dump_params(ctx);

	ctx->msgq_databuf =
			lisa_mem_calloc(IMAGE_VCAPTURE_DATA_MSGQ_DEPTH, sizeof(vcapture_msgq_data_t));
	if (ctx->msgq_databuf == NULL) {
		LISA_LOGE(TAG, "[%s %d]msgq_databuf malloc failed", __FUNCTION__, __LINE__);
		goto _err_msgq_malloc;
	}
	k_msgq_init(&ctx->msgq, ctx->msgq_databuf, sizeof(vcapture_msgq_data_t),
			IMAGE_VCAPTURE_DATA_MSGQ_DEPTH);

	ctx->vcapture = vcap_create();
	if (ctx->vcapture == NULL) {
		LISA_LOGE(TAG, "[%s %d]vcap_create failed", __FUNCTION__, __LINE__);
		goto _err_vcap_create;
	}

	_vcapture_set_status(ctx, VCAPTURE_STATUS_INIT);

	g_vcapture_handler->ctx = ctx;

	return g_vcapture_handler;

_err_vcap_create:
	lisa_mem_free(ctx->msgq_databuf);
_err_msgq_malloc:
	lisa_mem_free(ctx);
_err_ctx_malloc:
	lisa_mem_free(g_vcapture_handler);
_err_handler_malloc:
_err_handler:
_err_fmt:
	return NULL;
}

/**
 * @brief	start vcapture
 *
 * @param handler  vcapture handle pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_vcapture_start(lisa_vcapture_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa vcapture handler hasn't been created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "Start vcapture\n");

	lisa_vcapture_context_t *ctx = handler->ctx;
	if (_vcapture_get_status(ctx) != VCAPTURE_STATUS_INIT) {
		LISA_LOGE(TAG, "[%s %d]vcapture not init", __FUNCTION__, __LINE__);
		goto _err_status;
	}

	int ret = vcap_event_register(ctx->vcapture, AVF_STREAM_EVENT_THRES_NOTIFY,
			_vcapture_data_process, ctx, IMAGE_CAPTURE_EVENT_THRES);
	if (ret != 0) {
		LISA_LOGE(TAG, "[%s %d]vcap_event_register failed\n", __FUNCTION__, __LINE__);
		goto _err_vcap_event;
	}
	vcap_set_fmt(ctx->vcapture, &ctx->fmt);

	if (vcap_start(ctx->vcapture) != 0) {
		LISA_LOGE(TAG, "[%s %d]vcap_event_register failed\n", __FUNCTION__, __LINE__);
		goto _err_vcap_start;
	}

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "_vcapture_thread_process";
	thread_attr.stack_size = 1 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL
	ctx->tid = lisa_thread_create(&thread_attr, _vcapture_thread_process, ctx);
	if (ctx->tid == NULL) {
		LISA_LOGE(TAG, "[%s %d]k_thread_create vcapture thread failed\n", __FUNCTION__, __LINE__);
		goto _err_thread_cre;
	}

	_vcapture_set_status(ctx, VCAPTURE_STATUS_START);
	ctx->vcapturecb(VCAPTURE_START, NULL, 0);

	return 0;
_err_thread_cre:
	if (ctx->vcapture) vcap_stop(ctx->vcapture);
_err_vcap_start:
	if (ctx->vcapture) vcap_event_unregister(ctx->vcapture, AVF_STREAM_EVENT_THRES_NOTIFY);
_err_vcap_event:
_err_status:
_err_handler:
	return -1;
}

/**
 * @brief	vcapture stop record func
 *
 * @param handler	vcapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_vcapture_stop(lisa_vcapture_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa vcapture handler hasn't been created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "Stop vcapture\n");

	lisa_vcapture_context_t *ctx = handler->ctx;
	if (_vcapture_get_status(ctx) != VCAPTURE_STATUS_START) {
		LISA_LOGE(TAG, "[%s %d]vcapture not start", __FUNCTION__, __LINE__);
		goto _err_status;
	}

	ctx->isruning = 0;
	lisa_thread_delete(ctx->tid);

	vcap_stop(ctx->vcapture);
	vcap_event_unregister(ctx->vcapture, AVF_STREAM_EVENT_THRES_NOTIFY);

	_vcapture_set_status(ctx, VCAPTURE_STATUS_STOP);
	ctx->vcapturecb(VCAPTURE_STOP, NULL, 0);

	return 0;
_err_status:
_err_handler:
	return -1;
}

/**
 * @brief	vcapture resource recycle func
 *
 * @param handler vcapture handler pointer
 *
 * @retval  -1 failed 0 success
 */
int8_t lisa_vcapture_destroy(lisa_vcapture_handler_t *handler)
{
	if (handler == NULL) {
		LISA_LOGE(
				TAG, "[%s %d]lisa vcapture handler hasn't been created!\n", __FUNCTION__, __LINE__);
		goto _err_handler;
	}
	LISA_LOGD(TAG, "Start stop\n");

	lisa_vcapture_context_t *ctx = handler->ctx;

	if ((_vcapture_get_status(ctx) != VCAPTURE_STATUS_STOP) &&
			(_vcapture_get_status(ctx) != VCAPTURE_STATUS_INIT)) {
		LISA_LOGE(TAG, "[%s %d]vcapture not init", __FUNCTION__, __LINE__);
		goto _err_status;
	}

	vcap_destroy(ctx->vcapture);

	lisa_mem_free(ctx->msgq_databuf);
	lisa_mem_free(ctx);
	lisa_mem_free(handler);

	g_vcapture_handler = NULL;

	return 0;
_err_status:
_err_handler:
	return -1;
}
