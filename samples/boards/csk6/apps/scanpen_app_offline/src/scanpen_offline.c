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

static struct lisa_ring_buffer *s_send_ocr_simulation_image_buffer = NULL;
static lisa_semaphore_t *s_ocr_image_sema = NULL;
static lisa_semaphore_t *s_ocr_res_sema = NULL;
static lisa_thread_t *s_offline_recognize_thread = NULL;
static lisa_thread_t *s_uart_proto_thread = NULL;
static lisa_vcapture_handler_t *vcap_handler = NULL;
static lisa_translate_handler_t *trans_handler = NULL;
static lisa_tts_handler_t *tts_handler = NULL;
static volatile lisa_thread_t *s_cam2cp_thread = NULL;

static char ocr_result[1024] = {0};


typedef struct {
	volatile char *addr;
	volatile uint32_t frame_size;
	volatile uint32_t frame_cnt;
	volatile uint32_t w_pos;
	volatile uint32_t r_pos;
	volatile uint32_t tag;
} __packed frames_info_t;

typedef struct frame_info {
	uint32_t id;
	uint32_t sum;
} __packed frame_info_t;

Z_GENERIC_SECTION(.psram_section)
__attribute__((aligned(32),
	       used)) static char swp_frames[5][sizeof(frame_info_t) + 180 * 128] = { 0 };

typedef struct {
	lisa_image_transfer_status_e status;
	char *data;
	int size;
} lisa_image_msg_t;

/**
 * @brief 从ringbuf中接受一组图片
 *
 * @param ring_buffer
 * @param data
 * @param len
 * @param wait_ms
 * @return int
 */
static int recv_ocr_simulation_image_once(
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
static void send_ocr_simulation_image_once(struct lisa_ring_buffer *ring_buffer, lisa_image_msg_t *data)
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

static void _image_data_cb(lisa_vcapture_cbmsg_e msg, void *data, uint32_t len)
{
	switch (msg) {
		case VCAPTURE_START:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_START \n");
			break;
		case VCAPTURE_STOP:
			LISA_LOGD(TAG, "vcapture msg VCAPTURE_STOP \n");
			// lisa_semaphore_give(s_ocr_res_sema);
			break;
		case VCAPTURE_DATA: {
			// LISA_LOGD(TAG, "vcapture msg VCAPTURE_DATA len:%d \n", len);
			lisa_image_msg_t recv_image;
			recv_image.status = LISA_IMAGE_FRAME_NONE;
			recv_image.data = (char *)data;
			recv_image.size = len;
			send_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, &recv_image);
		} break;
		case VCAPTURE_OCR:
			LISA_LOGD(TAG, "OCR result [len: %d] %s\n", len, data);
			// spi send ocr
			uint32_t pack_len = 0;
			// spi pack
			uint8_t *text_pack = (uint8_t *)lisa_mem_alloc(len);
			if(text_pack) {
				pack_len = lisa_evs_pack_spi_data(text_pack, (uint8_t *)data, len, LISA_UP_TEXT_OCR);
				printk("pack len:%d\n", pack_len);
				for(int i=0;i < pack_len; i++) {
					printk("%02x ", text_pack[i]);
				}
				printk("\n");
			}
			// spi send
			lisa_spi_write(text_pack, pack_len);
			lisa_thread_mdelay(50);
			break;
		default:
			LISA_LOGE(TAG, "unsupport vcapture msg\n");
			break;
	}
}

static void _tts_cb(lisa_tts_cbmsg_e msg, void *data, uint32_t len)
{
	switch (msg) {
		case TTS_START:
			LISA_LOGD(TAG, "TTS_START \n");
			break;
		case TTS_PLAY_OVER:
			LISA_LOGD(TAG, "TTS_PLAY_OVER \n");
			//lisa_tts_stop(tts_handler);
			//lisa_tts_destroy(tts_handler);
			break;
		default:
			LISA_LOGE(TAG, "unsupport tts msg\n");
			break;
	}
}

