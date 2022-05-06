#include "scanpen_app.h"
#include "lisa_evs_uuid.h"
#include <signal.h>
#include <mbedtls/sha1.h>
#include <mbedtls/platform.h>
#include <lisa_http.h>
#include <fs/fs.h>
#include <shell/shell.h>
#include <lisa_time.h>
#define TAG "scanpen_app"


#define L1FS_FILE_PATH(file_name)	("/lfs1/" # file_name)
#define URL_GET_OTA_INFO ("http://ota.iflyos.cn/ota/client/check/packages")

#define IMAGE_MSG_QUE_CNT (24)  //需要是2的倍数
#define LOOP_AUDIO_TEST_SIZE (320)

static char s_request_access_token[80] = {0};
static char s_request_id[40] = {0};

static lisa_evs_t *s_handle = NULL;

static lisa_thread_t *s_ocr_recognize_thread = NULL;
static lisa_thread_t *s_send_ocr_simulation_image_thread = NULL;
static lisa_thread_t *s_spi_task_protocol_thread = NULL;
static lisa_thread_t *s_test_send_audio_thread = NULL;
static lisa_thread_t *s_test_evaluate_thread = NULL;

static lisa_queue_t *s_image_queue = NULL;
static bool s_ws_connected = false;

static struct lisa_ring_buffer *s_send_ocr_simulation_image_buffer = NULL;
static lisa_semaphore_t *s_ocr_image_sema = NULL;
static bool s_debug_mode = false;

static lisa_text_info_t s_notebook[100] = {0};
static lisa_semaphore_t *s_notebook_sem = NULL;
static uint32_t s_pagesize = 10;  // default 10
static uint32_t s_pagenum = 1;
static uint32_t s_word_cnt;
static int s_notepage, s_notesize, s_notetotal, s_pagecnt, s_lastcnt;

// 单行扫描
static bool s_singleline = false;
// 在线扫描
static bool g_isonline = false;

typedef struct {
	lisa_image_transfer_status_e status;
	char *data;
	int size;
} lisa_image_msg_t;

void run_device_init();

bool is_ws_connect(lisa_evs_ws_t *ws)
{
	if (ws != NULL && ws->m_context != NULL) {
		ws_connect_status_e statue = ws->m_context->m_connected;
		if (statue == WS_STATUS_CONNECTED) return true;
		return false;
	} else {
		LISA_LOGE(TAG, "ws: %p is NUll\n", ws);
	}

	return false;
}
/**
 * @brief 连接iflyos
 *
 */
void connect_iflyos()
{
	if (is_ws_connect(s_handle->ws)) {
		LISA_LOGE(TAG, "ws is already connect!\n");
	} else {
		if (s_handle) {
			/*启动ws并且连接iflyos，结果通过ws回调通知*/
			LISA_LOGE(TAG, "try to startup ws and connect iflyos...\n");
			lisa_evs_connect(s_handle, s_request_access_token);
		} else {
			/*设备端初始化：iflyos鉴权等*/
			run_device_init();
		}
	}
}

/**
 * @brief 断开websocket连接
 *
 */
void disconnect_ws()
{
	if (s_handle != NULL && is_ws_connect(s_handle->ws)) {
		LISA_LOGD(TAG, "ws is already connect, try to disconnect ws...!\n");
		lisa_evs_disconnect(s_handle);
	}
}

#include "lisa_vcapture.h"
static lisa_vcapture_handler_t *demo_handler = NULL;
void _image_data_cb(lisa_vcapture_cbmsg_e msg, void *data, uint32_t len)
{
	switch (msg) {
		case VCAPTURE_START:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_START \n");
			break;
		case VCAPTURE_STOP:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_STOP \n");
			break;
		case VCAPTURE_DATA: {
			/*LISA_LOGD(TAG, "vcapture msg VCAPTURE_DATA \n");*/
			lisa_image_msg_t recv_image;
			recv_image.data = (uint8_t *)data;
			recv_image.size = len;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
		} break;
		default:
			LISA_LOGE(TAG, "unsupport vcapture msg\n");
			break;
	}
}
static int _video_init()
{
	lisa_vcapture_params_t params;
	memset(&params, 0, sizeof(lisa_vcapture_params_t));

	params.fmt.rate = 16000;
	params.fmt.channels = 1;
	params.fmt.bits = 16;

	params.vcapturecb = _image_data_cb;

	params.log_to_file = 0;
	// strcpy(params.filename, filename);

	demo_handler = lisa_vcapture_create(&params);
	if (demo_handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_vcapture_create FAILED!\n", __FUNCTION__, __LINE__);
		return -1;
	}

	return 0;
}

/**
 * @brief 扫描笔图片发送回调
 *
 * @param data
 * @param len
 * @param status
 */
void scan_image_cb(char *data, int len, lisa_image_transfer_status_e status)
{
	if (status == LISA_IMAGE_FRAME_END) {
		LISA_LOGE(TAG, "scan_image_cb recv end frame\n");
	}

	lisa_err_t ret = LISA_FAIL;
	lisa_image_msg_t recv_image;
	recv_image.status = status;
	recv_image.data = (uint8_t *)lisa_mem_alloc(len);
	if (!recv_image.data && status != LISA_IMAGE_FRAME_END) {
		LISA_LOGE(TAG, "malloc fail");
		return;
	}
	recv_image.size = len;
	if (recv_image.data != NULL) {
		memcpy(recv_image.data, data, len);
	} else {
		recv_image.data = NULL;
	}
	ret = lisa_queue_push(s_image_queue, (void *)&recv_image, sizeof(lisa_image_msg_t), 0);
	if (ret != LISA_OK) {
		LISA_LOGE(TAG, "send img_msg failed! ret: %d", ret);
		if (recv_image.data != NULL) lisa_mem_free(recv_image.data);
	}
}

/**
 * @brief 从ringbuf中接受一组图片
 *
 * @param ring_buffer
 * @param data
 * @param len
 * @param wait_ms
 * @return int
 */
int recv_ocr_simulation_image_once(
		struct lisa_ring_buffer *ring_buffer, uint8_t *data, unsigned int len, int wait_ms)
{
	int ret = lisa_ring_buffer_get(ring_buffer, data, len);
	while (ret <= 0) {
		lisa_semaphore_take(s_ocr_image_sema, LISA_WAIT_FOREVER);
		ret = lisa_ring_buffer_get(ring_buffer, data, len);
	}
	return ret;
}

/**
 * @brief 从ringbuf中发送一组图片
 *
 * @param ring_buffer
 * @param data
 */
void send_ocr_simulation_image_once(struct lisa_ring_buffer *ring_buffer, lisa_image_msg_t *data)
{
	bool need_wake = false;
	if (lisa_ring_buffer_len(ring_buffer) <= 0) {
		need_wake = true;
	}

	int ret = lisa_ring_buffer_put(ring_buffer, data->data, data->size);
	if (need_wake) {
		lisa_semaphore_give(s_ocr_image_sema);
	}
	if (ret != data->size) {
		LISA_LOGE(TAG, "write stream full \n");
	}

	lisa_thread_mdelay(15);
}

/**
 * @brief ocr图片发送模拟线程
 *
 * @param arg
 */
