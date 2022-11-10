#include "lisa_evs_rec.h"

#include <lisa_mem.h>
#include <lisa_mutex.h>
#include <lisa_queue.h>
#include <lisa_thread.h>
#include <lisa_log.h>
#include "utils/cv_crop.h"
#include "cjson/cJSON.h"
#include "lisa_evs.h"
#include "lisa_evs_audiomgr.h"
#include "utils/evs_utils.h"
#include "sockets/lisa_evs_websocket.h"
#include "utils/image_frame_protocol.h"
#include <lisa_typedef.h>

#define TAG "lisa_rec"

/// 识别发送图像线程参数
#define REC_TX_IMAGE_THREAD_NAME ("rec_tx_image")
#define REC_TX_IMAGE_THREAD_PRIORITY OS_PRIORITY_HIGH
#define REC_TX_IMAGE_THREAD_STACKSIZE (5 * 1024)
/// 图像队列参数
#define REC_IMAGE_QUEUE_COUNT (20)
#define REC_IMAGE_QUEUE_NAME ("image_queue")
#define REC_IMAGE_QUEUE_ITEM_SIZE (sizeof(rec_image_block_t))

/// 此定义用于image得visual_in请求的metadata.image_debug字段
/// 此字段为true, 可以通过 https://cdn.iflyos.cn/public/ocr_debug/{request_id} 获取数据
#define REC_IMAGE_DEBUG_ORIGIN_DATA (true)

typedef struct lisa_evs_rec_context_s {
	lisa_thread_t *tx_image;
	lisa_queue_t *image_queue;
	lisa_mutex_t *image_mutex;
	lisa_evs_rec_data_cb data_cb;
} lisa_evs_rec_context_t;

typedef enum rec_image_data_type {
	REC_IMAGE_DATA_T_START = 0,  // 图片开始帧
	REC_IMAGE_DATA_T_IMAGE,  // 图片数据块
	REC_IMAGE_DATA_T_ENTER,  // 图片换行帧
	REC_IMAGE_DATA_T_END,  // 图片发送END
	REC_IMAGE_DATA_T_NONE,  // 非法
} rec_image_data_type_t;

/**
 * @brief 图片块
 */
typedef struct rec_image_block_s {
	img_frame_protocol_t *block;
	// 图片块类型
	rec_image_data_type_t block_type;
	// 一次识别中的音频块数据序号, 从0开始
	int index;
	void *usr_data;
} rec_image_block_t;

typedef struct rec_image_block_start_s {
	char *req_id;
	lisa_evs_rec_image_param_t *param;
} rec_image_block_start_t;

/**
 * @brief 图片参数
 */
typedef struct rec_image_param_s {
	uint8_t fmt;
	uint8_t depth;
} rec_image_param_t;

/**
 * @brief 音频块
 */
typedef struct rec_audio_block_s {
	char *data;
	int len;
} rec_audio_block_t;

#define REC_QUEUE_WAIT_FOREVER LISA_WAIT_FOREVER

static uint32_t s_block_image_index = 0;
static evs_rec_img_compress s_img_compress = NULL;
static bool s_debug = false;

/**
 * @brief 压缩图片并发送
 * @param handle 			rec句柄
 * @param in_img_frame_msg 	图像数据
 * @return int
 */
int _compress_and_send_image_frame(
		const lisa_evs_rec_t *const handle, const img_frame_protocol_t *const in_img_frame_msg)
{
	const uint32_t in_image_size = in_img_frame_msg->width * in_img_frame_msg->height;
	uint32_t image_frame_len = 0;
	uint8_t *image_frame_data = (uint8_t *)lisa_mem_alloc(in_image_size + CROP_DATA_HEADER_LEN);
	image_frame_data_build(
			in_img_frame_msg, &image_frame_data, &image_frame_len, s_img_compress, false);
	// LISA_LOGD(TAG, "build image frame, pre len: %d, after: %d", in_image_size, image_frame_len);

	// 发送5帧图片到EVS
	lisa_evs_websocket_send_image(handle->ws, image_frame_data, image_frame_len);
	if (handle->ctx->data_cb) handle->ctx->data_cb(image_frame_data, image_frame_len);
	if (image_frame_data) lisa_mem_free(image_frame_data);

	return 0;
}

