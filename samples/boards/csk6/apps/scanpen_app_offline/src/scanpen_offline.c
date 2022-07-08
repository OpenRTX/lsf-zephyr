#include "kernel.h"
#include "sys_clock.h"
#include <lisa_log.h>
#include <lisa_thread.h>
#include <lisa_queue.h>
#include <lisa_semaphore.h>
#include <lisa_mem.h>
#include <lisa_timer.h>
#include <shell/shell.h>

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/video.h>

#include "tts_playback.h"

#include "lisa_buttons.h"
#include "lisa_vcapture.h"
#include "lisa_translate.h"
#include "lisa_tts.h"
#include "lisa_uart_port.h"
#include "lisa_uart_recv.h"
#include "lisa_evs_cp_data_parser.h"

//#include "lisa_evs_uuid.h"
#include <fs/fs.h>
#include <lisa_time.h>
#define TAG "scanpen_offline"

#include "dict.h"
#include "dict_demo.h"

#define VIDEO_DEV DT_LABEL(DT_INST(0, byd_bf30a2))

typedef enum {
	e_work_status_idle = 0,
	e_work_status_scanning,
	e_work_status_waiting,
	e_work_status_deny,
	e_work_status_adjust,
	e_work_status_ready,
	e_work_status_camera_error,
	e_work_status_camera_decode_error,
	e_work_status_stitch_error,
	e_work_status_text_output,
	e_work_status_text_finish,
	e_work_status_tts_output,
	e_work_status_tts_finish,
	e_work_status_trans_start,
	e_work_status_trans_finish,
	e_work_status_dict_start,
	e_work_status_dict_finish,
} work_status_e;

typedef enum {
	e_scan_mode_singleline = 0,
	e_scan_mode_multiline,
	e_scan_mode_invalid,
	e_scan_mode_factory,
	e_scan_mode_debug = 4,
	e_scan_mode_age,
	e_scan_mode_adjust,
	e_scan_mode_offline_ocr,
	e_scan_mode_adjust_serial,
	e_scan_mode_maximum_picture,
	e_scan_mode_reboot = 10,
	e_scan_mode_offline_image_uart,
	e_scan_mode_online_image_uart,
	e_scan_mode_stitch_image_uart,
	e_scan_mode_max,
} scan_mode_e;

/* ocr */
static struct lisa_ring_buffer *s_send_ocr_simulation_image_buffer = NULL;
static lisa_semaphore_t *s_ocr_image_sema = NULL;
static lisa_thread_t *s_offline_recognize_thread = NULL;
static volatile lisa_thread_t *s_cam2cp_thread = NULL;
static lisa_vcapture_handler_t *vcap_handler = NULL;
static lisa_timer_t *multiline_scan_timer;
static char ocr_result[1024] = {0};
/* tts */
static struct lisa_ring_buffer *s_tts_audio_ring_buffer = NULL;
static lisa_thread_t *s_tts_thread = NULL;
static lisa_thread_t *s_tts_send_thread = NULL;
static lisa_semaphore_t *s_tts_sema = NULL;
static lisa_semaphore_t *s_tts_ring_buffer_sema = NULL;
static lisa_tts_handler_t *tts_handler = NULL;
/* translate */
static lisa_translate_handler_t *trans_handler = NULL;
/* protocol */
static lisa_thread_t *s_uart_proto_thread = NULL;

static scan_mode_e s_scan_mode = e_scan_mode_singleline;

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