static void _uart_recv_cb(recv_cmd_type_e type, void *data, uint32_t len)
{
	printk("@@recv_cmd_type_e:%d\n", type);
	int pack_len = 0;
	switch (type) {
		case e_cmd_scan_tts_text:
			LISA_LOGD(TAG, "tts text :%s, len:%d\n", data, len);
			lisa_tts_params_t params;
			memset(&params, 0, sizeof(lisa_tts_params_t));
			params.ttscb = _tts_cb;
			tts_handler = lisa_tts_create(&params);

			// lisa_tts_play(tts_handler, trans_data);
			break;
		case e_cmd_scan_translate_text:
			memset(ocr_result, 0, 1024);
			memcpy(ocr_result, data, len);
			LISA_LOGD(TAG, "translate text :%s, len:%d\n", data, len);
			char trans_data[1024] = {0};
			trans_handler = lisa_translate_create();
			lisa_translate_exec(trans_handler, ocr_result, trans_data, 1024, 40000);
			lisa_translate_destroy(trans_handler);
			uint16_t trans_len = strlen(trans_data);
			// spi pack
			uint8_t *text_pack = (uint8_t *)lisa_mem_alloc(trans_len+16);
			if(text_pack) {
				pack_len = lisa_evs_pack_spi_data(text_pack, (uint8_t *)trans_data, trans_len, LISA_UP_TEXT_TRANSLATE);
				printk("pack len:%d\n", pack_len);
				for(int i=0;i < pack_len; i++) {
					printk("%02x ", text_pack[i]);
				}
				printk("\n");
			}
			// spi send
			lisa_spi_write(text_pack, pack_len);
			lisa_thread_mdelay(50);
			// pack free
			if(text_pack) lisa_mem_free(text_pack);
			break;
		case e_cmd_scan_word_search_text:
			LISA_LOGD(TAG, "word_search text :%s, len:%d\n", data, len);
			break;
		default:
			LISA_LOGE(TAG, "unsupport tts msg\n");
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

static void  offline_recognize_thread(void *arg)
{
	LISA_LOGI(TAG, "%s", __FUNCTION__);

	s_ocr_res_sema = lisa_semaphore_create(1);

	if (!s_send_ocr_simulation_image_buffer) {
		unsigned char *buffer = lisa_mem_alloc(64 * 1024);
		s_send_ocr_simulation_image_buffer = lisa_ring_buffer_init(buffer, 64 * 1024);
		s_ocr_image_sema = lisa_semaphore_create(2);
	}

	#define CROP_BUF_LEN (4*1024)
	unsigned char *crop_image_buffer = lisa_mem_alloc(CROP_BUF_LEN);
	int ret_len = 0;
	while (true) {
		// lisa_semaphore_take(s_ocr_res_sema, LISA_WAIT_FOREVER);
		ret_len = recv_ocr_simulation_image_once(s_send_ocr_simulation_image_buffer, crop_image_buffer, CROP_BUF_LEN, LISA_WAIT_FOREVER);
		// spi send, unnecessary packed
		printk("ret_len:%d\n", ret_len);
		lisa_spi_write(crop_image_buffer, ret_len);
	}
}

/**
 * @brief Create a ocr recognize thread object
 *
 */
static void create_offline_recognize_thread(void)
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


static inline char *bf30a2_frames_info_get_free(frames_info_t *info)
{
	if (info == NULL || info->addr == NULL || info->frame_cnt <= 0) {
		return NULL;
	}

	SCB_InvalidateDCache_by_Addr(info, sizeof(frames_info_t));
	int r = info->r_pos;
	int w = info->w_pos;
	int cnt = info->frame_cnt;
	char *addr = info->addr;

	w %= info->frame_cnt;

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

	return info->addr + w * info->frame_size;
}

static inline int32_t bf30a2_frames_info_update_w_pos(frames_info_t *info)
{
	if (info == NULL || info->addr == NULL || info->frame_cnt <= 0) {
		return -1;
	}

	info->w_pos++;
	info->w_pos %= info->frame_cnt;
	SCB_CleanDCache_by_Addr(info, sizeof(frames_info_t));

	return info->w_pos;
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


static void video_ap2cp_thread(void *arg1)
{
	struct video_buffer *vbuf = NULL;
	frames_info_t *info = (frames_info_t *)0x30704000;

	const struct device *video = device_get_binding(VIDEO_DEV);

	if (!device_is_ready(video)) {
		printk("ipm device is null.\n");
		return;
	}
	memset(info, 0, sizeof(frames_info_t));
	info->addr = &swp_frames[0][0];
	info->frame_size = 180 * 128 + sizeof(frame_info_t);
	info->frame_cnt = 5;
	info->w_pos = 0;
	info->r_pos = 0;
	info->tag = 0x12345678;

	LISA_LOGI(TAG, "##########Frames info##########");
	LISA_LOGI(TAG, "info       :%p.", info);
	LISA_LOGI(TAG, "buffer addr:%p.", info->addr);
	LISA_LOGI(TAG, "frame  size:%u.", info->frame_size);
	LISA_LOGI(TAG, "frame count:%d.", info->frame_cnt);
	LISA_LOGI(TAG, "write   pos:%d.", info->w_pos);
	LISA_LOGI(TAG, "read    pos:%d.", info->r_pos);

	uint32_t last_tick = 0;
	int id = 0;
	int fps = 0;
	int frame_cnt = 0;
	int discard_cnt = 0;
	while (1) {
		vbuf = NULL;
		video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		frame_cnt++;
		/* send datas to cp core */
		char *buffer = bf30a2_frames_info_get_free(info);
		if (buffer != NULL) {
			frame_info_t *frame_info = (frame_info_t *)buffer;
			frame_info->id = id++;
			memcpy(buffer + sizeof(frame_info_t), vbuf->buffer, 180 * 128);
			frame_info->sum = calc_sum(buffer + sizeof(frame_info_t), 180 * 128);
			SCB_CleanDCache_by_Addr(buffer, info->frame_size);
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

static void create_cam2cp_thread()
{

	if (s_cam2cp_thread) {
		LISA_LOGE(TAG, "s_cam2cp_thread exist ");
		return;
	}
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "cam2cp_thread";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;  // OS_PRIORITY_NORMAL

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
		lisa_translate_destroy(trans_handler);
		lisa_tts_stop(tts_handler);
		lisa_tts_destroy(tts_handler);
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

void ocr_test_offline_start()
{
	pen_head_button_init();
	create_cam2cp_thread();
	create_offline_recognize_thread();
	// lisa_evs_parser_cp_init(NULL);
	// create_uart_proto_thread();
}