int _reset_image_process_param(
		uint8_t *stitch_image, CVSize *in_size, CVSize *out_size, CVSize *last_stitch)
{
	if (stitch_image) memset(stitch_image, 0xFF, sizeof(uint8_t) * MAX_OUTPUT_IMAGE_SIZE);
	if (in_size) memset(in_size, 0, sizeof(CVSize));
	if (out_size) memset(out_size, 0, sizeof(CVSize));
	if (last_stitch) memset(last_stitch, 0, sizeof(CVSize));

	return 0;
}

int _rec_image_start(const lisa_evs_rec_t *const handle, const uint8_t *const req_id,
		const lisa_evs_rec_image_param_t *const param)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "name", "recognizer.visual_in");
	cJSON_AddStringToObject(header, "request_id", req_id);
	cJSON_AddItemToObject(root, "header", header);
	cJSON_AddBoolToObject(payload, "realtime", !(param->single_line));
	switch (param->rec_mode) {
		case EVS_REC_MODE_OCR:
			cJSON_AddStringToObject(payload, "profile", "OCR");
			break;
		case EVS_REC_MODE_OCR_TTS:
			cJSON_AddStringToObject(payload, "profile", "OCR_TTS");
			break;
		case EVS_REC_MODE_OCR_TRANS:
			cJSON_AddStringToObject(payload, "profile", "OCR_TRANS");
			break;
		case EVS_REC_MODE_OCR_TRANS_TTS:
			cJSON_AddStringToObject(payload, "profile", "OCR_TRANS_TTS");
			break;
		case EVS_REC_MODE_TEXTBOOK:
			cJSON_AddStringToObject(payload, "profile", "TEXTBOOK");
			break;
	}
	cJSON *metadata = cJSON_CreateObject();
	cJSON_AddBoolToObject(metadata, "reverse", !param->debug);
	cJSON_AddNumberToObject(metadata, "height", 128);
	cJSON_AddBoolToObject(metadata, "image_debug", param->debug_image);
	cJSON_AddBoolToObject(metadata, "tokenization", param->tokenization);
	cJSON_AddBoolToObject(metadata, "dictionary", param->dictionary);
	cJSON_AddBoolToObject(metadata, "resources", param->resources);
	cJSON_AddStringToObject(metadata, "compress", "lzma2");
	cJSON_AddItemToObject(payload, "image_metadata", metadata);
	if (param->debug) cJSON_AddBoolToObject(metadata, "debug", param->debug);

	cJSON_AddItemToObject(root, "payload", payload);
	int ret = lisa_evs_client_request(handle->evs, root, req_id);
	if (ret != 0) {
		LISA_LOGE(TAG, "request error!");
		return LISA_FAIL;
	}
	lisa_evs_websocket_begin_image(handle->evs->ws);

	s_block_image_index = 0;
	return LISA_OK;
}

int _send_origin_image_frame(
		const lisa_evs_rec_t *const handle, const img_frame_protocol_t *const in_img_frame_msg)
{
	const uint32_t in_image_size = in_img_frame_msg->width * in_img_frame_msg->height;
	uint32_t image_frame_len = 0;
	uint8_t *image_frame_data = (uint8_t *)lisa_mem_alloc(in_image_size + CROP_DATA_HEADER_LEN);
	image_frame_data_build(
			in_img_frame_msg, &image_frame_data, &image_frame_len, s_img_compress, false);
	// LISA_LOGD(TAG, "build image frame, pre len: %d, after: %d", in_image_size, image_frame_len);

	lisa_evs_websocket_send_image(handle->ws, image_frame_data, image_frame_len);
	if (handle->ctx->data_cb) handle->ctx->data_cb(image_frame_data, image_frame_len);
	if (image_frame_data) lisa_mem_free(image_frame_data);
	return 0;
}