typedef struct {
	lisa_image_transfer_status_e status;
	char *data;
	int size;
} lisa_image_msg_t;

 static void work_status_set(work_status_e sta)
 {
	send_proc_scan_work_status(sta);
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
static int recv_ocr_simulation_image_once(struct lisa_ring_buffer *ring_buffer,
				uint8_t *data, unsigned int len, lisa_semaphore_t *sema, int wait_ms)
{
	int ret = lisa_ring_buffer_get(ring_buffer, data, len);
	while (ret <= 0) {
		lisa_semaphore_take(sema, LISA_WAIT_FOREVER);
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
static void send_ocr_simulation_image_once(struct lisa_ring_buffer *ring_buffer,
				lisa_image_msg_t *data, lisa_semaphore_t *sema)
{
	bool need_wake = false;
	if (lisa_ring_buffer_len(ring_buffer) <= 0) {
		need_wake = true;
	}

	int ret = lisa_ring_buffer_put(ring_buffer, data->data, data->size);
	if (need_wake) {
		lisa_semaphore_give(sema);
	}
	if (ret != data->size) {
		LISA_LOGE(TAG, "write stream full \n");
	}

	lisa_thread_mdelay(15);
}

static void ocr_text_send(const char *data, uint32_t len)
{
	work_status_set(e_work_status_text_output);
	memset(ocr_result, 0, 1024);
	memcpy(ocr_result, data, len);
	// spi pack
	uint8_t *text_pack = (uint8_t *)lisa_mem_alloc(len+16);
	if(text_pack) {
		uint32_t pack_len = lisa_evs_pack_spi_data(text_pack, (uint8_t *)ocr_result, len, LISA_UP_TEXT_OCR);
		printk("pack len:%d\n", pack_len);
		for(int i=0;i < pack_len; i++) {
			printk("%02x ", text_pack[i]);
		}
		printk("\n");
		// spi send
		lisa_spi_write(text_pack, pack_len);
		lisa_thread_mdelay(50);
		// pack free
		lisa_mem_free(text_pack);
	}
	work_status_set(e_work_status_text_finish);
}


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
		//	LISA_LOGD(TAG, "vcapture msg VCAPTURE_DATA \n");
			lisa_image_msg_t recv_image;
			recv_image.status = LISA_IMAGE_FRAME_NONE;
			recv_image.data = (char *)data;
			recv_image.size = len;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image, s_ocr_image_sema);
		} break;
		case VCAPTURE_OCR:
			LISA_LOGD(TAG, "OCR result [len: %d] %s\n", len, data);
			ocr_text_send(data, len);
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

	params.use_ocr = 1;
	params.vcapturecb = _image_data_cb;
	params.log_to_file = 0;

	vcap_handler = lisa_vcapture_create(&params);
	if (vcap_handler == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_vcapture_create FAILED!\n", __FUNCTION__, __LINE__);
		return -1;
	}

	return 0;
}

static void dirc_test(const char *data, uint32_t len)
{
	work_status_set(e_work_status_dict_start);
	memset(ocr_result, 0, 1024);
	memcpy(ocr_result, data, len);
	dict_init(NULL_DICT);
	#define DICT_TEST_SIZE (16*1024)
	char *algo_out = lisa_mem_alloc(DICT_TEST_SIZE);
	if(algo_out) {
		int gif_len = 0;
		dict_test(ocr_result, algo_out, NULL, &gif_len);
		uint32_t algo_out_len = strlen(algo_out);
		// spi pack
		uint8_t *text_pack = (uint8_t *)lisa_mem_alloc(algo_out_len+16);
		if(text_pack) {
			uint32_t pack_len = lisa_evs_pack_spi_data(text_pack, (uint8_t *)algo_out, algo_out_len, LISA_UP_TEXT_WORD_SEARCH);
			printk("pack len:%d\n", pack_len);
			for(int i=0;i < pack_len; i++) {
				printk("%02x ", text_pack[i]);
			}
			printk("\n");
			// spi send
			lisa_spi_write(text_pack, pack_len);
			lisa_thread_mdelay(50);
			// pack free
			lisa_mem_free(text_pack);
		}
		lisa_mem_free(algo_out);
	} else {
		printk("Address not 4 byte aligned or NULL error \n");
	}
	work_status_set(e_work_status_dict_finish);
}