static void send_ocr_simulation_image_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);

	struct fs_file_t fp;
	fs_file_t_init(&fp);
	int err = fs_open(&fp, L1FS_FILE_PATH(image.bin), FS_O_READ);
	if (err) {
		LISA_LOGE(TAG, " %s %d open file failed, err = %d", __FUNCTION__, __LINE__, err);
		goto ERR_OPEN_FILE;
	}
	fs_seek(&fp, 0, FS_SEEK_END);
	const long spi_mock_data_len = fs_tell(&fp);
	LISA_LOGD(TAG, "audio file size %ld\n", spi_mock_data_len);
	fs_seek(&fp, 0, FS_SEEK_SET);
	const int spi_frame_size = 2048;

	lisa_image_msg_t recv_image;
	recv_image.status = LISA_IMAGE_FRAME_NONE;
	recv_image.data = (uint8_t *)lisa_mem_alloc(spi_frame_size);
	if (!recv_image.data) {
		LISA_LOGE(TAG, "malloc fail");
		goto ERR_MALLOC;
	}

	int send_total = 0;

	// 扫描开始
	send_uart_scan_start();

	LISA_LOGD(TAG, "send_uart_scan_start\n");
	lisa_thread_delay(2);

	// 发送ocr模拟图片
	while (spi_mock_data_len > send_total) {
		LISA_LOGD(TAG, "simulation while: send_total: %d\n", send_total);
		if (spi_mock_data_len > (send_total + spi_frame_size)) {
			recv_image.size = spi_frame_size;
			fs_read(&fp, recv_image.data, recv_image.size);
			send_total += recv_image.size;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
		} else {
			LISA_LOGD(TAG, "send_uart_scan_end\n");
			lisa_thread_mdelay(100);
			send_uart_scan_end();

			// 扫描结束
			recv_image.size = spi_mock_data_len - send_total;
			fs_read(&fp, recv_image.data, recv_image.size);
			send_total += recv_image.size;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);

			break;
		}
	}

	lisa_thread_delay(2);
	if (!s_singleline) {
		fs_seek(&fp, 0, FS_SEEK_SET);
		send_uart_scan_start();
		lisa_thread_mdelay(500);
		send_total = 0;
		while (spi_mock_data_len > send_total) {
			LISA_LOGD(TAG, "simulation while: send_total: %d\n", send_total);
			if (spi_mock_data_len > (send_total + spi_frame_size)) {
				recv_image.size = spi_frame_size;
				fs_read(&fp, recv_image.data, recv_image.size);
				send_total += recv_image.size;
				send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
			} else {
				LISA_LOGD(TAG, "send_uart_scan_end\n");
				lisa_thread_mdelay(100);
				send_uart_scan_end();

				// 扫描结束
				recv_image.size = spi_mock_data_len - send_total;
				fs_read(&fp, recv_image.data, recv_image.size);
				send_total += recv_image.size;
				send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
				break;
			}
		}
	}

	LISA_LOGD(TAG, "ocr thread end!\n");
	if (recv_image.data) lisa_mem_free(recv_image.data);

ERR_MALLOC:
	fs_close(&fp);
ERR_OPEN_FILE:
	if (s_send_ocr_simulation_image_thread) {
		lisa_thread_t *temp = s_send_ocr_simulation_image_thread;
		s_send_ocr_simulation_image_thread = NULL;
		lisa_thread_delete(temp);
	}
}

/**
 * @brief 从spi buffer中读取数据
 *
 * @param data
 * @param len
 * @param wait_ms
 * @return uint32_t
 */
uint32_t _read_spi_buffer(uint8_t *data, unsigned int len, int wait_ms)
{
	return recv_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, data, len, wait_ms);
}

/**
 * @brief spi task protocol线程，用于图片数据接受和解析
 *
 * @param argv
 */
void spi_task_protocol_thread(void *argv)
{
	lisa_evs_parser_spi_init(_read_spi_buffer);
	while (1) {
		if (lisa_evs_parser_spi_data()) {
		} else {
		}
	}

	if (s_spi_task_protocol_thread) {
		lisa_thread_t *temp = s_spi_task_protocol_thread;
		s_spi_task_protocol_thread = NULL;
		lisa_thread_delete(temp);
	}
}

/**
 * @brief ocr识别线程
 *
 * @param arg
 */
void ocr_recognize_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);
// 设置扫描模式
#if 0
#if (OCR_MODE == OCR_SINGLELINE)
	listenai_set_scan_mode(e_scan_mode_singleline);
#else
	listenai_set_scan_mode(e_scan_mode_multiline);
#endif
#endif

	// 注册图像回调
	lisa_evs_register_image_callback(scan_image_cb);

	// spi图片消息队列
	s_image_queue = lisa_queue_create(IMAGE_MSG_QUE_CNT, "img_msg", sizeof(lisa_image_msg_t));
	if (s_image_queue == NULL) {
		LISA_LOGE(TAG, "create img_msg queue failed!");
		return;
	}

	lisa_image_msg_t send_image;
	memset(&send_image, 0, sizeof(lisa_image_msg_t));
	static uint32_t image_index = 0;
	LISA_LOGI(TAG, "%s Enter While", __FUNCTION__);
	while (true) {
		if (lisa_queue_receive(s_image_queue, &send_image, sizeof(lisa_image_msg_t),
					LISA_WAIT_FOREVER) == LISA_OK) {
			if (!s_ws_connected) {
				LISA_LOGE(TAG, "websocket is disconnected, can't scan image");
				continue;
			}
			switch (send_image.status) {
				case LISA_IMAGE_FRAME_START:
					if (image_index == 0) {
						lisa_evs_uuid_generate_string(s_request_id);
						lisa_evs_rec_image_param_t image_param = EVS_REC_IMG_PARAM_INIT(
										// 模拟中文分词
										// .tokenization = true,
										// .dictionary = true,
										// .resources = true,
										.rec_mode = EVS_REC_MODE_OCR_TRANS_TTS);
						image_param.debug = s_debug_mode;
						image_param.dictionary = true;
						image_param.single_line = s_singleline;
						LISA_LOGD(TAG, "lisa evs rec image start");
						lisa_evs_rec_image_start(s_handle->rec, s_request_id, &image_param);
					}
					// image_index = 0;
					image_index++;
					lisa_evs_rec_send_image(s_handle->rec, send_image.data, send_image.size);
					break;
				case LISA_IMAGE_FRAME_CONTINUE:
					image_index++;
					lisa_evs_rec_send_image(s_handle->rec, send_image.data, send_image.size);
					break;
				case LISA_IMAGE_FRAME_END:
					image_index++;
					if (send_image.data != NULL)
						lisa_evs_rec_send_image(s_handle->rec, send_image.data, send_image.size);

					if (image_index <= 48 && !s_singleline) {  // test
						lisa_evs_rec_send_image_enter(s_handle->rec);
						LISA_LOGD(TAG, "send image count: %d\n", image_index);
					} else {
						lisa_evs_rec_image_end(s_handle->rec, s_request_id);
						LISA_LOGD(TAG, "send image count: %d\n", image_index);
						image_index = 0;
					}
					break;
				default:
					break;
			}
			if (send_image.data) {
				lisa_mem_free(send_image.data);
			}
		}
	}

	if (s_ocr_recognize_thread) {
		lisa_thread_t *temp_thread = s_ocr_recognize_thread;
		s_ocr_recognize_thread = NULL;
		lisa_thread_delete(temp_thread);
	}
}

/**
 * @brief 词典查询成功回调
 *
 * @param result
 */
void dictionary_lookup_success(const char *const result)
{
	LISA_LOGD(TAG, "dictionary_lookup_success result = %s\n", result);
}

/**
 * @brief 词典查询失败回调
 *
 */
void dictionary_lookup_failed()
{
	LISA_LOGD(TAG, "dictionary_lookup_failed");
}

/**
 * @brief 播放器播放回调
 *
 * @param usr_data
 */
void _play_ws_runnable(void *usr_data)
{
	char *msg = (char *)usr_data;
	if (msg != NULL) {
		cJSON *info = cJSON_Parse(msg);
		if (info != NULL) {
			cJSON *url_json = cJSON_GetObjectItem(info, "url");
			cJSON *content_type_json = cJSON_GetObjectItem(info, "content_type");
			char *url = url_json->valuestring;
			char *content_type = content_type_json->valuestring;
			if (strcmp(content_type, "original") == 0) {
				//播原始文字对应的音频
			} else if ((strcmp(content_type, "translation") == 0)) {
				//翻译后的音频
			} else {
			}
			cJSON_Delete(info);
		}
		lisa_mem_free(msg);
	}
}

/**
 * @brief 打印ws接受到的数据
 *
 * @param msg
 * @param len
 */
void play_ws_msg(const char *const msg, int len)
{
	char *copy_msg = (char *)lisa_mem_alloc(sizeof(char) * len);
	if (copy_msg) {
		memcpy(copy_msg, msg, len);
		evs_handler_post_runnable(_play_ws_runnable, (void *)copy_msg);
	} else {
		LISA_LOGE(TAG, "play websocket message error\n");
	}
}

/**
 * @brief 翻译回调声明和初始化
 *
 */
const static lisa_dictionary_lookup_cb_t s_dictionary_lookup_cb = {
		.on_dictionary_lookup_failed = dictionary_lookup_failed,
		.on_dictionary_lookup_succ = dictionary_lookup_success,
};

/**
 * @brief Create a spi task protocol thread object
 *
 */
