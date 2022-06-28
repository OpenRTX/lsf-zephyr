#include "lisa_evs_uuid.h"
#include <signal.h>
#include <mbedtls/sha1.h>
#include <mbedtls/platform.h>
#include <lisa_http.h>
#include <fs/fs.h>
#include <shell/shell.h>
#include <lisa_time.h>

#include <lisa_log.h>
#include <lisa_thread.h>
#include <lisa_queue.h>
#include <lisa_semaphore.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>

//#include "evs_auth_mgr.h"
#include "cjson/cJSON.h"
#include "utils/evs_utils.h"
#include "lisa_evs.h"
#include "lisa_evs_config.h"
#include "lisa_evs_rec.h"
#include "lisa_evs_ws.h"
#include "lisa_evs_system.h"
#include "utils/lisa_ring_buffer.h"
#include "lisa_evs_websocket.h"

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/video.h>


#include "lisa_buttons.h"
#include "lisa_vcapture.h"
#include "lisa_evs_cp_data_parser.h"

#define TAG "scanpen_online"


//client_id，iflyos平台分配
//https://device.iflyos.cn/product/device-info
//从设备控制台=>产品信息=>client_id 获取
#define PRODUCT_CLIENT_ID ("ad9f3c5d-38fd-4383-8d8d-8d460590de22")

//正式集成的时候需要从设备中获取，不同的设备deviceid不一样，不能重复！！！
//https://device.iflyos.cn/product/device-id
//从上述链接导入设备白名单，并且确保每台设备device_id不一样，否则产生设备竞争，导致设备和账号异常。
#define PRODUCT_DEVICE_ID ("test0008")

//ota_secret_id，iflyos平台分配
//https://device.iflyos.cn/product/auto-update
//从设备控制台=>自动更新=>加密密钥 获取
#define PRODUCT_OTA_SECRET_ID ("7c7e25414ec0389d6a3d68645d93fea804aa972b")

#define L1FS_FILE_PATH(file_name) ("/lfs1/" #file_name)
#define URL_GET_OTA_INFO ("http://ota.iflyos.cn/ota/client/check/packages")

#define IMAGE_MSG_QUE_CNT (256)  //需要是2的倍数

extern lisa_image_transfer_status_e g_img_status;
static char s_request_access_token[80] = {0};
static char s_request_id[40] = {0};

static lisa_evs_t *s_handle = NULL;

static lisa_thread_t *s_ocr_recognize_thread = NULL;
static int s_ocr_recognize_run = 0;
static lisa_thread_t *s_offline_recognize_thread = NULL;
static lisa_thread_t *s_send_ocr_simulation_image_thread = NULL;
static lisa_thread_t *s_cp_task_protocol_thread = NULL;
static int s_cp_task_run = 0;
static lisa_thread_t *s_test_send_audio_thread = NULL;

static lisa_queue_t *s_image_queue = NULL;
static bool s_ws_connected = false;

static struct lisa_ring_buffer *s_send_ocr_simulation_image_buffer = NULL;
static lisa_semaphore_t *s_ocr_image_sema = NULL;
static lisa_semaphore_t *s_ocr_res_sema = NULL;
static bool s_debug_mode = false;

typedef struct {
	volatile char *addr;
	volatile uint32_t frame_size;
	volatile uint32_t frame_cnt;
	volatile uint32_t w_pos;
	volatile uint32_t tag;
} __attribute__((aligned(32))) swap_block_ap_t;

typedef struct {
	volatile uint32_t r_pos;
} __attribute__((aligned(32))) swap_block_cp_t;

typedef struct {
	swap_block_ap_t ap_share;
	swap_block_cp_t cp_share;
} __packed frames_info_t;

typedef struct frame_info {
	uint32_t id;
	uint32_t sum;
} __attribute__((aligned(32))) frame_info_t;

Z_GENERIC_SECTION(.psram_section)
__attribute__((aligned(32),
	       used)) static char swp_frames[5][sizeof(frame_info_t) + 180 * 128] = { 0 };

#define BUILD_CHECK(x) do {char var[!!(x) - 1] __attribute__((unused));} while(0)


// 单行扫描
static bool s_singleline = false;
static int button_status = 0;

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