static void trans_test(const char *data, uint32_t len)
{
	work_status_set(e_work_status_trans_start);
	memset(ocr_result, 0, 1024);
	memcpy(ocr_result, data, len);
	LISA_LOGD(TAG, "translate text :%s, len:%d\n", data, len);
	#define TRANS_TEST_SIZE (1*1024)
	char *algo_out = lisa_mem_alloc(TRANS_TEST_SIZE);
	trans_handler = lisa_translate_create();
	lisa_translate_exec(trans_handler, ocr_result, algo_out, 1024, 40000);
	lisa_translate_destroy(trans_handler);
	uint32_t algo_out_len = strlen(algo_out);
	// spi pack
	uint8_t *text_pack = (uint8_t *)lisa_mem_alloc(algo_out_len+16);
	if(text_pack) {
		uint32_t pack_len = lisa_evs_pack_spi_data(text_pack, (uint8_t *)algo_out, algo_out_len, LISA_UP_TEXT_TRANSLATE);
		printk("pack len:%d\n", pack_len);
		for(int i=0;i < pack_len; i++) {
			printk("%02x ", text_pack[i]);
		}
		printk("\n");
		// spi send
		lisa_spi_write(text_pack, pack_len);
		lisa_thread_mdelay(50);
		// pack free
		if(text_pack) lisa_mem_free(text_pack);
	}
	work_status_set(e_work_status_trans_finish);
}

static void multiline_scan_timer_cb(void *arg)
{
	/* set idle */
	LISA_LOGD(TAG, "###timer callback==");
	work_status_set(e_work_status_idle);
	lisa_timer_stop(multiline_scan_timer);
}

void  offline_recognize_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);

	if (!s_send_ocr_simulation_image_buffer) {
		unsigned char *buffer = lisa_mem_alloc(64 * 1024);
		s_send_ocr_simulation_image_buffer = lisa_ring_buffer_init(buffer, 64 * 1024);
		s_ocr_image_sema = lisa_semaphore_create(2);
	}

	// timer
	multiline_scan_timer = lisa_timer_create(OS_TIMER_PERIODIC, multiline_scan_timer_cb, NULL, LISA_WAIT_FOREVER);
	if (multiline_scan_timer == NULL) {
		LISA_LOGE(TAG, "buttons timer create error");
		return -1;
	}

	#define CROP_BUF_LEN (4*1024)
	unsigned char *crop_image_buffer = lisa_mem_alloc(CROP_BUF_LEN);
	int ret_len = 0;

	while (true) {
		ret_len = recv_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer,
						crop_image_buffer, CROP_BUF_LEN, s_ocr_image_sema, LISA_WAIT_FOREVER);
		// spi send, unnecessary packed
		printk("ret_len:%d\n", ret_len);
		lisa_spi_write(crop_image_buffer, ret_len);
	}
	lisa_timer_delete(multiline_scan_timer);
}

/**
 * @brief Create a ocr recognize thread object
 *
 */
void create_offline_recognize_thread()
{
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


/*******************************************************/
/****************** tts test   ***********************/
/*******************************************************/
#define TTS_AUDIO_SAVE_FLASH 0

#if TTS_AUDIO_SAVE_FLASH
#include <drivers/flash.h>
#include <storage/flash_map.h>
#define UPLOAD_FLASH_AREA_ID FLASH_AREA_ID(image_1)
#define BLOCK_SIZE (4096)
static const struct flash_area *fa;
static int tts_audio_offset = 0;
static char *tts_audio_buffer = NULL;
#endif

static int tts_test(uint8_t *text, uint32_t len)
{
	if(NULL == tts_handler) {
		printk("tts_handler null\n");
		return -1;
	}

	atts_config_t tts_config = {
		.role 		= ivTTS_ROLE_XIAOYAN,
		.language	= ivTTS_LANGUAGE_AUTO,
		.speed		= ivTTS_SPEED_NORMAL,
		.volume		= ivTTS_VOLUME_NORMAL,
	};
	if(text[0] > 127){
		tts_config.role = ivTTS_ROLE_XIAOYAN;
	}else{
		tts_config.role = ivTTS_ROLE_CATHERINE;
	}
	lisa_tts_play(tts_handler, text, &tts_config);
	work_status_set(e_work_status_tts_output);
	return 0;
}

static void _tts_cb(lisa_tts_cbmsg_e msg, void *data, uint32_t len)
{
	switch (msg) {
		case TTS_START:
			LISA_LOGD(TAG, "TTS_START \n");
			break;
		case TTS_PLAY_OVER:
			LISA_LOGD(TAG, "TTS_PLAY_OVER \n");
			lisa_semaphore_give(s_tts_sema);
			break;
		case TTS_AUDIO:
			printk("len:%d\n", len);
			#if TTS_AUDIO_SAVE_FLASH
			if(tts_audio_buffer) {
				memcpy(tts_audio_buffer+tts_audio_offset, data, len);
				tts_audio_offset += len;
			}
			#endif
			lisa_image_msg_t recv_image;
			recv_image.status = LISA_IMAGE_FRAME_NONE;
			recv_image.data = (char *)data;
			recv_image.size = len;
			send_ocr_simulation_image_once(s_tts_audio_ring_buffer, &recv_image, s_tts_ring_buffer_sema);
			break;
		default:
			LISA_LOGE(TAG, "unsupport tts msg\n");
			break;
	}
}

static void tts_test_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);