void create_spi_task_protocol_thread()
{
	if (s_spi_task_protocol_thread) {
		return;
	}
	// 1. spi_task_protocol_thread
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "spi_task_protocol_thread";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	//解析图片
	s_spi_task_protocol_thread = lisa_thread_create(&thread_attr, spi_task_protocol_thread, NULL);
	if (s_spi_task_protocol_thread == NULL) {
		LISA_LOGE(TAG, "thread_create: spi_task_protocol_thread failed");
		return;
	}
}

/**
 * @brief Create a ocr recognize thread object
 *
 */
void create_ocr_recognize_thread()
{
	if (s_ocr_recognize_thread) {
		return;
	}
	// 2. ocr_thread
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "ocr_recognize_thread";
	thread_attr.stack_size = 2 * 1024;
	// thread_attr.stack_size = K_THREAD_STACK_SIZEOF(ocr_recognize_stack_area);
	// thread_attr.stack = ocr_recognize_stack_area;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	//接受组装并发送图片到云端
	s_ocr_recognize_thread = lisa_thread_create(&thread_attr, ocr_recognize_thread, NULL);
	if (s_ocr_recognize_thread == NULL) {
		LISA_LOGE(TAG, "thread_create: ocr_recognize_thread failed");
		return;
	}
}

/**
 * @brief Create a send ocr simulation image thread object
 *
 */
static void create_send_ocr_simulation_image_thread()
{
	// 3. send_ocr_simulation_image_thread
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "send_ocr_simulation_image_thread";
	thread_attr.stack_size = 2 * 1024;
	// thread_attr.stack_size = K_THREAD_STACK_SIZEOF(send_ocr_simulation_stack_area);
	// thread_attr.stack = send_ocr_simulation_stack_area;
	thread_attr.priority = OS_PRIORITY_HIGH;  // OS_PRIORITY_NORMAL
	// 发送simulation ocr图片
	s_send_ocr_simulation_image_thread =
			lisa_thread_create(&thread_attr, send_ocr_simulation_image_thread, NULL);
	if (s_send_ocr_simulation_image_thread == NULL) {
		LISA_LOGE(TAG, "thread_create: send_ocr_simulation_image_thread failed");
		return;
	}
}

/**
 * @brief 启动ocr识别
 *
 */
void run_ocr_recognition()
{
	start_uart_app();
	if (!s_send_ocr_simulation_image_buffer) {
		unsigned char *buffer = lisa_mem_alloc(64 * 1024);

		s_send_ocr_simulation_image_buffer = lisa_ring_buffer_init(buffer, 64 * 1024);

		s_ocr_image_sema = lisa_semaphore_create(2);
		create_spi_task_protocol_thread();
		create_ocr_recognize_thread();
	}

	create_send_ocr_simulation_image_thread();
}

/**
 * @brief 启动在线ocr识别
 *
 */
void run_ocr_recognition_online()
{
	if (!s_send_ocr_simulation_image_buffer) {
		unsigned char *buffer = lisa_mem_alloc(64 * 1024);

		s_send_ocr_simulation_image_buffer = lisa_ring_buffer_init(buffer, 64 * 1024);

		s_ocr_image_sema = lisa_semaphore_create(2);
		create_spi_task_protocol_thread();
		create_ocr_recognize_thread();
	}

	if (g_isonline) {
		_video_init();
		send_uart_scan_start();

		LISA_LOGD(TAG, "send_uart_scan_start\n");
		lisa_vcapture_start(demo_handler);
		lisa_thread_delay(20);
		LISA_LOGD(TAG, "send_uart_scan_stop\n");
		send_uart_scan_end();
		lisa_vcapture_stop(demo_handler);
		lisa_vcapture_destroy(demo_handler);
	} else {
		create_send_ocr_simulation_image_thread();
	}
}

/**
 * @brief websocket断开重连
 *
 * @param arg
 * @return int
 */
int ws_disconn_runnable(void *arg)
{
	/*如需断开重连，下方做重连动作*/
	// lisa_evs_connect(s_handle, s_request_access_token);
	return 0;
}

/**
 * @brief websocket连接成功回调
 *
 */
void ws_connected_cb()
{
	s_ws_connected = true;
	LISA_LOGD(TAG, "websocket connected\n");
}
/**
 * @brief websocket连接失败回调
 *
 * @param msg
 */
void ws_connect_failed_cb(const char *const msg)
{
	LISA_LOGD(TAG, "websocket connect fail\n");
	s_ws_connected = false;
	evs_handler_post_runnable_delay(ws_disconn_runnable, NULL, 5000);
}
/**
 * @brief websocket断开连接回调
 *
 * @param msg
 */
void ws_disconnected_cb(const char *const msg)
{
	s_ws_connected = false;
	LISA_LOGD(TAG, "websocket disconnect\n");
	evs_handler_post_runnable_delay(ws_disconn_runnable, NULL, 5000);
}
/**
 * @brief websocket消息回调
 *
 * @param action
 * @param msg
 * @param len
 * @param req_id
 */
void ws_message_cb(
		const char *const action, const char *const msg, int len, const char *const req_id)
{
	printk("%s %d run here!\n",__func__,__LINE__);
	LISA_LOGD(TAG, "message, action: %s, len: %d, payload: %s, request id: %s", action, len, msg,
			req_id);
	if (strcmp(action, "audio_player.audio_out") == 0) {
		play_ws_msg(msg, len);
	} else if (strcmp(action, "recognizer.evaluate_result") == 0) {
		LISA_LOGD(TAG, "evaluate result = %s\n", msg);
	}
	printk("%s %d run here!\n",__func__,__LINE__);
}
/**
 * @brief websocket消息发送失败回调
 *
 * @param type
 * @param data
 */
void ws_send_failed_cb(ws_msg_type_e type, const char *data)
{
	LISA_LOGD(TAG, "websocket send fail\n");
}
/**
 * @brief iflyos 下发ping包
 *
 * @param msg
 * @param req_id
 */
void sys_on_ping(const char *const msg, const char *const req_id)
{
	LISA_LOGD(TAG, "sysrem ping, msg: %s, req id: %s", msg, req_id);
}
/**
 * @brief iflyos 下发系统错误消息
 *
 * @param msg
 * @param req_id
 */
void sys_on_error(const char *const msg, const char *const req_id)
{
	LISA_LOGD(TAG, "sysrem error, msg: %s, req id: %s", msg, req_id);
}
/**
 * @brief iflyos 下发云端检查版本更新消息
 *
 * @param msg
 * @param req_id
 */
void sys_on_check_software_update(const char *const msg, const char *const req_id)
{
	LISA_LOGD(TAG, "check software update, msg: %s, req id: %s", msg, req_id);
}
/**
 * @brief iflyos 下发版本升级消息
 *
 * @param msg
 * @param req_id
 */
void sys_on_update_software(const char *const msg, const char *const req_id)
{
	LISA_LOGD(TAG, "software update, msg: %s, req id: %s", msg, req_id);
}
/**
 * @brief iflyos 云端下发撤销鉴权消息
 *
 * @param msg
 * @param req_id
 */
void sys_on_revoke_auth(const char *const msg, const char *const req_id)
{
	LISA_LOGD(TAG, "revoke auth, msg: %s, req id: %s", msg, req_id);
}
/**
 * @brief iflyos 云端下发关机消息
 *
 * @param msg
 * @param req_id
 */
void sys_on_power_off(const char *const msg, const char *const req_id)
{
	LISA_LOGD(TAG, "power off, msg: %s, req id: %s", msg, req_id);
}
/**
 * @brief iflyos 云端下发音量调节消息
 *
 * @param msg
 * @param req_id
 */
void spk_on_set_vol(const char *const msg, const char *const req_id)
{
	LISA_LOGD(TAG, "set vol, msg: %s, req id: %s", msg, req_id);
}

/**
 * @brief 获取设备端状态
 *
 * @return lisa_evs_state_t
 */