void _rec_tx_image_thread_fun(void *arg)
{
	lisa_evs_rec_t *handle = (lisa_evs_rec_t *)arg;
	if (handle == NULL) {
		LISA_LOGE(TAG, "lisa_evs_rec is null in tx image thread");
		return;
	}
	if (handle->ctx == NULL) {
		LISA_LOGE(TAG, "lisa evs rec context is null in tx image thread");
		return;
	}
	if (handle->ctx->image_queue == NULL) {
		LISA_LOGE(TAG, "lisa evs rec image queue is null in tx image thread");
		return;
	}

	uint8_t *stitch_image = (uint8_t *)lisa_mem_calloc(1, MAX_OUTPUT_IMAGE_SIZE);
	bool running = true;
	lisa_err_t queue_rev_ret = LISA_OK;
	rec_image_block_t image_block;
	uint32_t stitch_index = 0;
	CVSize in_size;
	CVSize out_size;
	CVSize last_stitch;
	rec_image_param_t last_img_param = {0, 0};
	int shift[2] = {0, 0};

	_reset_image_process_param(stitch_image, &in_size, &out_size, &last_stitch);

	int in_status = ISTITCH_ISTATUS_BEGIN_GLOBAL_;

	while (running) {
		memset(&image_block, 0, sizeof(rec_image_block_t));
		queue_rev_ret = lisa_queue_receive(handle->ctx->image_queue, &image_block,
				REC_IMAGE_QUEUE_ITEM_SIZE, REC_QUEUE_WAIT_FOREVER);
		if (queue_rev_ret != LISA_OK) {
			LISA_LOGE(TAG, "get image item fail from image queue in rec thread.");
			continue;
		}

		rec_image_data_type_t block_type = image_block.block_type;

		// 该数据为开始帧
		if (block_type == REC_IMAGE_DATA_T_START) {
			rec_image_block_start_t *start = (rec_image_block_start_t *)(image_block.usr_data);
			if (start != NULL) {
				LISA_LOGD(TAG, "Send image start event");
				s_debug = start->param->debug;
				_rec_image_start(handle, start->req_id, start->param);
				if (start->req_id) lisa_mem_free(start->req_id);
				if (start->param) lisa_mem_free(start->param);
				if (start) lisa_mem_free(start);
			}
			continue;
		} else if (block_type == REC_IMAGE_DATA_T_ENTER) {  // 该数据为换行帧
			if (s_debug) continue;

			// 之前有不满5帧的数据未发送
			if (stitch_index % 5 != 0) {
				img_frame_protocol_t in_img_frame_msg = {.tag = IMAGE_FRAME_MASSAGE_TAG,
						.ver = IMAGE_FRAME_MASSAGE_VER,  // TODO
														 // 调试阶段数据不对，写死，后期根据CSK上来的数据
						.fuid = stitch_index / 5,
						.type = IMAGE_FRAME_MASSAGE_TYPE,  // TODO
														   // 调试阶段数据不对，写死，后期根据CSK上来的数据
						.fmt = IMAGE_FRAME_MASSAGE_FMT,
						.xsft = last_stitch.x,
						.ysft = last_stitch.y,
						.width = out_size.width,
						.height = out_size.height,
						.depth = last_img_param.depth,
						.reserved = 0,
						.data = stitch_image};
				// 该帧不满足5帧数据, 但是计数得正常
				stitch_index += 5 - (stitch_index % 5);
				LISA_LOGD(TAG, "send remaining image frame before enter frame");
				_compress_and_send_image_frame(handle, &in_img_frame_msg);
			}
			// 发送换行帧
			const uint8_t image_enter_frame[16] = {0x46, 0x5A, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
			LISA_LOGD(TAG, "send image enter frame to websocket");
			lisa_evs_websocket_send_image(handle->ws, image_enter_frame, 16);
			_reset_image_process_param(stitch_image, &in_size, &out_size, NULL);
			continue;
		} else if (block_type == REC_IMAGE_DATA_T_END) {
			if (s_debug) {
				lisa_evs_websocket_end_image(handle->evs->ws);
				LISA_LOGD(TAG, "Send image end event");
				int send_end_ret = lisa_evs_websocket_send_text(
						handle->evs->ws, "__END__", strlen("__END__"), NULL);
				if (send_end_ret != 0) {
					LISA_LOGE(TAG, "lisa evs send image end fail %d", send_end_ret);
				}
				stitch_index = 0;
				continue;
			} else {
				// 数据为发送图片END
				// 之前有不满5帧的数据未发送
				// 正常模式,之前有不满5帧的数据未发送
				if (stitch_index % 5 != 0) {
					img_frame_protocol_t in_img_frame_msg = {.tag = IMAGE_FRAME_MASSAGE_TAG,
							.ver = IMAGE_FRAME_MASSAGE_VER,  // TODO
															 // 调试阶段数据不对，写死，后期根据CSK上来的数据
							.fuid = stitch_index / 5,
							.type = IMAGE_FRAME_MASSAGE_TYPE,  // TODO
															   // 调试阶段数据不对，写死，后期根据CSK上来的数据
							.fmt = IMAGE_FRAME_MASSAGE_FMT,
							.xsft = last_stitch.x,
							.ysft = last_stitch.y,
							.width = out_size.width,
							.height = out_size.height,
							.depth = last_img_param.depth,
							.reserved = 0,
							.data = stitch_image};
					// 该帧不满足5帧数据, 但是计数得正常
					stitch_index += 5 - (stitch_index % 5);
					LISA_LOGD(TAG, "send remaining image frame before send end image");
					_compress_and_send_image_frame(handle, &in_img_frame_msg);
				}
				lisa_evs_websocket_end_image(handle->evs->ws);
				LISA_LOGD(TAG, "Send image end event");
				int send_end_ret = lisa_evs_websocket_send_text(
						handle->evs->ws, "__END__", strlen("__END__"), NULL);
				if (send_end_ret != 0) {
					LISA_LOGE(TAG, "lisa evs send image end fail %d", send_end_ret);
				}
				stitch_index = 0;
				continue;
			}
		}

		// 退出线程数据
		if (image_block.block_type == REC_IMAGE_DATA_T_NONE) {
			running = false;
			LISA_LOGD(TAG, "!!!!!!!! exit image queue thread !!!!!!!!");
			break;
		}

		if (s_debug) {
			stitch_index++;
			img_frame_protocol_t in_img_frame_msg = {.tag = IMAGE_FRAME_MASSAGE_TAG,
					.ver = IMAGE_FRAME_MASSAGE_VER,
					.fuid = stitch_index,
					.type = IMAGE_FRAME_MASSAGE_TYPE,
					.fmt = IMAGE_FRAME_MASSAGE_FMT,
					.xsft = image_block.block->xsft,
					.ysft = image_block.block->ysft,
					.width = image_block.block->width,
					.height = image_block.block->height,
					.depth = image_block.block->depth,
					.reserved = 0,
					.data = image_block.block->data};
			_send_origin_image_frame(handle, &in_img_frame_msg);
			lisa_mem_free(image_block.block->data);
			lisa_mem_free(image_block.block);
		} else {
			if (image_block.index == 0) {
				in_status = ISTITCH_ISTATUS_BEGIN_GLOBAL_;  // 扫描第一个图片
				_reset_image_process_param(stitch_image, &in_size, &out_size, &last_stitch);
			} else if (stitch_index % 5 == 0) {
				in_status = ISTITCH_ISTATUS_BEGIN_BATCH_;  // 某次拼接开始
			} else {
				in_status = ISTITCH_ISTATUS_CONTINUE_;
			}

			// 转置拼接
			in_img_transfer((uint8_t *)image_block.block->data, (uint8_t *)image_block.block->data,
					image_block.block->width, image_block.block->height);
			shift[0] = image_block.block->xsft;
			shift[1] = image_block.block->ysft;
			in_size.height = image_block.block->width;
			in_size.width = image_block.block->height;

			stitch_img_T((uint8_t *)image_block.block->data, stitch_image, &in_size, &out_size,
					&last_stitch, shift, in_status);
			stitch_index++;
			last_img_param.fmt = image_block.block->fmt;
			last_img_param.depth = image_block.block->depth;
			if (image_block.block->data) lisa_mem_free(image_block.block->data);

			if (stitch_index % 5 == 0) {
				img_frame_protocol_t in_img_frame_msg = {.tag = IMAGE_FRAME_MASSAGE_TAG,
						.ver = IMAGE_FRAME_MASSAGE_VER,
						.fuid = stitch_index / 5,
						.type = IMAGE_FRAME_MASSAGE_TYPE,
						.fmt = IMAGE_FRAME_MASSAGE_FMT,
						.xsft = last_stitch.x,
						.ysft = last_stitch.y,
						.width = out_size.width,
						.height = out_size.height,
						.depth = image_block.block->depth,
						.reserved = 0,
						.data = stitch_image};

				_compress_and_send_image_frame(handle, &in_img_frame_msg);
				_reset_image_process_param(stitch_image, &in_size, &out_size, NULL);
			}
			if (image_block.block) lisa_mem_free(image_block.block);
		}
	}
	if (stitch_image) lisa_mem_free(stitch_image);

	lisa_thread_delete(handle->ctx->tx_image);
}

lisa_evs_rec_t *lisa_evs_rec_create(lisa_evs_t *evs, lisa_evs_ws_t *ws)
{
	LISA_LOGD(TAG, "lisa evs recognizer create [START]");
	lisa_evs_rec_t *rec = (lisa_evs_rec_t *)lisa_mem_calloc(1, sizeof(lisa_evs_rec_t));
	lisa_evs_rec_context_t *rec_ctx = NULL;
	lisa_queue_t *rec_image_queue = NULL;
	lisa_mutex_t *rec_image_mutex = NULL;
	lisa_thread_attr_t tx_image_thread_attr;
	lisa_thread_t *tx_image_thread = NULL;

	if (!rec) {
		LISA_LOGE(TAG, "alloc lisa_evs_rec pointer error.");
		goto LISA_EVS_REC_EXIT_ERROR;
	}
	rec->ws = ws;
	rec->evs = evs;
	rec_ctx = (lisa_evs_rec_context_t *)lisa_mem_calloc(1, sizeof(lisa_evs_rec_context_t));
	if (!rec_ctx) {
		LISA_LOGE(TAG, "alloc lisa_evs_rec_context pointer error.");
		goto LISA_EVS_REC_EXIT_ERROR_CTX;
	}
	rec->ctx = rec_ctx;

	rec_image_queue = lisa_queue_create(
			REC_IMAGE_QUEUE_COUNT, REC_IMAGE_QUEUE_NAME, REC_IMAGE_QUEUE_ITEM_SIZE);
	if (!rec_image_queue) {
		LISA_LOGE(TAG, "create lisa rec image queue fail.");
		goto LISA_EVS_REC_EXIT_ERROR_IMAGE_QUEUE;
	}
	rec_ctx->image_queue = rec_image_queue;

	rec_image_mutex = lisa_mutex_create();
	if (!rec_image_mutex) {
		LISA_LOGE(TAG, "create lisa rec image mutex fail.");
		goto LISA_EVS_REC_EXIT_ERROR_IMAGE_MUTEX;
	}
	rec_ctx->image_mutex = rec_image_mutex;

	tx_image_thread_attr.name = REC_TX_IMAGE_THREAD_NAME;
	tx_image_thread_attr.priority = REC_TX_IMAGE_THREAD_PRIORITY;
	tx_image_thread_attr.stack_size = REC_TX_IMAGE_THREAD_STACKSIZE;
	tx_image_thread = lisa_thread_create(&tx_image_thread_attr, _rec_tx_image_thread_fun, rec);
	if (!tx_image_thread) {
		LISA_LOGE(TAG, "create lisa rec tx image thread fail.");
		goto LISA_EVS_REC_EXIT_ERROR_TX_IMAGE;
	}
	rec_ctx->tx_image = tx_image_thread;

	s_img_compress = inner_image_jpeg_compress_fun;

	goto LISA_EVS_REC_EXIT_SUCCESS;

LISA_EVS_REC_EXIT_ERROR_TX_IMAGE:
	if (rec_image_mutex) {
		lisa_mutex_delete(rec_image_mutex);
	}
LISA_EVS_REC_EXIT_ERROR_IMAGE_MUTEX:
	if (rec_image_queue) {
		lisa_queue_clear(rec_image_queue);
		lisa_queue_delete(rec_image_queue);
	}
LISA_EVS_REC_EXIT_ERROR_IMAGE_QUEUE:
	lisa_mem_free(rec_ctx);
LISA_EVS_REC_EXIT_ERROR_CTX:
	lisa_mem_free(rec);
LISA_EVS_REC_EXIT_ERROR:
	rec = NULL;
LISA_EVS_REC_EXIT_SUCCESS:
	LISA_LOGD(TAG, "lisa evs recognizer create [END]");
	return rec;
}

lisa_err_t lisa_evs_rec_image_start(const lisa_evs_rec_t *const handle, const uint8_t *const req_id,
		const lisa_evs_rec_image_param_t *const param)
{
	const int req_id_len = strlen(req_id);
	rec_image_block_t block;

	rec_image_block_start_t *block_start =
			(rec_image_block_start_t *)lisa_mem_calloc(1, sizeof(rec_image_block_start_t));
	if (block_start == NULL) goto LISA_EVS_IMAGE_START_EXIT;

	block_start->req_id = (char *)lisa_mem_alloc(sizeof(char) * (req_id_len + 1));
	if (block_start->req_id == NULL) goto LISA_EVS_IMAGE_REQID_ERR;
	strcpy(block_start->req_id, req_id);
	block_start->req_id[req_id_len] = '\0';

	block_start->param =
			(lisa_evs_rec_image_param_t *)lisa_mem_alloc(sizeof(lisa_evs_rec_image_param_t));
	if (block_start->param == NULL) goto LISA_EVS_IMAGE_PARAM_ERR;
	block_start->param->debug_image = param->debug_image;
	block_start->param->debug = param->debug;
	block_start->param->rec_mode = param->rec_mode;
	block_start->param->single_line = param->single_line;
	block_start->param->tokenization = param->tokenization;
	block_start->param->dictionary = param->dictionary;
	block_start->param->resources = param->resources;

	block.block_type = REC_IMAGE_DATA_T_START;
	block.index = 0;
	block.block = NULL;
	block.usr_data = (void *)block_start;

	lisa_err_t ret =
			lisa_queue_push(handle->ctx->image_queue, &block, sizeof(rec_image_block_t), 0);
	return (ret == LISA_OK) ? LISA_OK : LISA_FAIL;

LISA_EVS_IMAGE_PARAM_ERR:
	lisa_mem_free(block_start->req_id);
	LISA_LOGE(TAG, "malloc image param fail");
LISA_EVS_IMAGE_REQID_ERR:
	LISA_LOGE(TAG, "malloc image req id fail");
	lisa_mem_free(block_start);
LISA_EVS_IMAGE_START_EXIT:
	LISA_LOGE(TAG, "malloc image block start fail");
	return LISA_FAIL;
}

lisa_err_t lisa_evs_rec_image_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id)
{
	rec_image_block_t image_block;
	image_block.block_type = REC_IMAGE_DATA_T_END;
	lisa_err_t ret =
			lisa_queue_push(handle->ctx->image_queue, &image_block, sizeof(rec_image_block_t), 0);
	return (ret == LISA_OK) ? LISA_OK : LISA_FAIL;
}