#if TTS_AUDIO_SAVE_FLASH
	tts_audio_buffer = lisa_mem_alloc(100 * 4096);
#endif

	s_tts_sema = lisa_semaphore_create(1);

	/* create tts */
	lisa_tts_params_t params;
	memset(&params, 0, sizeof(lisa_tts_params_t));
	params.ttscb = _tts_cb;
	tts_handler = lisa_tts_create(&params);
	LISA_LOGD(TAG, "TTS_INIT_DONE \n");

	while (true) {
		lisa_semaphore_take(s_tts_sema, LISA_WAIT_FOREVER);
		lisa_tts_stop(tts_handler);
		work_status_set(e_work_status_tts_finish);

#if TTS_AUDIO_SAVE_FLASH
		k_msleep(2000);
		LISA_LOGI(TAG, "storage tts audio");
		if (flash_area_open(UPLOAD_FLASH_AREA_ID, &fa)) {
			LISA_LOGE(TAG, "open image_1 partiton failed!!");
		}
		LISA_LOGI(TAG, "fa_dev_name:%s", fa->fa_dev_name);
		int ret = flash_area_erase(fa, 0, BLOCK_SIZE*100);
		if (ret) {
			LISA_LOGE(TAG, "flash partiton erase failed!,ret[%d]", ret);
		}
		ret = flash_area_write(fa, 0, (uint8_t *)tts_audio_buffer, tts_audio_offset);
		if (ret) {
			LISA_LOGE(TAG, "flash partiton write failed!,ret[%d]", ret);
		}
		LISA_LOGI(TAG, "close flash area ,total:%d", tts_audio_offset);
		flash_area_close(fa);
		if(tts_audio_buffer) lisa_mem_free(tts_audio_buffer);
#endif
	}
	lisa_tts_destroy(tts_handler);
}

static void tts_send_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);
	s_tts_ring_buffer_sema = lisa_semaphore_create(2);
	if (!s_tts_audio_ring_buffer) {
		unsigned char *buffer = lisa_mem_alloc(64 * 1024);
		s_tts_audio_ring_buffer = lisa_ring_buffer_init(buffer, 64 * 1024);
	}
	int send_len = 0;
	#define TTS_AUDIO_BUF_LEN (4*1024)
	unsigned char *tts_buffer = lisa_mem_alloc(TTS_AUDIO_BUF_LEN);

	// packed
	uint8_t *audio_pack = lisa_mem_alloc(TTS_AUDIO_BUF_LEN+16);

	while (true) {
		send_len = recv_ocr_simulation_image_once(s_tts_audio_ring_buffer,
						tts_buffer, TTS_AUDIO_BUF_LEN, s_tts_ring_buffer_sema, LISA_WAIT_FOREVER);
		uint32_t pack_len = lisa_evs_pack_spi_data(audio_pack, (uint8_t *)tts_buffer, send_len, LISA_UP_AUDIO);
		// spi send
		printk("tts pack_len:%d\n", pack_len);
		lisa_spi_write(audio_pack, pack_len);
	}
}

/**
 * @brief Create a tts thread object
 *
 */