lisa_evs_state_t evs_get_state()
{
	//需要根据实际的状态来
	LISA_LOGD(TAG, "lisa evs get all state");
	lisa_evs_state_t state = {0};
	// system相关
	strcpy(state.system_state.version, "1.3");
	strcpy(state.system_state.firmware_version, "1.0.1");
	// recognizer相关
	strcpy(state.recognizer_state.version, "1.1");
	// playback相关
	strcpy(state.playback_state.version, "1.2");
	strcpy(state.playback_state.resource_id, "");
	state.playback_state.state = LISA_EVS_PLAYSTATE_IDLE;
	state.playback_state.offset = 0;
	// speaker相关
	strcpy(state.speaker_state.version, "1.0");
	state.speaker_state.volume = 100;
	// platform相关
	strcpy(state.platform_info.platform_name, "FreeRTOS");
	strcpy(state.platform_info.platform_version, "10.2.1");
	return state;
}

/**
 * @brief 初始化iflyos设备账号信息
 *
 */
static lisa_evs_config_t s_lisa_evs_config = {.client_id = PRODUCT_CLIENT_ID,
		.device_id = PRODUCT_DEVICE_ID,
		.ota_secret = PRODUCT_OTA_SECRET_ID};

/**
 * @brief 初始化websocket回调接口
 *
 */
const static lisa_evs_websocket_cb_t s_ws_cb = {.connected = ws_connected_cb,
		.connect_failed = ws_connect_failed_cb,
		.disconnected = ws_disconnected_cb,
		.message = ws_message_cb,
		.send_failed = ws_send_failed_cb};
/**
 * @brief 初始化iflyos系统消息回调接口
 *
 */
const static lisa_evs_system_cb_t s_sys_cb = {.on_ping = sys_on_ping,
		.on_error = sys_on_error,
		.on_check_software_update = sys_on_check_software_update,
		.on_update_software = sys_on_update_software,
		.on_revoke_auth = sys_on_revoke_auth,
		.on_power_off = sys_on_power_off};
/**
 * @brief 初始化iflyos扬声器回调接口
 *
 */
const static lisa_evs_speaker_cb_t s_spk_cb = {.on_set_vol = spk_on_set_vol};
/**
 * @brief 初始化 iflyos websocket、系统消息、扬声器接口回调
 *
 */
const static lisa_evs_cb_t s_evs_cb = {
		.websocket_cb = &s_ws_cb, .system_cb = &s_sys_cb, .speaker_cb = &s_spk_cb};
/**
 * @brief 初始化iflyos设备状态获取回调
 *
 */
const static lisa_evs_state_cb_t s_get_state_cb = {.get_state = evs_get_state};

/**
 * @brief 从缓冲中循环读取音频，进行语音识别
 *
 */