lisa_err_t lisa_evs_rec_audio_start(const lisa_evs_rec_t *const handle, const uint8_t *const req_id,
		const lisa_evs_rec_audio_param_t *const param)
{
	LISA_LOGD(TAG, "lisa evs rec audio start with request id: %s [BEGIN]", req_id);
	int ret = 0;
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "name", "recognizer.audio_in");
	cJSON_AddStringToObject(header, "request_id", req_id);
	cJSON_AddItemToObject(root, "header", header);

	cJSON_AddStringToObject(payload, "profile", "CLOSE_TALK");
	switch (param->format) {
		case EVS_REC_AUDIO_FORMAT_BASE:
			cJSON_AddStringToObject(payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1");
			break;
		case EVS_REC_AUDIO_FORMAT_OPUS:
			cJSON_AddStringToObject(payload, "format", "OPUS");
			break;
		case EVS_REC_AUDIO_FORMAT_SPEEX:
			cJSON_AddStringToObject(payload, "format", "SPEEX_WB_QUALITY_9");
			break;
	}
	cJSON_AddBoolToObject(payload, "enable_vad", param->enable_vad);
	cJSON_AddBoolToObject(payload, "translation", param->translation);
	cJSON_AddNumberToObject(payload, "vad_eos", ((param->vad_eos < 800) ? 800 : (param->vad_eos)));
	cJSON_AddStringToObject(payload, "reply_key", param->reply_key);
	cJSON_AddItemToObject(root, "payload", payload);
	ret = lisa_evs_client_request(handle->evs, root, req_id);
	if (ret != 0) {
		LISA_LOGE(TAG, "request error!");
		return LISA_FAIL;
	}

	bool speex_enable = (param->format == EVS_REC_AUDIO_FORMAT_SPEEX) ? true : false;
	lisa_evs_websocket_begin_audio(handle->evs->ws, speex_enable);
	LISA_LOGD(TAG, "lisa evs rec audio start [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_audio_end(const lisa_evs_rec_t *const handle, const uint8_t *const req_id)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__END__", strlen("__END__"), NULL);
}