static void create_tts_thread()
{
	if (s_tts_thread) {
		return;
	}

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "tts_thread";
	thread_attr.stack_size = 4 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	s_tts_thread = lisa_thread_create(&thread_attr, tts_test_thread, NULL);
	if (s_tts_thread == NULL) {
		LISA_LOGE(TAG, "thread_create:  tts_test_thread failed");
		return;
	}

	lisa_thread_attr_t tts_thread_attr;
	tts_thread_attr.name = "tts_send_thrd";
	tts_thread_attr.stack_size = 4 * 1024;
	tts_thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	s_tts_send_thread = lisa_thread_create(&tts_thread_attr, tts_send_thread, NULL);
	if (s_tts_send_thread == NULL) {
		LISA_LOGE(TAG, "thread_create:  tts_send_thread failed");
		return;
	}
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

uint32_t calc_sum(uint8_t *src, uint32_t size)
{
	uint32_t sum = 0;

	while (size--) {
		sum += (uint8_t)*src;
		src++;
	}

	return sum;
}

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
		LISA_LOGD(TAG, "send_uart_scan_start\n");
		_video_init();
		lisa_vcapture_start(vcap_handler);
		work_status_set(e_work_status_scanning);
	} else if (sta == RELEASE) {
		lisa_vcapture_stop(vcap_handler);
		lisa_vcapture_destroy(vcap_handler);
		if(e_scan_mode_multiline == s_scan_mode) {
			/* set the time */
			lisa_err_t sta = lisa_timer_change_period(multiline_scan_timer, 2000);
			if (sta != LISA_OK) {
				LISA_LOGE(TAG, "timer change period error");
			}
			if (!lisa_timer_isactive(multiline_scan_timer)) {
				lisa_timer_start(multiline_scan_timer);
			}
			work_status_set(e_work_status_waiting);
		} else {
			work_status_set(e_work_status_idle);
		}
		LISA_LOGD(TAG, "send_uart_scan_stop\n");
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

static void _uart_recv_cb(recv_cmd_type_e type, void *data, uint32_t len)
{
	printk("@@recv_cmd_type_e:%d\n", type);

	switch (type) {
		case e_cmd_scan_tts_text:
			LISA_LOGD(TAG, "tts text :%s, len:%d\n", data, len);
			tts_test(data, len);
			break;
		case e_cmd_scan_translate_text:
			trans_test(data, len);
			break;
		case e_cmd_scan_word_search_text:
			dirc_test(data, len);
			break;
		case e_cmd_scan_work_mode: {
			uint8_t *mode = (uint8_t *)data;
			s_scan_mode = mode[0];
			LISA_LOGD(TAG, "receive scan mode:%d\n", s_scan_mode);
			break;
		}
		default:
			LISA_LOGE(TAG, "unsupport tts msg\n");
			break;
	}
}

static void create_uart_proto_thread(void)
{
	if (s_uart_proto_thread) {
		return;
	}
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "uart_proto";
	thread_attr.stack_size = 4 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

	s_uart_proto_thread = lisa_thread_create(&thread_attr, prot_recv_task, _uart_recv_cb);
	if (s_uart_proto_thread == NULL) {
		LISA_LOGE(TAG, "thread_create:  _uart_proto_thread failed");
		return;
	}
}


enum {
       HOLDPEN_LEFT,
       HOLDPEN_RIGHT
};
/**
 * @brief 设置扫描笔左右手持笔扫描模式
 *
 * @param hand
 */
static int set_holdpen_mode(uint8_t hand){
       const struct device *video = device_get_binding(VIDEO_DEV);
       int r = 0;
       uint8_t right_val = 0x42;
       uint8_t left_val = 0x4e;

       if(hand == HOLDPEN_LEFT){
               r = video_set_ctrl(video, 0x00, &left_val);             // left
               LISA_LOGD(TAG, "set_hand_scan_mode:left %d\n", r);
       }
       else if(hand == HOLDPEN_RIGHT){
               r = video_set_ctrl(video, 0x00, &right_val);            // right
               LISA_LOGD(TAG, "set_hand_scan_mode:right %d\n", r);
       }
       return r;
}
static int _set_holdpen_mode(const struct shell *shell, size_t argc, char **argv)
{
       int type = atoi(argv[1]);
       switch (type) {
               case 0:
                       set_holdpen_mode(HOLDPEN_LEFT);
                       break;
               case 1:
                       set_holdpen_mode(HOLDPEN_RIGHT);
                       break;
               default:
                       LISA_LOGD(TAG, "err subcmd %d\n", type);
                       break;
       }
}

SHELL_STATIC_SUBCMD_SET_CREATE(scanpen_app_test,
               SHELL_CMD_ARG(set_holdpen_mode, NULL, "0:left 1:right", _set_holdpen_mode, 0, 0),
               SHELL_SUBCMD_SET_END /* Array terminated. */
);
SHELL_CMD_REGISTER(scanpen_app, &scanpen_app_test, "scanpen_app related commands.", NULL);


static void test_app(void)
{
	// wait all thread ready
	k_msleep(1000);
#if 0 // algo test
	printk("######dirc_test##########\n");
	char *dirc_data = "cherry";
	dirc_test(dirc_data,  strlen(dirc_data));

	char *dirc_cn_data = "人";
	dirc_test(dirc_cn_data, strlen(dirc_cn_data));

	printk("######trans_test##########\n");
	char *trans_data = "good morning";
	trans_test(trans_data, strlen(trans_data));

	char *trans_cn_data = "中文翻译测试";
	trans_test(trans_cn_data, strlen(trans_cn_data));

	char *ocr_data = "hello world";
	ocr_text_send(ocr_data, strlen(ocr_data));
#endif

#if 0 // protocol test
	uint8_t *ocr_txt = "good";
	ocr_text_send(ocr_txt, strlen(ocr_txt));

	char *trans_spi = "中文";
	int trnas_spi_len = strlen(trans_spi);
	trans_test(trans_spi, trnas_spi_len);

	#define _BUF_LEN (64)
	uint8_t *test_buffer = lisa_mem_alloc(_BUF_LEN);
	for(int i=0;i<20;i++) {
		test_buffer[16+i] = i;
	}
	uint32_t pack_len = lisa_evs_pack_spi_data(test_buffer, test_buffer+16, 20, LISA_UP_AUDIO);
	// spi send
	printk("tts pack_len:%d\n", pack_len);
	for(int k=0;k<pack_len;k++) {
		printk("%02x ", test_buffer[k]);;
	}
	printk("\n");
#endif

#if 0
	printk("######tts_cn_test##########\n");
	// char *tts_data = "This is an English short sentence synthesis test case";
	char *tts_data = "这是一个中文短句测试用例";
	tts_test(tts_data, strlen(tts_data));
#endif

#if 0
	k_msleep(5000);
	printk("######tts_en_test##########\n");
	char *tts_en_data = "This is an English short sentence synthesis test case";
	tts_test(tts_en_data, strlen(tts_en_data));
#endif

#if 0 // multiline scan timer test
	printk("timer start11\n");
	lisa_err_t sta = lisa_timer_change_period(multiline_scan_timer, 2000);
	if (sta != LISA_OK) {
		LISA_LOGE(TAG, "timer change period error");
	}
	if (!lisa_timer_isactive(multiline_scan_timer)) {
		lisa_timer_start(multiline_scan_timer);
	}
	k_msleep(1000);
	printk("timer start222\n");
	sta = lisa_timer_change_period(multiline_scan_timer, 4000);
	if (sta != LISA_OK) {
		LISA_LOGE(TAG, "timer change period error");
	}
#endif
}

void ocr_test_offline_start()
{
	/* button initialize */
	pen_head_button_init();

	/* image data from AP to dsp */
	create_cam2cp_thread();

	/* dsp callback image process */
	create_offline_recognize_thread();

	/* tts thread */
	create_tts_thread();

	/* spi protocol */
	// lisa_evs_parser_cp_init(NULL);

	/* uart protocol */
	// create_uart_proto_thread();

	/* unit testing */
	test_app();
}