void loop_trans_audio()
{
	struct fs_file_t fp;
	fs_file_t_init(&fp);
	int err = fs_open(&fp, L1FS_FILE_PATH(audio1.bin), FS_O_READ);
	if (err) {
		LISA_LOGE(TAG, "open audio failed, err = %d", err);
		goto ERR_OPEN_FILE;
	}
	fs_seek(&fp, 0, FS_SEEK_END);
	const long audio_mock_data_len = fs_tell(&fp);
	LISA_LOGD(TAG, "audio file size %ld\n", audio_mock_data_len);
	fs_seek(&fp, 0, FS_SEEK_SET);
	uint8_t *audio_mock_data = lisa_mem_alloc(LOOP_AUDIO_TEST_SIZE);
	if (!audio_mock_data) {
		LISA_LOGE(TAG, "malloc audio buf failed");
		goto ERR_MALLOC;
	}

	lisa_evs_uuid_generate_string(s_request_id);
	lisa_evs_rec_audio_param_t param;
	param.format = EVS_REC_AUDIO_FORMAT_SPEEX;
	param.profile = EVS_REC_AUDIO_PROFILE_CLOSE_TALK;
	param.enable_vad = false;
	param.vad_eos = 800;
	param.reply_key = "";
	param.translation = false;
	lisa_evs_rec_audio_start(s_handle->rec, s_request_id, &param);

	const int sleep_time = LOOP_AUDIO_TEST_SIZE / 32;
	LISA_LOGD(TAG, "send audio begin, sleep time %dms", sleep_time);

	for (int i = 0; i < audio_mock_data_len; i += LOOP_AUDIO_TEST_SIZE) {
		fs_read(&fp, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_evs_rec_send_audio(s_handle->rec, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_thread_mdelay(sleep_time);
	}

	LISA_LOGD(TAG, "Now End Send Audio");
	lisa_evs_rec_audio_end(s_handle->rec, s_request_id);

ERR_MALLOC:
	fs_close(&fp);
ERR_OPEN_FILE:
	if (s_test_send_audio_thread) {
		lisa_thread_t *temp_thread = s_test_send_audio_thread;
		s_test_send_audio_thread = NULL;
		lisa_thread_delete(temp_thread);
	}
}

/**
 * @brief 从缓冲中循环读取音频，进行语音翻译
 *
 */
void loop_trans_audio_translate()
{
	struct fs_file_t fp;
	fs_file_t_init(&fp);
	int err = fs_open(&fp, L1FS_FILE_PATH(audio1.bin), FS_O_READ);
	if (err) {
		LISA_LOGE(TAG, "open audio failed, err = %d", err);
		goto ERR_OPEN_FILE;
	}
	fs_seek(&fp, 0, FS_SEEK_END);
	const long audio_mock_data_len = fs_tell(&fp);
	LISA_LOGD(TAG, "audio file size %ld\n", audio_mock_data_len);
	fs_seek(&fp, 0, FS_SEEK_SET);
	uint8_t *audio_mock_data = lisa_mem_alloc(LOOP_AUDIO_TEST_SIZE);
	if (!audio_mock_data) {
		LISA_LOGE(TAG, "malloc audio buf failed");
		goto ERR_MALLOC;
	}

	lisa_evs_uuid_generate_string(s_request_id);
	lisa_evs_rec_audio_param_t param;
	param.format = EVS_REC_AUDIO_FORMAT_SPEEX;
	param.profile = EVS_REC_AUDIO_PROFILE_CLOSE_TALK;
	param.enable_vad = false;
	param.vad_eos = 800;
	param.reply_key = "";
	param.translation = true;
	lisa_evs_rec_audio_start(s_handle->rec, s_request_id, &param);

	const int sleep_time = LOOP_AUDIO_TEST_SIZE / 32;
	LISA_LOGD(TAG, "send audio begin, sleep time %dms", sleep_time);

	for (int i = 0; i < audio_mock_data_len; i += LOOP_AUDIO_TEST_SIZE) {
		fs_read(&fp, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_evs_rec_send_audio(s_handle->rec, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_thread_mdelay(sleep_time);
	}

	LISA_LOGD(TAG, "Now End Send Audio");
	lisa_evs_rec_audio_end(s_handle->rec, s_request_id);

ERR_MALLOC:
	fs_close(&fp);
ERR_OPEN_FILE:
	if (s_test_send_audio_thread) {
		lisa_thread_t *temp_thread = s_test_send_audio_thread;
		s_test_send_audio_thread = NULL;
		lisa_thread_delete(temp_thread);
	}
}

/**
 * @brief 从缓冲中循环读取音频，进行口语评测
 *
 */
void loop_evalute_audio()
{
	struct fs_file_t fp;
	fs_file_t_init(&fp);
	int err = fs_open(&fp, L1FS_FILE_PATH(audio1.bin), FS_O_READ);
	if (err) {
		LISA_LOGE(TAG, "open audio failed, err = %d", err);
		goto ERR_OPEN_FILE;
	}
	fs_seek(&fp, 0, FS_SEEK_END);
	const long audio_mock_data_len = fs_tell(&fp);
	LISA_LOGD(TAG, "audio file size %ld\n", audio_mock_data_len);
	fs_seek(&fp, 0, FS_SEEK_SET);
	uint8_t *audio_mock_data = lisa_mem_alloc(LOOP_AUDIO_TEST_SIZE);
	if (!audio_mock_data) {
		LISA_LOGE(TAG, "malloc audio buf failed");
		goto ERR_MALLOC;
	}

	const int sleep_time = LOOP_AUDIO_TEST_SIZE / 32;
	LISA_LOGD(TAG, "send evalute begin, sleep time %dms", sleep_time);

	lisa_evs_uuid_generate_string(s_request_id);
	lisa_evs_rec_audio_param_t param;
	param.format = EVS_REC_AUDIO_FORMAT_SPEEX;
	param.profile = EVS_REC_AUDIO_PROFILE_CLOSE_TALK;
	param.enable_vad = false;
	param.vad_eos = 300;

	// 评测设置
	lisa_evs_rec_evaluate_param_t evalute_param;
	evalute_param.category = EVS_REC_EVALUATE_READ_SENTENCE;
	evalute_param.language = EVS_REC_EVALUATE_ZH_CN;
	evalute_param.text = "深圳今天的天气";  //评测文本

	lisa_evs_rec_evaluate_start(s_handle->rec, s_request_id, &param, &evalute_param);

	for (int i = 0; i < audio_mock_data_len; i += LOOP_AUDIO_TEST_SIZE) {
		fs_read(&fp, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_evs_rec_send_audio(s_handle->rec, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_thread_mdelay(sleep_time);
	}

	LISA_LOGD(TAG, "Now End Send Audio");
	lisa_evs_rec_evaluate_end(s_handle->rec, s_request_id);

ERR_MALLOC:
	fs_close(&fp);
ERR_OPEN_FILE:
	if (s_test_evaluate_thread) {
		lisa_thread_t *temp = s_test_evaluate_thread;
		s_test_evaluate_thread = NULL;
		lisa_thread_delete(temp);
	}
}

/**
 * @brief 启动语音识别
 *
 */
void run_audio_recognition()
{
	// 语音识别
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "test_send_thread";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	// 发送音频
	s_test_send_audio_thread = lisa_thread_create(&thread_attr, loop_trans_audio, NULL);
	if (s_test_send_audio_thread == NULL) {
		LISA_LOGE(TAG, "thread_create failed");
		return;
	}
}

/**
 * @brief 启动语音翻译
 *
 */
void run_audio_translate()
{
	// 语音识别
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "test_send_thread";
	thread_attr.stack_size = 2 * 1024;
	// thread_attr.stack_size = K_THREAD_STACK_SIZEOF(send_ocr_simulation_stack_area);
	// thread_attr.stack = send_ocr_simulation_stack_area;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	// 发送音频
	s_test_send_audio_thread = lisa_thread_create(&thread_attr, loop_trans_audio_translate, NULL);
	if (s_test_send_audio_thread == NULL) {
		LISA_LOGE(TAG, "thread_create failed");
		return;
	}
}

/**
 * @brief 启动口语评测
 *
 */
void run_audio_evaluate()
{
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "test_send_thread";
	thread_attr.stack_size = 2 * 1024;
	// thread_attr.stack_size = K_THREAD_STACK_SIZEOF(send_ocr_simulation_stack_area);
	// thread_attr.stack = send_ocr_simulation_stack_area;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	// 评测接口
	s_test_evaluate_thread = lisa_thread_create(&thread_attr, loop_evalute_audio, NULL);
	if (s_test_evaluate_thread == NULL) {
		LISA_LOGE(TAG, "thread_create failed");
		return;
	}
}

/**
 * @brief 启动查词功能
 *
 */
run_dictionary_lookup(const char *word)
{
	lisa_dictionary_lookup(s_handle, word, &s_dictionary_lookup_cb);
}

typedef struct {
	unsigned char *name;
	unsigned char *url;
	uint32_t addr;
	uint32_t size;
	unsigned char *md5;
} csk_ota_task_info;

static csk_ota_task_info **s_ota_tasks = NULL;
static int s_ota_task_size = 0;

static void _get_ota_info_result(lisa_http_data_t *data)
{
	if (data->len > 0) {
		cJSON *root = cJSON_Parse(data->buf);
		if (root) {
			cJSON *res = cJSON_GetObjectItem(root, "resources");
			if (res) {
				int size = cJSON_GetArraySize(res);
				s_ota_task_size = size;
				if (size > 0) s_ota_tasks = lisa_mem_calloc(size, sizeof(csk_ota_task_info *));
				for (int i = 0; i < size; i++) {
					csk_ota_task_info *info = lisa_mem_calloc(1, sizeof(csk_ota_task_info));
					cJSON *item = cJSON_GetArrayItem(res, i);
					cJSON *param = cJSON_GetObjectItem(item, "file_path");
					if (param && param->valuestring) {
						info->url = lisa_mem_calloc(1, strlen(param->valuestring) + 1);
						strcpy(info->url, param->valuestring);
					}
					param = cJSON_GetObjectItem(item, "md5");
					if (param && param->valuestring) {
						info->md5 = lisa_mem_calloc(1, strlen(param->valuestring) + 1);
						strcpy(info->md5, param->valuestring);
					}
					param = cJSON_GetObjectItem(item, "file_name");
					if (param && param->valuestring) {
						info->name = lisa_mem_calloc(1, strlen(param->valuestring) + 1);
						strcpy(info->name, param->valuestring);
					}
					param = cJSON_GetObjectItem(item, "addr");
					if (param && param->valuestring) {
						sscanf(param->valuestring, "%x", &info->addr);
					}
					param = cJSON_GetObjectItem(item, "file_size");
					if (param) {
						info->size = param->valueint;
					}
					s_ota_tasks[i] = info;
				}
			}
			cJSON_Delete(root);
		}
	}
}

#define HTTP_TEST_HEAD ("Content-Type:application/x-www-form-urlencoded;charset=utf-8")
static void *_get_http_header(void)
{
	return HTTP_TEST_HEAD;
}

static _request_ota_info(const char *url)
{
	char *http_url = lisa_mem_calloc(1, strlen(url));
	if (sscanf(url, "https%s", http_url) != 1) {
		lisa_mem_free(http_url);
		return;
	}
	char *req_url = lisa_mem_calloc(1, strlen(url));
	sprintf(req_url, "http%s", http_url);
	lisa_mem_free(http_url);
	lisa_http_request_t *http_param = NULL;

	http_param = (lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		return LISA_FAIL;
	}

	http_param->method = LISA_HTTP_GET;
	http_param->url = req_url;
	http_param->headers = (void *)_get_http_header;
	http_param->timeout_ms = 10;
	http_param->body_len = 0;
	http_param->body = NULL;
	http_param->on_data = (void *)_get_ota_info_result;

	http_param->user = NULL;
	lisa_http_t *check_ota_info_http = lisa_http_init(http_param);
	if (!check_ota_info_http) {
		LISA_LOGE(TAG, " http  error");
		goto ERR;
	}

	if (lisa_http_perform(check_ota_info_http) != 0) {
		LISA_LOGE(TAG, "http get request info err..");
		goto ERR;
	}

	for (int i = 0; i < s_ota_task_size; i++) {
		char *replace_url = lisa_mem_calloc(1, strlen(req_url) + strlen(s_ota_tasks[i]->url) - 7);
		snprintf(replace_url, strlen(req_url) - 7, "%s", req_url);
		sprintf(replace_url, "%s%s", replace_url, s_ota_tasks[i]->url);
		lisa_mem_free(s_ota_tasks[i]->url);
		s_ota_tasks[i]->url = replace_url;
	}

ERR:
	if (req_url) {
		lisa_mem_free(req_url);
	}
	if (check_ota_info_http) {
		lisa_http_cleanup(check_ota_info_http);
		check_ota_info_http = NULL;
	}
	if (http_param) {
		lisa_mem_free(http_param);
	}
}

#define MAX_HEADER_LEN 512
static char *s_ota_header[MAX_HEADER_LEN];
#define SIGN_SEP ":"

static int ota_req_params_signature(const char *time, const char *rand, char **out)
{
	mbedtls_sha1_context ctx;
	char output[21] = {0};
	mbedtls_sha1_init(&ctx);
	mbedtls_sha1_starts(&ctx);
	mbedtls_sha1_update(&ctx, s_handle->config->client_id, strlen(s_handle->config->client_id));
	mbedtls_sha1_update(&ctx, SIGN_SEP, strlen(SIGN_SEP));
	mbedtls_sha1_update(&ctx, s_handle->config->device_id, strlen(s_handle->config->device_id));
	mbedtls_sha1_update(&ctx, SIGN_SEP, strlen(SIGN_SEP));
	mbedtls_sha1_update(&ctx, time, strlen(time));
	mbedtls_sha1_update(&ctx, SIGN_SEP, strlen(SIGN_SEP));
	mbedtls_sha1_update(&ctx, rand, strlen(rand));
	mbedtls_sha1_update(&ctx, SIGN_SEP, strlen(SIGN_SEP));
	mbedtls_sha1_update(&ctx, s_handle->config->ota_secret, strlen(s_handle->config->ota_secret));
	mbedtls_sha1_finish(&ctx, output);
	mbedtls_sha1_free(&ctx);
	char *out_inner = (char *)lisa_mem_calloc(1, 41);
	if (out_inner == NULL) {
		LISA_LOGE(TAG, "params signature failed, malloc failed");
		return -1;
	}
	for (int i = 0; i < 20; i++) {
		sprintf(out_inner + i * 2, "%02x", (0x000000FF & output[i]));
	}
	*out = out_inner;
	return 0;
}

static void *get_ota_info_headers(void)
{
	memset(s_ota_header, 0, MAX_HEADER_LEN);
	uint32_t time = lisa_os_get_time();
	uint32_t random = lisa_rand32() % 65535;
	char time_str[12] = {0};
	sprintf(time_str, "%lu", time);
	char rand_str[12] = {0};
	sprintf(rand_str, "%lu", random);
	char *signature = NULL;
	if (ota_req_params_signature(time_str, rand_str, &signature) != 0) {
		return "";
	}
	sprintf(s_ota_header,
			"X-Client-ID:%s&X-Device-ID:%s&X-Version-ID:%d&X-Timestamp:%s&X-Nonce:%s&X-Signature:%"
			"s",
			s_handle->config->client_id, s_handle->config->device_id, 0, time_str, rand_str,
			signature);
	lisa_mem_free(signature);
	LISA_LOGI(TAG, "ota_get_heads headers: %s", s_ota_header);
	return s_ota_header;
}

static char *s_ota_info_url = NULL;
static void _check_ota_result(lisa_http_data_t *data)
{
	if (data->len > 0) {
		cJSON *root = cJSON_Parse(data->buf);
		if (root != NULL) {
			cJSON *item = cJSON_GetArrayItem(root, 0);
			if (!item) {
				cJSON_Delete(root);
				return;
			}
			cJSON *param = cJSON_GetObjectItem(item, "url");
			if (param && param->valuestring) {
				s_ota_info_url = lisa_mem_calloc(1, strlen(param->valuestring) + 1);
				strcpy(s_ota_info_url, param->valuestring);
			}

			cJSON_Delete(root);
		}
	}
}

static void _check_ota_result_event_cb(lisa_http_event_t *event)
{
	LISA_LOGD(TAG, "_check_ota_result_event_cb event->what [%d]", event->what);
}

static run_check_ota_info()
{
	lisa_http_request_t *http_param = NULL;

	http_param = (lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		return LISA_FAIL;
	}

	http_param->method = LISA_HTTP_GET;
	http_param->url = URL_GET_OTA_INFO;
	http_param->headers = (void *)get_ota_info_headers;
	http_param->timeout_ms = 10;
	http_param->body_len = 0;
	http_param->body = NULL;
	http_param->on_data = (void *)_check_ota_result;
	http_param->on_event = (void *)_check_ota_result_event_cb;

	http_param->user = NULL;
	lisa_http_t *check_ota_info_http = lisa_http_init(http_param);
	if (!check_ota_info_http) {
		LISA_LOGE(TAG, " http  error");
		goto ERR;
	}

	if (lisa_http_perform(check_ota_info_http) != 0) {
		LISA_LOGE(TAG, "http get request info err..");
		goto ERR;
	}

ERR:
	if (check_ota_info_http) {
		lisa_http_cleanup(check_ota_info_http);
		check_ota_info_http = NULL;
	}
	if (http_param) {
		lisa_mem_free(http_param);
	}

	// parser ota info
	if (s_ota_info_url) {
		_request_ota_info(s_ota_info_url);
		lisa_mem_free(s_ota_info_url);
		s_ota_info_url = NULL;

		if (s_ota_task_size > 0 && s_ota_tasks) {
			// csk_ota_task_perform(s_ota_tasks, s_ota_task_size, &_ota_contr_cb);
			for (int i = 0; i < s_ota_task_size; i++) {
				lisa_mem_free(s_ota_tasks[i]->md5);
				lisa_mem_free(s_ota_tasks[i]->url);
				lisa_mem_free(s_ota_tasks[i]->name);
				lisa_mem_free(s_ota_tasks[i]);
			}
			lisa_mem_free(s_ota_tasks);
			s_ota_tasks = NULL;
			s_ota_task_size = 0;
		}
	}
	return;
}

static void *_get_evaluate_lev_cb(const uint8_t *const data, uint32_t len)
{
	char *response = lisa_mem_alloc(len + 1);
	memcpy(response, data, len);
	response[len] = '\0';
	LISA_LOGE(TAG, "response = %s", response);
	lisa_mem_free(response);
}

void loop_evalute_audio_quest()
{
	// 获取难度
	lisa_err_t ret = lisa_get_evaluate_level(s_handle, _get_evaluate_lev_cb);
	if (ret == LISA_OK) LISA_LOGD(TAG, "get evaluate level success");
	if (ret == LISA_FAIL) LISA_LOGD(TAG, "get evaluate level success");
	// 获取随即题目
	ret = lisa_get_evaluate_quest(s_handle, "1", "answer", _get_evaluate_lev_cb);
	if (ret == LISA_OK) LISA_LOGD(TAG, "get evaluate question success");
	if (ret == LISA_FAIL) LISA_LOGD(TAG, "get evaluate question success");

	struct fs_file_t fp;
	fs_file_t_init(&fp);
	int err = fs_open(&fp, L1FS_FILE_PATH(audio2.bin), FS_O_READ);
	if (err) {
		LISA_LOGE(TAG, "open audio failed, err = %d", err);
		goto ERR_OPEN_FILE;
	}
	fs_seek(&fp, 0, FS_SEEK_END);
	const long audio_mock_data_len = fs_tell(&fp);
	LISA_LOGD(TAG, "audio file size %ld\n", audio_mock_data_len);
	fs_seek(&fp, 0, FS_SEEK_SET);
	uint8_t *audio_mock_data = lisa_mem_alloc(LOOP_AUDIO_TEST_SIZE);
	if (!audio_mock_data) {
		LISA_LOGE(TAG, "malloc audio buf failed");
		goto ERR_MALLOC;
	}

	const int sleep_time = LOOP_AUDIO_TEST_SIZE / 32;
	LISA_LOGD(TAG, "send evalute begin, sleep time %dms", sleep_time);

	lisa_evs_uuid_generate_string(s_request_id);
	lisa_evs_rec_audio_param_t param;
	param.format = EVS_REC_AUDIO_FORMAT_SPEEX;
	param.profile = EVS_REC_AUDIO_PROFILE_CLOSE_TALK;
	param.enable_vad = false;
	param.vad_eos = 300;

	// 评测设置
	lisa_evs_rec_evaluate_param_t evalute_param;
	evalute_param.category = EVS_REC_EVALUATE_READ_CHOICE;
	evalute_param.language = EVS_REC_EVALUATE_EN_US;
	evalute_param.text = "[choice]\n1. Kate\n[keywords]\nKate";  //评测文本

	lisa_evs_rec_evaluate_start(s_handle->rec, s_request_id, &param, &evalute_param);

	for (int i = 0; i < audio_mock_data_len; i += LOOP_AUDIO_TEST_SIZE) {
		fs_read(&fp, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_evs_rec_send_audio(s_handle->rec, audio_mock_data, LOOP_AUDIO_TEST_SIZE);
		lisa_thread_mdelay(sleep_time);
	}

	LISA_LOGD(TAG, "Now End Send Audio");
	lisa_evs_rec_evaluate_end(s_handle->rec, s_request_id);

ERR_MALLOC:
	fs_close(&fp);
ERR_OPEN_FILE:
	if (s_test_evaluate_thread) {
		lisa_thread_t *temp = s_test_evaluate_thread;
		s_test_evaluate_thread = NULL;
		lisa_thread_delete(temp);
	}
}

void run_oral_practice()
{
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "test_send_thread";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	// 评测接口
	s_test_evaluate_thread = lisa_thread_create(&thread_attr, loop_evalute_audio_quest, NULL);
	if (s_test_evaluate_thread == NULL) {
		LISA_LOGE(TAG, "thread_create failed");
		return;
	}
}

void run_ol_tts_speed(const char *lang, const char *speed)
{
	// (english,mandarin)
	// (slow,middle,fast)
	lisa_set_online_tts_speed(s_handle, lang, speed, _get_evaluate_lev_cb);
	lisa_get_online_tts_speed(s_handle, _get_evaluate_lev_cb);
}

/**
 * @brief	feedback upload
 */
static void _test_feedback_upload(const uint8_t isonline)
{
	lisa_text_info_t feedback_array[4] = {{0, "ocr", "word", "feed"},
			{0, "translation", "sentence", "hello world feedback_array  feedback_array"},
			{0, "ocr", "sentence", "feedback_array feedback_array  feedback_array"},
			{0, "translation", "word", "feedback_array feedback_array  feedback_array"}};
	if (lisa_feedback_upload(s_handle, isonline, &feedback_array, 2, _get_evaluate_lev_cb) == 0) {
		LISA_LOGD(TAG, "feedback upload success \n");
	}
}

/**
 * @brief	feedback sync test
 */
static void _test_feedback_sync(void)
{
	if (lisa_feedback_sync_text(s_handle, _get_evaluate_lev_cb) == 0) {
		printf("lisa_feedback_sync_text success\n");
	}
}

/**
 * @brief	notebook upload
 */
static void _test_notebook_upload(const uint8_t is_online)
{
	lisa_text_info_t feedback_array[4] = {{0, "work", "english", "hello"},
			{0, "text", "english", "welcome to listenai"}, {0, "word", "chinese", "你好"},
			{0, "text", "chinese", "聆思欢迎你"}};
	if (lisa_notebook_create(s_handle, is_online, &feedback_array, 4, _get_evaluate_lev_cb) == 0) {
		LISA_LOGD(TAG, "feedback upload success \n");
	}
}

/**
 * @brief	feedback sync test
 */
static void _test_notebook_sync(void)
{
	if (lisa_notebook_sync_text(s_handle, _get_evaluate_lev_cb) == 0) {
		printf("lisa_feedback_sync_text success\n");
	}
}

/**
 * @brief
 *
 * @param data
 * @param len
 *
 * @retval
 */
static void *_notebook_getlist_cb(const uint8_t *const data, uint32_t len)
{
	// LISA_LOGE(TAG, "data = %s", data);
	if (len > 0) {
		cJSON *root = cJSON_Parse(data);
		if (root) {
			cJSON *res = cJSON_GetObjectItem(root, "notebook");
			if (res) {
				int size = cJSON_GetArraySize(res);
				s_word_cnt = size;
				// if (size > 0) s_ota_tasks = lisa_mem_calloc(size, sizeof(csk_ota_task_info *));
				for (int i = 0; i < s_word_cnt + s_lastcnt; i++) {
					//	csk_ota_task_info *info = lisa_mem_calloc(1, sizeof(csk_ota_task_info));
					cJSON *item = cJSON_GetArrayItem(res, i);
					cJSON *param = cJSON_GetObjectItem(item, "id");
					if (param && param->valuedouble) {
						// printf("[%d].id	%d \n", i, param->valueint);
						s_notebook[i + s_lastcnt].id = param->valueint;
					}
					param = cJSON_GetObjectItem(item, "type");
					if (param && param->valuestring) {
						// printf("[%d].type	%s \n", i, param->valuestring);
						strcpy(s_notebook[i + s_lastcnt].type, param->valuestring);
					}
					param = cJSON_GetObjectItem(item, "language_type");
					if (param && param->valuestring) {
						// printf("[%d].language_type	%s \n", i, param->valuestring);
						strcpy(s_notebook[i + s_lastcnt].content_type, param->valuestring);
					}
					param = cJSON_GetObjectItem(item, "content");
					if (param && param->valuestring) {
						// printf("[%d].content	%s \n", i, param->valuestring);
						strcpy(s_notebook[i + s_lastcnt].message, param->valuestring);
					}
				}
			}
			res = cJSON_GetObjectItem(root, "page");
			if (res && res->valueint) {
				/*printf("page	%d \n", res->valueint);*/
				s_notepage = res->valueint;
			}

			res = cJSON_GetObjectItem(root, "size");
			if (res && res->valueint) {
				/*printf("size	%d \n", res->valueint);*/
				s_notesize = res->valueint;
			}

			res = cJSON_GetObjectItem(root, "total");
			if (res && res->valueint) {
				/*printf("total	%d \n", res->valueint);*/
				s_notetotal = res->valueint;
			}

			cJSON_Delete(root);
		}
	}

	lisa_semaphore_give(s_notebook_sem);
}

/**
 * @brief	notebook test
 */
static void _test_notebook_getlist(const uint8_t is_online, const char *const type_name)
{
	LISA_LOGE(TAG, "_test_notebook_getlist %s get %s\n", (is_online == 1) ? "online" : "offline",
			type_name);

	lisa_err_t ret = LISA_OK;
	s_notebook_sem = lisa_semaphore_create(1);
	s_pagesize = 5;

	do {
		ret = lisa_notebook_get_list(
				s_handle, is_online, s_pagenum++, s_pagesize, type_name, _notebook_getlist_cb);
		if (ret != LISA_OK) {
			LISA_LOGE(TAG, "notebook:  online test get_list failed \n");
			s_notetotal = s_notesize = 0;
			s_notepage = s_pagecnt = s_lastcnt = 0;
			s_pagenum = 1;
			return;
		}

		lisa_semaphore_take(s_notebook_sem, LISA_WAIT_FOREVER);
		if (s_notetotal != 0) {
			printk("Total: %d s_pagesize %d\n", s_notetotal, s_notesize);
			s_pagecnt = s_notetotal / s_notesize;
			if (s_notetotal % s_notesize != 0) s_pagecnt += 1;
			printk("【Page %d/%d】\n", s_notepage, s_pagecnt);
			for (int i = s_lastcnt; i < s_word_cnt + s_lastcnt; i++) {
				printk("[%d]-->[%s][%s][%d] %s\n", i, s_notebook[i].type,
						s_notebook[i].content_type, s_notebook[i].id, s_notebook[i].message);
			}
			s_lastcnt += s_word_cnt;
		}
	} while (s_notepage < s_pagecnt);
	printk("查询结束!\n");
	s_notetotal = s_notesize = 0;
	s_notepage = s_pagecnt = s_lastcnt = 0;
	s_pagenum = 1;
	lisa_semaphore_delete(s_notebook_sem);
	s_notebook_sem = NULL;
}

/**
 * @brief test notebook delete func
 */
static void _test_notebook_delete(void)
{
	lisa_err_t ret = LISA_OK;
	uint32_t id_table[100] = {0};

	for (int i = 0; i < 4; i++) {
		id_table[i] = s_notebook[i].id;
		// printf("[%d] %d\n", i, id_table[i]);
	}

	ret = lisa_notebook_delete(s_handle, id_table, 4, _get_evaluate_lev_cb);
	if (ret != LISA_OK) {
		printf("notebook:  online test create failed \n");
		return;
	}
}

/*
 *mbedtls_time_t _mbedtls_time_func(mbedtls_time_t* time) {
 *    return (mbedtls_time_t)(lisa_os_get_time() + 1649830875);
 *}
 */

/**
 * @brief 设备端初始化
 *
 */
void run_device_init()
{
	// ssl 初始化
	// mbedtls_platform_set_calloc_free(lisa_mem_calloc, lisa_mem_free);
	// mbedtls_platform_set_time(_mbedtls_time_func);
	// 1、工具类初始化 <= lisa_evs_sdk接口
	evs_utils_init();
	lisa_pref_init();
	// 2、休眠1s, 确保网络以及可以正常访问
	lisa_thread_delay(1);
	// 3、create lisa

	char *client_id = NULL;
	if (lisa_pref_get_string(PREF_KEY_CLIENT_ID, &client_id) == LISA_OK) {
		s_lisa_evs_config.client_id = lisa_mem_alloc(strlen(client_id) + 1);
		strcpy(s_lisa_evs_config.client_id, client_id);
		lisa_mem_free(client_id);
		LISA_LOGD(TAG, "client id: %s", s_lisa_evs_config.client_id);
	}
	char *device_id = NULL;
	if (lisa_pref_get_string(PREF_KEY_DEVICE_ID, &device_id) == LISA_OK) {
		s_lisa_evs_config.device_id = lisa_mem_alloc(strlen(device_id) + 1);
		strcpy(s_lisa_evs_config.device_id, device_id);
		lisa_mem_free(device_id);
		LISA_LOGD(TAG, "device id: %s", s_lisa_evs_config.device_id);
	}
	char *ota_secret = NULL;
	if (lisa_pref_get_string(PREF_KEY_OTA_SECRET, &ota_secret) == LISA_OK) {
		s_lisa_evs_config.ota_secret = lisa_mem_alloc(strlen(ota_secret) + 1);
		strcpy(s_lisa_evs_config.ota_secret, ota_secret);
		lisa_mem_free(ota_secret);
		LISA_LOGD(TAG, "ota secret: %s", s_lisa_evs_config.ota_secret);
	}
	s_handle = lisa_evs_create(&s_lisa_evs_config, &s_evs_cb, &s_get_state_cb);
	// 4、连上网以后进行EVS鉴权操作
	start_evs_auth(&s_lisa_evs_config);

	char *accessToken = get_evs_access_token();
	LISA_LOGD(TAG, "accessToken : %s", accessToken);
	strcpy(s_request_access_token, accessToken);
	// 5、设置access token
	lisa_evs_set_access_token(s_handle, s_request_access_token, strlen(s_request_access_token));
	// 6、ws连接EVS云端
	// lisa_evs_connect(s_handle, s_request_access_token);
}

/**
 * @brief 启动扫描笔app
 *
 */
void start_scanpen_app()
{
	/*设备端初始化*/
	run_device_init();

	connect_iflyos();
	return;
}

#define PEN_HEAD_BUTTON KEY1
static button_handle *pen_head_button;
static void pen_head_button_cb(BUTTON_STATE sta, void *arg)
{
	/* long button has the PRESS/RELEASE state */
	if (sta == PRESS) {
		printf("pen head button press\n");
		/*_video_init();*/
		send_uart_scan_start();

		LISA_LOGD(TAG, "send_uart_scan_start\n");
		/*lisa_vcapture_start(demo_handler);*/
		/*lisa_thread_delay(100);*/
	} else if (sta == RELEASE) {
		LISA_LOGD(TAG, "send_uart_scan_stop\n");
		send_uart_scan_end();
		/*lisa_vcapture_stop(demo_handler);*/
		/*lisa_vcapture_destroy(demo_handler);*/
		printf("pen button release\n");
	}
}

void pen_head_button_exit(void)
{
	if (pen_head_button) {
		pen_head_button->stop(pen_head_button);
		/* destory buttons */
		pen_head_button->destroy(pen_head_button);
	}
}

int pen_head_button_init(void)
{
	buttons_init();
	pen_head_button = create_long_button(PEN_HEAD_BUTTON, 50);
	pen_head_button->cb = pen_head_button_cb;

	pen_head_button->start(pen_head_button);
}

/**
 * @brief 启动在线ocr识别
 *
 */
void ocr_test_online_start()
{
	if (!s_send_ocr_simulation_image_buffer) {
		unsigned char *buffer = lisa_mem_alloc(64 * 1024);

		s_send_ocr_simulation_image_buffer = lisa_ring_buffer_init(buffer, 64 * 1024);

		s_ocr_image_sema = lisa_semaphore_create(2);
		create_spi_task_protocol_thread();
		create_ocr_recognize_thread();
	}

	pen_head_button_init();
}
/**
 * @brief 停止在线ocr识别
 *
 */
void ocr_test_online_stop()
{
	send_uart_scan_end();
	/*lisa_vcapture_stop(demo_handler);*/
	/*lisa_vcapture_destroy(demo_handler);*/

	pen_head_button_exit();
}

// -----------------------------------------------shell-------------------------------------------
// -----------------------------------------------shell-------------------------------------------
static int _test_ocr_single_line_online(const struct shell *shell, size_t argc, char **argv)
{
	int type = atoi(argv[1]);
	if (type == 1) {
		ocr_test_online_start();
	} else {
		ocr_test_online_stop();
	}
	return 0;
}

static int _test_ocr_single_line(const struct shell *shell, size_t argc, char **argv)
{
	s_singleline = true;
	run_ocr_recognition();
	return 0;
}

static int _test_ocr_multi_line(const struct shell *shell, size_t argc, char **argv)
{
	s_singleline = false;
	run_ocr_recognition();
	return 0;
}

static int _test_audio_trans(const struct shell *shell, size_t argc, char **argv)
{
	run_audio_translate();
	return 0;
}

static int _test_online_dict(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		LISA_LOGE(TAG, "not input word");
		return 0;
	}
	run_dictionary_lookup(argv[1]);
	return 0;
}

static int _test_audio_evaluate(const struct shell *shell, size_t argc, char **argv)
{
	run_audio_evaluate();
	return 0;
}

static int _test_recognition(const struct shell *shell, size_t argc, char **argv)
{
	run_audio_recognition();
	return 0;
}

static int _test_oral_practice(const struct shell *shell, size_t argc, char **argv)
{
	run_oral_practice();
	return 0;
}

/**
 * @brief	feedback test
 */
static int _test_feedback(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		LISA_LOGE(TAG, "miss input param");
		return;
	}
	int type = atoi(argv[1]);

	switch (type) {
		case 0:
			_test_feedback_upload(0);
			break;
		case 1:
			_test_feedback_upload(1);
			break;
		case 2:
			_test_feedback_sync();
			break;
		defualt:
			LISA_LOGD(TAG, "err subcmd %d\n", type);
	};
}

/**
 * @brief	feedback test
 */
static int _test_notebook(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		LISA_LOGE(TAG, "miss input param");
		return;
	}
	int type = atoi(argv[1]);

	switch (type) {
		case 0:
			_test_notebook_upload(0);
			break;
		case 1:
			_test_notebook_upload(1);
			break;
		case 2:
			_test_notebook_sync();
			break;
		case 3:
			_test_notebook_getlist(1, "word");
			_test_notebook_getlist(1, "text");
			break;
		case 4:
			_test_notebook_getlist(0, "word");
			_test_notebook_getlist(0, "text");
			break;
		case 5:
			_test_notebook_upload(1);
			lisa_thread_delay(1);
			_test_notebook_getlist(1, "word");
			_test_notebook_getlist(1, "text");
			lisa_thread_delay(1);
			_test_notebook_delete();
			lisa_thread_delay(1);
			_test_notebook_getlist(1, "word");
			_test_notebook_getlist(1, "text");
			break;
		defualt:
			LISA_LOGD(TAG, "err subcmd %d\n", type);
	};
}

static int _test_tts_speed(const struct shell *shell, size_t argc, char **argv)
{
	// (english,mandarin)
	// (slow,middle,fast)
	run_ol_tts_speed("english", "slow");
	run_ol_tts_speed("english", "middle");
	run_ol_tts_speed("english", "fast");
	run_ol_tts_speed("mandarin", "slow");
	run_ol_tts_speed("mandarin", "middle");
	run_ol_tts_speed("mandarin", "fast");
}

#define FEEDBACK_HELP ("0 online 1 offline 2 sync\n")

SHELL_STATIC_SUBCMD_SET_CREATE(scanpen_app_test,
		SHELL_CMD_ARG(test_ocr_single_line, NULL, "单行扫描", _test_ocr_single_line, 0, 0),
		SHELL_CMD_ARG(test_ocr_single_line_online, NULL, "在线单行扫描",
				_test_ocr_single_line_online, 0, 0),
		SHELL_CMD_ARG(test_ocr_multi_line, NULL, "多行扫描", _test_ocr_multi_line, 0, 0),
		SHELL_CMD_ARG(test_audio_trans, NULL, "语音翻译", _test_audio_trans, 0, 0),
		SHELL_CMD_ARG(test_online_dict, NULL, "在线词典", _test_online_dict, 0, 0),
		SHELL_CMD_ARG(test_audio_evaluate, NULL, "口语评测", _test_audio_evaluate, 0, 0),
		SHELL_CMD_ARG(test_recognition, NULL, "语音识别", _test_recognition, 0, 0),
		SHELL_CMD_ARG(test_oral_practice, NULL, "口语练习", _test_oral_practice, 0, 0),
		SHELL_CMD_ARG(test_tts_speed, NULL, "tts语速", _test_tts_speed, 0, 0),
		SHELL_CMD_ARG(test_feedback, NULL, FEEDBACK_HELP, _test_feedback, 0, 0),
		SHELL_CMD_ARG(test_notebook, NULL, FEEDBACK_HELP, _test_notebook, 0, 0),

		SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(scanpen_app, &scanpen_app_test, "scanpen_app related commands.", NULL);