static lisa_vcapture_handler_t *vcap_handler = NULL;
static char ocr_result[1024] = {0};
void _image_data_cb(lisa_vcapture_cbmsg_e msg, void *data, uint32_t len)
{
	switch (msg) {
		case VCAPTURE_START:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_START \n");
			break;
		case VCAPTURE_STOP:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_STOP \n");
#if 0
			if (g_img_status == LISA_IMAGE_FRAME_CONTINUE) {
				g_img_status = LISA_IMAGE_FRAME_END;
				if (s_ws_connected)
					lisa_evs_rec_image_end(s_handle->rec, s_request_id);
				g_img_status = LISA_IMAGE_FRAME_NONE;
			}
#else
			ocr_test_online_stop();
#endif
			break;
		case VCAPTURE_DATA: {
			lisa_image_msg_t recv_image;
			recv_image.status = LISA_IMAGE_FRAME_NONE;
			recv_image.data = (char *)data;
			recv_image.size = len;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
		} break;
		case VCAPTURE_OCR:
			// LISA_LOGD(TAG, "OCR result [len: %d] %s\n", len, data);
#if(CONFIG_AVF_MODULES_ALG_OCR) 
			snprintf(ocr_result,sizeof(ocr_result),"%s",data);
			lisa_semaphore_give(s_ocr_res_sema);
#endif	
			
			break;
		default:
			LISA_LOGE(TAG, "unsupport vcapture msg\n");
			break;
	}
}

