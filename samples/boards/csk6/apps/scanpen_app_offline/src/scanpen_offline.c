#include "kernel.h"
#include "sys_clock.h"
#include <lisa_log.h>
#include <lisa_thread.h>
#include <lisa_queue.h>
#include <lisa_semaphore.h>
#include <lisa_mem.h>

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


static lisa_semaphore_t *s_ocr_res_sema = NULL;
static lisa_thread_t *s_offline_recognize_thread = NULL;
static lisa_thread_t *s_tts_thread = NULL;
static lisa_semaphore_t *s_tts_sema = NULL;
static lisa_vcapture_handler_t *vcap_handler = NULL;
static lisa_translate_handler_t *trans_handler = NULL;
static lisa_tts_handler_t *tts_handler = NULL;
static volatile lisa_thread_t *s_cam2cp_thread = NULL;
static lisa_thread_t *s_uart_proto_thread = NULL;

static char ocr_result[1024] = {0};

static volatile uint32_t tts_done = 0;


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




void _image_data_cb(lisa_vcapture_cbmsg_e msg, void *data, uint32_t len)
{
	switch (msg) {
		case VCAPTURE_START:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_START \n");
			break;
		case VCAPTURE_STOP:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_STOP \n");
			lisa_semaphore_give(s_ocr_res_sema);
			break;
		case VCAPTURE_DATA: {
		//	LISA_LOGD(TAG, "vcapture msg VCAPTURE_DATA \n");
		} break;
		case VCAPTURE_OCR:
			memset(ocr_result, 0, sizeof(ocr_result));
			LISA_LOGD(TAG, "OCR result [len: %d] %s\n", len, data);
			memcpy(ocr_result, data, len);
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
}

static void trans_test(const char *data, uint32_t len)
{
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
}

void  offline_recognize_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);

	s_ocr_res_sema = lisa_semaphore_create(1);

	while (true) {
		lisa_semaphore_take(s_ocr_res_sema, LISA_WAIT_FOREVER);
		
		k_msleep(100);
	}

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
			break;
		default:
			LISA_LOGE(TAG, "unsupport tts msg\n");
			break;
	}
}

static void tts_test_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);

	printk("######dirc_test##########\n");
	char *dirc_data = "cherry";
	int dirc_len = strlen(dirc_data);
	dirc_test(dirc_data, dirc_len);

	char *dirc_cn_data = "人";
	dirc_len = strlen(dirc_cn_data);
	dirc_test(dirc_cn_data, dirc_len);

	printk("######trans_test##########\n");
	char *trans_data = "good morning";
	int trnas_len = strlen(trans_data);
	trans_test(trans_data, trnas_len);

	char *trans_cn_data = "中文翻译测试";
	trnas_len = strlen(trans_cn_data);
	trans_test(trans_cn_data, trnas_len);

#if TTS_AUDIO_SAVE_FLASH
	tts_audio_buffer = lisa_mem_alloc(100 * 4096);
#endif

	s_tts_sema = lisa_semaphore_create(1);

	// char *tts_data = "This is an English short sentence synthesis test case";
	char *tts_data = "这是一个中文短句测试用例";
	/* create tts */
	lisa_tts_params_t params;
	memset(&params, 0, sizeof(lisa_tts_params_t));
	params.ttscb = _tts_cb;
	tts_handler = lisa_tts_create(&params);
	LISA_LOGD(TAG, "TTS_INIT_DONE \n");

	atts_config_t tts_config = {
		.role 		= ivTTS_ROLE_XIAOYAN,
		.language	= ivTTS_LANGUAGE_AUTO,
		.speed		= ivTTS_SPEED_NORMAL,
		.volume		= ivTTS_VOLUME_NORMAL,
	};
	if(tts_data[0] > 127){
		tts_config.role = ivTTS_ROLE_XIAOYAN;
	}else{
		tts_config.role = ivTTS_ROLE_CATHERINE;
	}
	lisa_tts_play(tts_handler, tts_data, &tts_config);

	while (true) {
		lisa_semaphore_take(s_tts_sema, LISA_WAIT_FOREVER);
		lisa_tts_stop(tts_handler);
		lisa_tts_destroy(tts_handler);

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
}

/**
 * @brief Create a tts thread object
 *
 */
void create_tts_thread()
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
		LISA_LOGE(TAG, "thread_create:  offline_recognize_thread failed");
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
	} else if (sta == RELEASE) {
		lisa_vcapture_stop(vcap_handler);
		lisa_vcapture_destroy(vcap_handler);
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
	int pack_len = 0;
	uint16_t algo_out_len = 0;
	char *algo_out = NULL;

	switch (type) {
		case e_cmd_scan_tts_text:
			LISA_LOGD(TAG, "tts text :%s, len:%d\n", data, len);
			lisa_tts_params_t params;
			memset(&params, 0, sizeof(lisa_tts_params_t));
			params.ttscb = _tts_cb;
			tts_handler = lisa_tts_create(&params);
			break;
		case e_cmd_scan_translate_text:
			trans_test(data, len);
			break;
		case e_cmd_scan_word_search_text:
			dirc_test(data, len);
			break;
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
}