lisa_err_t lisa_evs_rec_audio_stop(const lisa_evs_rec_t *const handle)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__END__", strlen("__END__"), NULL);
}

lisa_err_t lisa_evs_rec_image_cancel(const lisa_evs_rec_t *const handle)
{
	lisa_evs_websocket_end_image(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__CANCEL__", strlen("__CANCEL__"), NULL);
}

lisa_err_t lisa_evs_rec_audio_cancel(const lisa_evs_rec_t *const handle)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__CANCEL__", strlen("__CANCEL__"), NULL);
}

lisa_err_t lisa_evs_rec_send_audio(
		const lisa_evs_rec_t *const handle, const uint8_t *const audio, int len)
{
	lisa_evs_websocket_send_audio(handle->evs->ws, audio, len);
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_send_image(const lisa_evs_rec_t *const handle,
		const uint8_t *const image_frame_data, int image_frame_len)
{
	if (handle == NULL || image_frame_data == NULL || image_frame_len == 0) {
		LISA_LOGE(TAG, "send image fail with invalid param or data");
		return LISA_FAIL;
	}

	img_frame_protocol_t *out_img_frame_msg =
			(img_frame_protocol_t *)lisa_mem_calloc(1, sizeof(img_frame_protocol_t));
	int parser_ret = image_frame_data_parser(image_frame_data, image_frame_len, out_img_frame_msg);
	if (parser_ret != 0) {
		LISA_LOGE(TAG, "parser image frame fail, ret %d", parser_ret);
		return LISA_FAIL;
	}

	rec_image_block_t image_block;
	image_block.block = out_img_frame_msg;
	image_block.block_type = REC_IMAGE_DATA_T_IMAGE;
	image_block.index = s_block_image_index;

	s_block_image_index++;
	lisa_err_t ret =
			lisa_queue_push(handle->ctx->image_queue, &image_block, sizeof(rec_image_block_t), 0);
	return (ret == LISA_OK) ? LISA_OK : LISA_FAIL;
}

lisa_err_t lisa_evs_rec_send_image_enter(const lisa_evs_rec_t *const handle)
{
	if (handle == NULL) {
		LISA_LOGE(TAG, "send image enter frame fail because handle is null");
		return LISA_FAIL;
	}

	rec_image_block_t image_block;
	image_block.block_type = REC_IMAGE_DATA_T_ENTER;
	lisa_err_t ret =
			lisa_queue_push(handle->ctx->image_queue, &image_block, sizeof(rec_image_block_t), 0);
	return (ret == LISA_OK) ? LISA_OK : LISA_FAIL;
}

lisa_err_t lisa_evs_rec_set_compress_fun(evs_rec_img_compress compress_fun)
{
	if (compress_fun) {
		LISA_LOGD(TAG, "change image compress function to external function %p", compress_fun);
		s_img_compress = compress_fun;
	} else {
		LISA_LOGD(TAG, "reset inner image compress function");
		s_img_compress = inner_image_jpeg_compress_fun;
	}
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_destory(lisa_evs_rec_t *handle)
{
	LISA_LOGD(TAG, "lisa evs recognizer destory [START]");
	if (handle) {
		if (handle->ctx) {
			if (handle->ctx->tx_image) {
				lisa_queue_clear(handle->ctx->image_queue);
				rec_image_block_t image_block = {.block_type = REC_IMAGE_DATA_T_NONE};
				lisa_queue_push(
						handle->ctx->image_queue, &image_block, sizeof(rec_image_block_t), 0);
				lisa_thread_delete(handle->ctx->tx_image);
			}
			if (handle->ctx->image_queue) {
				lisa_queue_clear(handle->ctx->image_queue);
				lisa_queue_delete(handle->ctx->image_queue);
			}
			if (handle->ctx->image_mutex) {
				lisa_mutex_delete(handle->ctx->image_mutex);
			}
			lisa_mem_free(handle->ctx);
		}

		lisa_mem_free(handle);
	}
	LISA_LOGD(TAG, "lisa evs recognizer destory [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_evaluate_start(const lisa_evs_rec_t *const handle,
		const uint8_t *const req_id, const lisa_evs_rec_audio_param_t *const param,
		const lisa_evs_rec_evaluate_param_t *const evaluate)
{
	LISA_LOGD(TAG, "lisa evs rec evaluate start with request id: %s [BEGIN]", req_id);
	int ret = 0;
	cJSON *root = cJSON_CreateObject();
	cJSON *header = cJSON_CreateObject();
	cJSON *payload = cJSON_CreateObject();
	cJSON *evaluate_obj = cJSON_CreateObject();
	cJSON_AddStringToObject(header, "name", "recognizer.audio_in");
	cJSON_AddStringToObject(header, "request_id", req_id);
	cJSON_AddItemToObject(root, "header", header);

	cJSON_AddStringToObject(payload, "profile", "EVALUATE");
	switch (param->format) {
		case EVS_REC_AUDIO_FORMAT_BASE:
			cJSON_AddStringToObject(payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1");
			break;
		case EVS_REC_AUDIO_FORMAT_OPUS:
			cJSON_AddStringToObject(payload, "format", "OPUS");
			break;
		case EVS_REC_AUDIO_FORMAT_SPEEX:
			cJSON_AddStringToObject(payload, "format", "SPEEX_WB_QUALITY_9");
			break;
	}
	cJSON_AddBoolToObject(payload, "enable_vad", param->enable_vad);
	cJSON_AddNumberToObject(payload, "vad_eos", ((param->vad_eos < 0) ? 800 : (param->vad_eos)));
	cJSON_AddStringToObject(evaluate_obj, "text", evaluate->text);
	switch (evaluate->category) {
		case EVS_REC_EVALUATE_READ_CHAPTER:
			cJSON_AddStringToObject(evaluate_obj, "category", "read_chapter");
			break;
		case EVS_REC_EVALUATE_READ_SENTENCE:
			cJSON_AddStringToObject(evaluate_obj, "category", "read_sentence");
			break;
		case EVS_REC_EVALUATE_READ_WORD:
			cJSON_AddStringToObject(evaluate_obj, "category", "read_word");
			break;
		case EVS_REC_EVALUATE_READ_SYLLABLE:
			cJSON_AddStringToObject(evaluate_obj, "category", "read_syllable");
			break;
		case EVS_REC_EVALUATE_READ_CHOICE:
			cJSON_AddStringToObject(evaluate_obj, "category", "read_choice");
			break;
	}
	switch (evaluate->language) {
		case EVS_REC_EVALUATE_ZH_CN:
			cJSON_AddStringToObject(evaluate_obj, "language", "zh_cn");
			break;
		case EVS_REC_EVALUATE_EN_US:
			cJSON_AddStringToObject(evaluate_obj, "language", "en_us");
			break;
	}
	cJSON_AddItemToObject(payload, "evaluate", evaluate_obj);
	cJSON_AddItemToObject(root, "payload", payload);
	ret = lisa_evs_client_request(handle->evs, root, req_id);
	if (ret != 0) {
		LISA_LOGE(TAG, "request error!");
		return LISA_FAIL;
	}

	bool speex_enable = (param->format == EVS_REC_AUDIO_FORMAT_SPEEX) ? true : false;
	lisa_evs_websocket_begin_audio(handle->evs->ws, speex_enable);
	LISA_LOGD(TAG, "lisa evs rec audio start [END]");
	return LISA_OK;
}

lisa_err_t lisa_evs_rec_evaluate_end(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__END__", strlen("__END__"), NULL);
}

lisa_err_t lisa_evs_rec_evaluate_cancel(
		const lisa_evs_rec_t *const handle, const uint8_t *const req_id)
{
	lisa_evs_websocket_end_audio(handle->evs->ws);
	return lisa_evs_websocket_send_text(handle->evs->ws, "__CANCEL__", strlen("__CANCEL__"), NULL);
}

lisa_err_t lisa_evs_rec_set_data_cb(lisa_evs_rec_t *const handle, lisa_evs_rec_data_cb data_cb)
{
	if (handle != NULL) {
		handle->ctx->data_cb = data_cb;
		return LISA_OK;
	}

	LISA_LOGE(TAG, "lisa evs rec set data callback fail!!!");
	return LISA_FAIL;
}