static int _video_init()
{
	lisa_vcapture_params_t params;
	memset(&params, 0, sizeof(lisa_vcapture_params_t));

#if (CONFIG_AVF_MODULES_ALG_OCR)
	params.use_ocr = 1;
#endif
	params.vcapturecb = _image_data_cb;

	vcap_handler = lisa_vcapture_create(&params);
	if (vcap_handler == NULL) {
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

	// lisa_thread_mdelay(15);
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
	const long cp_mock_data_len = fs_tell(&fp);
	LISA_LOGD(TAG, "audio file size %ld\n", cp_mock_data_len);
	fs_seek(&fp, 0, FS_SEEK_SET);
	const int cp_frame_size = 2048;

	lisa_image_msg_t recv_image;
	recv_image.status = LISA_IMAGE_FRAME_NONE;
	recv_image.data = (uint8_t *)lisa_mem_alloc(cp_frame_size);
	if (!recv_image.data) {
		LISA_LOGE(TAG, "malloc fail");
		goto ERR_MALLOC;
	}

	int send_total = 0;

	// 扫描开始
	g_img_status = LISA_IMAGE_FRAME_START;

	LISA_LOGD(TAG, "send_uart_scan_start\n");
	lisa_thread_delay(2);

	// 发送ocr模拟图片
	while (cp_mock_data_len > send_total) {
		LISA_LOGD(TAG, "simulation while: send_total: %d\n", send_total);
		if (cp_mock_data_len > (send_total + cp_frame_size)) {
			recv_image.size = cp_frame_size;
			fs_read(&fp, recv_image.data, recv_image.size);
			send_total += recv_image.size;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
		} else {
			LISA_LOGD(TAG, "send_uart_scan_end\n");
			lisa_thread_mdelay(100);
			g_img_status = LISA_IMAGE_FRAME_END;

			// 扫描结束
			recv_image.size = cp_mock_data_len - send_total;
			fs_read(&fp, recv_image.data, recv_image.size);
			send_total += recv_image.size;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);

			break;
		}
	}

	lisa_thread_delay(2);
	if (!s_singleline) {
		fs_seek(&fp, 0, FS_SEEK_SET);
		g_img_status = LISA_IMAGE_FRAME_START;
		lisa_thread_mdelay(500);
		send_total = 0;
		while (cp_mock_data_len > send_total) {
			LISA_LOGD(TAG, "simulation while: send_total: %d\n", send_total);
			if (cp_mock_data_len > (send_total + cp_frame_size)) {
				recv_image.size = cp_frame_size;
				fs_read(&fp, recv_image.data, recv_image.size);
				send_total += recv_image.size;
				send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
			} else {
				LISA_LOGD(TAG, "send_uart_scan_end\n");
				lisa_thread_mdelay(100);
				g_img_status = LISA_IMAGE_FRAME_END;

				// 扫描结束
				recv_image.size = cp_mock_data_len - send_total;
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
 * @brief 从cp buffer中读取数据
 *
 * @param data
 * @param len
 * @param wait_ms
 * @return uint32_t
 */
uint32_t _read_cp_buffer(uint8_t *data, unsigned int len, int wait_ms)
{
	#if 0
	return recv_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, data, len, wait_ms);
	#else
	int iret;
	if(lisa_vcapture_is_runing(vcap_handler) == 0){
		k_msleep(10);
		return 0;
	}
	iret = lisa_vcapture_readi(vcap_handler,data,len);
	if(iret >= 0){
		return iret;
	}
	LISA_LOGE(TAG,"Read vcapture datai failed iret %d\n",iret);
	k_msleep(10);
	return 0;
	#endif
}

/**
 * @brief cp task protocol线程，用于图片数据接受和解析
 *
 * @param argv
 */
void cp_task_protocol_thread(void *argv)
{
	lisa_evs_parser_cp_init(_read_cp_buffer);
	while (s_cp_task_run) {
		if (lisa_evs_parser_cp_data()) {
		} else {
		}
	}

	if (s_cp_task_protocol_thread) {
		lisa_thread_t *temp = s_cp_task_protocol_thread;
		s_cp_task_protocol_thread = NULL;
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

	// cp图片消息队列
	s_image_queue = lisa_queue_create(IMAGE_MSG_QUE_CNT, "img_msg", sizeof(lisa_image_msg_t));
	if (s_image_queue == NULL) {
		LISA_LOGE(TAG, "create img_msg queue failed!");
		return;
	}

	printk("[Debug-%s-%s-%d]------\n",__FILE__,__func__,__LINE__);
	lisa_image_msg_t send_image;
	memset(&send_image, 0, sizeof(lisa_image_msg_t));
	static uint32_t image_index = 0;
	LISA_LOGI(TAG, "%s Enter While", __FUNCTION__);
	while (s_ocr_recognize_run) {
		if (lisa_queue_receive(s_image_queue, &send_image, sizeof(lisa_image_msg_t),
					10) == LISA_OK) {
			if (!s_ws_connected) {
				LISA_LOGE(TAG, "websocket is disconnected, can't scan image");
				if (send_image.data) lisa_mem_free(send_image.data);
				continue;
			}
			switch (send_image.status) {
				case LISA_IMAGE_FRAME_START:
					if (image_index == -1) {
						printk("[Debug-%s-%s-%d]------\n",__FILE__,__func__,__LINE__);
						lisa_evs_uuid_generate_string(s_request_id);
						lisa_evs_rec_image_param_t image_param = EVS_REC_IMG_PARAM_INIT(
										// 模拟中文分词
										// .tokenization = true,
										// .dictionary = true,
										// .resources = true,
										.rec_mode = EVS_REC_MODE_OCR_TRANS_TTS);
						image_param.debug = s_debug_mode;
						image_param.tokenization = true;
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
					if (send_image.data != NULL) {
						lisa_evs_rec_send_image(s_handle->rec, send_image.data, send_image.size);
					}

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
		printk("[Debug-%s-%s-%d]------\n",__FILE__,__func__,__LINE__);
		lisa_thread_t *temp_thread = s_ocr_recognize_thread;
		s_ocr_recognize_thread = NULL;
		lisa_thread_delete(temp_thread);
		image_index = 0;
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
 * @brief Create a cp task protocol thread object
 *
 */
void create_cp_task_protocol_thread()
{
	/*
	 *if (s_cp_task_protocol_thread) {
	 *    return;
	 *}
	 */
	// 1. cp_task_protocol_thread
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "cp_task_protocol_thread";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = OS_PRIORITY_HIGH;  // OS_PRIORITY_NORMAL

	s_cp_task_run = 1;
	//解析图片
	s_cp_task_protocol_thread = lisa_thread_create(&thread_attr, cp_task_protocol_thread, NULL);
	if (s_cp_task_protocol_thread == NULL) {
		LISA_LOGE(TAG, "thread_create: cp_task_protocol_thread failed");
		return;
	}
}

/**
 * @brief Create a ocr recognize thread object
 *
 */
void create_ocr_recognize_thread()
{
	/*
	 *if (s_ocr_recognize_thread) {
	 *    return;
	 *}
	 */

    printk("[Debug-%s-%s-%d]------\n",__FILE__,__func__,__LINE__);
	// 2. ocr_thread
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "ocr_recognize_thread";
	thread_attr.stack_size = 2 * 1024;
	// thread_attr.stack_size = K_THREAD_STACK_SIZEOF(ocr_recognize_stack_area);
	// thread_attr.stack = ocr_recognize_stack_area;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	s_ocr_recognize_run = 1;
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
	if (!s_send_ocr_simulation_image_buffer) {
		unsigned char *buffer = lisa_mem_alloc(256 * 1024);

		s_send_ocr_simulation_image_buffer = lisa_ring_buffer_init(buffer, 256* 1024);

		s_ocr_image_sema = lisa_semaphore_create(2);
		create_cp_task_protocol_thread();
		create_ocr_recognize_thread();
	}

	create_send_ocr_simulation_image_thread();
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
	LISA_LOGD(TAG, "message, action: %s, len: %d, payload: %s, request id: %s", action, len, msg,
			req_id);
	if (strcmp(action, "audio_player.audio_out") == 0) {
		play_ws_msg(msg, len);
	} else if (strcmp(action, "recognizer.evaluate_result") == 0) {
		LISA_LOGD(TAG, "evaluate result = %s\n", msg);
	}
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

static inline char *bf30a2_frames_info_get_free(frames_info_t *info)
{
	if (info == NULL || info->ap_share.addr == NULL || info->ap_share.frame_cnt <= 0) {
		return NULL;
	}

	// SCB_InvalidateDCache_by_Addr(info, sizeof(frames_info_t));
	int r = info->cp_share.r_pos;
	int w = info->ap_share.w_pos;
	int cnt = info->ap_share.frame_cnt;
	char *addr = info->ap_share.addr;

	w %= info->ap_share.frame_cnt;

	if (w > r) {
		if ((r + cnt - w) <= 1) {
			LISA_LOGE(TAG, "No free space to copy datas, w:%d, r=%d.", w, r);
			return NULL;
		}
	} else if (w < r) {
		if (r - w <= 1) {
			LISA_LOGE(TAG, "No free space to copy datas, w:%d, r=%d.", w, r);
			return NULL;
		}
	}

	return info->ap_share.addr + w * info->ap_share.frame_size;
}

static inline int32_t bf30a2_frames_info_update_w_pos(frames_info_t *info)
{
	if (info == NULL || info->ap_share.addr == NULL || info->ap_share.frame_cnt <= 0) {
		return -1;
	}

	info->ap_share.w_pos++;
	info->ap_share.w_pos %= info->ap_share.frame_cnt;
	// SCB_CleanDCache_by_Addr(info, sizeof(frames_info_t));

	return info->ap_share.w_pos;
}

#define VIDEO_DEV DT_LABEL(DT_INST(0, byd_bf30a2))

uint32_t calc_sum(uint8_t *src, uint32_t size)
{
	uint32_t sum = 0;

	while (size--) {
		sum += (uint8_t)*src;
		src++;
	}

	return sum;
}

static lisa_thread_t *s_cam2cp_thread = NULL;
void video_ap2cp_thread(void *p1)
{
	struct video_buffer *vbuf = NULL;

	/*Don't change this next line,it is reserved for video share memory size 256 Bytes!!!!!!!!!!!!!!*/
	frames_info_t *info = (frames_info_t *)(CONFIG_SOF_IPC_SHM_BASE_ADDRESS + CONFIG_SOF_IPC_SHM_SIZE - 0x100);

	const struct device *video = device_get_binding(VIDEO_DEV);

	if (!device_is_ready(video)) {
		printk("ipm device is null.\n");
		return;
	}
	memset(info, 0, sizeof(frames_info_t));
	info->ap_share.addr = &swp_frames[0][0];
	info->ap_share.frame_size = 180 * 128 + sizeof(frame_info_t);
	info->ap_share.frame_cnt = 5;
	info->ap_share.w_pos = 0;
	info->cp_share.r_pos = 0;
	info->ap_share.tag = 0x12345678;

	LISA_LOGI(TAG, "##########Frames info##########");
	LISA_LOGI(TAG, "info       :%p.", info);
	LISA_LOGI(TAG, "buffer addr:%p.", info->ap_share.addr);
	LISA_LOGI(TAG, "frame  size:%u.", info->ap_share.frame_size);
	LISA_LOGI(TAG, "frame count:%d.", info->ap_share.frame_cnt);
	LISA_LOGI(TAG, "write   pos:%d.", info->ap_share.w_pos);
	LISA_LOGI(TAG, "read    pos:%d.", info->cp_share.r_pos);
	LISA_LOGI(TAG, "sizeof(frame_info_t):%d, sizeof(swap_block_ap_t):%d.", sizeof(frame_info_t), sizeof(swap_block_ap_t));

	uint32_t last_tick = 0;
	int id = 0;
	int fps = 0;
	int frame_cnt = 0;
	int discard_cnt = 0;

	BUILD_CHECK(sizeof(frames_info_t) % 32 == 0);

	while (1) {
		vbuf = NULL;
		video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		frame_cnt++;
		/* send datas to cp core */
		char *buffer = bf30a2_frames_info_get_free(info);
		if (buffer != NULL) {
			if ((buffer < info->ap_share.addr) || (buffer >= (info->ap_share.addr + info->ap_share.frame_size * info->ap_share.frame_cnt))) {
				LISA_LOGI(TAG, "##########Frames info##########");
				LISA_LOGI(TAG, "info       :%p.", info);
				LISA_LOGI(TAG, "buffer addr:%p.", info->ap_share.addr);
				LISA_LOGI(TAG, "frame  size:%u.", info->ap_share.frame_size);
				LISA_LOGI(TAG, "frame count:%d.", info->ap_share.frame_cnt);
				LISA_LOGI(TAG, "write   pos:%d.", info->ap_share.w_pos);
				LISA_LOGI(TAG, "read    pos:%d.", info->cp_share.r_pos);

				LISA_LOGI(TAG, "buffer:%p.", buffer);
				k_panic();
			}
			frame_info_t *frame_info = (frame_info_t *)buffer;
			frame_info->id = id++;
			memcpy(buffer + sizeof(frame_info_t), vbuf->buffer, 180 * 128);
			frame_info->sum = calc_sum(buffer + sizeof(frame_info_t), 180 * 128);
			SCB_CleanDCache_by_Addr(buffer, info->ap_share.frame_size);
			bf30a2_frames_info_update_w_pos(info);
			fps = vbuf->timestamp - last_tick > 0 ?
				      1000 / (vbuf->timestamp - last_tick) :
				      0;
			last_tick = vbuf->timestamp;
		} else {
			discard_cnt++;
		}

		if (frame_cnt % 10 == 0) {
			LISA_LOGI(TAG, "fps:%d, T:%d, D%d.", fps, frame_cnt, discard_cnt);
		}
		video_enqueue(video, VIDEO_EP_OUT, vbuf);
	}
}

void create_cam2cp_thread()
{

	if (s_cam2cp_thread) {
		LISA_LOGE(TAG, "s_cam2cp_thread exist ");
		return;
	}
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "cam2cp_thread";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = OS_PRIORITY_HIGHER;  // OS_PRIORITY_NORMAL

	s_cam2cp_thread = lisa_thread_create(&thread_attr, video_ap2cp_thread, NULL);
	if (s_cam2cp_thread == NULL) {
		LISA_LOGE(TAG, "thread_create:  s_cam2cp_thread failed");
		return;
	}
}







#define PEN_HEAD_BUTTON KEY1
static button_handle *pen_head_button;
static void pen_head_button_cb(BUTTON_STATE sta, void *arg)
{
	/* long button has the PRESS/RELEASE state */
	if (sta == PRESS) {
		ocr_test_online_start();
		g_img_status = LISA_IMAGE_FRAME_START;
		LISA_LOGD(TAG, "send_uart_scan_start\n");
		lisa_vcapture_start(vcap_handler);
		lisa_evs_uuid_generate_string(s_request_id);
		lisa_evs_rec_image_param_t image_param = EVS_REC_IMG_PARAM_INIT(
				// 模拟中文分词
				// .tokenization = true,
				// .dictionary = true,
				// .resources = true,
				.rec_mode = EVS_REC_MODE_OCR_TRANS_TTS);
		image_param.debug = s_debug_mode;
		image_param.tokenization = true;
		image_param.dictionary = true;
		image_param.single_line = s_singleline;
		LISA_LOGD(TAG, "lisa evs rec image start");
		lisa_evs_rec_image_start(s_handle->rec, s_request_id, &image_param);

	} else if (sta == RELEASE) {
		LISA_LOGD(TAG, "send_uart_scan_stop\n");
		lisa_vcapture_stop(vcap_handler);
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
	pen_head_button = create_long_button(PEN_HEAD_BUTTON, 100);
	pen_head_button->cb = pen_head_button_cb;

	pen_head_button->start(pen_head_button);
}
#if (CONFIG_AVF_MODULES_ALG_OCR)
void  offline_recognize_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);
	char trans_data[1024];
	while (true) {
		lisa_semaphore_take(s_ocr_res_sema, LISA_WAIT_FOREVER);
		LISA_LOGI(TAG,"########  offline result ########\n");

		LISA_LOGI(TAG, "ocr result [len: %d] %s\n", strlen(ocr_result), ocr_result);
		// trans_handler = lisa_translate_create();
		// lisa_translate_exec(trans_handler, ocr_result, trans_data, 1024, 40000);
		// lisa_translate_destroy(trans_handler);

		// lisa_tts_params_t params;
		// memset(&params, 0, sizeof(lisa_tts_params_t));
		// params.ttscb = _tts_cb;
		// tts_handler = lisa_tts_create(&params);

		// lisa_tts_play(tts_handler, trans_data);
		LISA_LOGI(TAG,"########  offline flush end ########\n");
	}

}
/**
 * @brief Create a ocr recognize thread object
 *
 */
void create_offline_recognize_thread()
{
	s_ocr_res_sema = lisa_semaphore_create(1);
	if (s_offline_recognize_thread) {
		return;
	}

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "offline_recognize_thread";
	thread_attr.stack_size = 4 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	s_offline_recognize_thread = lisa_thread_create(&thread_attr, offline_recognize_thread, NULL);
	if (s_offline_recognize_thread == NULL) {
		LISA_LOGE(TAG, "thread_create:  offline_recognize_thread failed");
		return;
	}
}
#endif
/**
 * @brief 启动在线ocr识别
 *
 */
void ocr_test_online_start()
{
	s_singleline = true;

	g_img_status = LISA_IMAGE_FRAME_NONE;
	if (!s_send_ocr_simulation_image_buffer) {
	printk("[Debug-%s-%s-%d]------\n",__FILE__,__func__,__LINE__);
		unsigned char *buffer = lisa_mem_alloc(256 * 1024);
		if (!buffer) {
			LISA_LOGE(TAG, "buffer malloc failed \n");
		}

		s_send_ocr_simulation_image_buffer = lisa_ring_buffer_init(buffer, 256 * 1024);

		s_ocr_image_sema = lisa_semaphore_create(2);
		create_cp_task_protocol_thread();
		create_ocr_recognize_thread();
#if (CONFIG_AVF_MODULES_ALG_OCR)
		create_offline_recognize_thread();
#endif
	}

	//_video_init();
	//pen_head_button_init();
}

void ocr_test_online_stop()
{

	//lisa_vcapture_stop(vcap_handler);
	//lisa_vcapture_destroy(vcap_handler);

	if (g_img_status == LISA_IMAGE_FRAME_CONTINUE) {
		g_img_status = LISA_IMAGE_FRAME_END;
		if (s_ws_connected)
			lisa_evs_rec_image_end(s_handle->rec, s_request_id);
	}
	
	s_cp_task_run = 0;
	s_ocr_recognize_run = 0;
//	lisa_thread_delete(s_ocr_recognize_thread);
//	lisa_thread_delete(s_cp_task_protocol_thread);
	lisa_semaphore_delete(s_ocr_image_sema);
	
	lisa_ring_buffer_free(s_send_ocr_simulation_image_buffer);
	s_send_ocr_simulation_image_buffer = NULL;
	/*s_ocr_recognize_thread = NULL;*/
	/*s_cp_task_protocol_thread = NULL;*/
	printk("[Debug-%s-%s-%d]------\n",__FILE__,__func__,__LINE__);


//	pen_head_button_exit();
}

/**
 * @brief 启动扫描笔app
 *
 */
void start_scanpen_online()
{
	/*设备端初始化*/
	run_device_init();

	connect_iflyos();

	k_msleep(1000);

	//ocr_test_online_start();
	create_cam2cp_thread();
	_video_init();
	pen_head_button_init();
}

