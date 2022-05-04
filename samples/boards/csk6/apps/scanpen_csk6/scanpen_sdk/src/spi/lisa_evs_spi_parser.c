#include "lisa_evs_spi_parser.h"
#include <lisa_log.h>
#include <lisa_time.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>
#include <lisa_err.h>
#include <string.h>

#define TAG "spi_parser"

#define IMAGE_FRM_HDR_TAG (0x5a46)
#define IMAGE_FRM_HDR_TAG_L (0x46)
#define IMAGE_FRM_HDR_TAG_H (0x5a)
#define IMAGE_FRM_HDR_LEN (16)
#define BITMAP_WIDTH (128)
#define BITMAP_HEIGHT (180)
#define BITMAP_DEPTH (1)
#define BITMAP_LEN (BITMAP_WIDTH * BITMAP_HEIGHT * BITMAP_DEPTH)
#define IMAGE_FRM_MAX_LEN (IMAGE_FRM_HDR_LEN + BITMAP_LEN)
#define IMAGE_FRM_HDR_CONTINUE_LEN (IMAGE_FRM_HDR_LEN - 2)

#define BITMAP_ORIGIN_WIDTH (240)
#define BITMAP_ORIGIN_HEIGHT (320)
#define BITMAP_ORIGIN_LEN (BITMAP_ORIGIN_WIDTH * BITMAP_ORIGIN_HEIGHT * BITMAP_DEPTH)
#define ORIGIN_IMAGE_FRM_MAX_LEN (IMAGE_FRM_HDR_LEN + BITMAP_ORIGIN_LEN)

#define CV_FRAME_FORMAT_INDEX_TAG (0)
#define CV_FRAME_FORMAT_INDEX_VER (2)
#define CV_FRAME_FORMAT_INDEX_FUID (3)
#define CV_FRAME_FORMAT_INDEX_TYPE (5)
#define CV_FRAME_FORMAT_INDEX_FMT (6)
#define CV_FRAME_FORMAT_INDEX_X (7)
#define CV_FRAME_FORMAT_INDEX_Y (8)
#define CV_FRAME_FORMAT_INDEX_WIDTH (9)
#define CV_FRAME_FORMAT_INDEX_HEIGHT (11)
#define CV_FRAME_FORMAT_INDEX_DEPTH (13)
#define CV_FRAME_FORMAT_INDEX_RESERVED (14)
#define CV_FRAME_FORMAT_INDEX_IMAGEDATA (16)

#define TTS_FRM_HDR_TAG (0x5b46)
#define TTS_FRM_HDR_TAG_H (0x5b)
#define TTS_FRM_HDR_LEN (12)
#define TTS_FRM_HDR_CONTINUE_LEN (TTS_FRM_HDR_LEN - 2)

#define TTS_FRAME_FORMAT_INDEX_FMT (2)
#define TTS_FRAME_FORMAT_INDEX_CHANNEL (3)
#define TTS_FRAME_FORMAT_INDEX_SAMPLE (4)
#define TTS_FRAME_FORMAT_INDEX_DEPTH (5)
#define TTS_FRAME_FORMAT_INDEX_LENGTH (6)
#define TTS_FRAME_FORMAT_INDEX_CHECKSUM (10)

#define OCR_FRM_HDR_TAG (0x5c46)
#define OCR_FRM_HDR_TAG_H (0x5c)
#define OCR_FRM_HDR_LEN (8)
#define OCR_FRM_HDR_CONTINUE_LEN (OCR_FRM_HDR_LEN - 2)

#define OCR_FRAME_FORMAT_INDEX_LENGTH (2)
#define OCR_FRAME_FORMAT_INDEX_CHECKSUM (6)

typedef struct {
	// uint8_t image_frame_data[IMAGE_FRM_MAX_LEN];
	uint8_t *image_frame_data;
} image_frame_desc_t;

static image_frame_desc_t proto_recv_algo __attribute__((aligned(32))) = {{0}};
static image_data_callback g_image_cb = NULL;
static tts_data_callback g_tts_cb = NULL;
static ocr_data_callback g_ocr_cb = NULL;
static read_spi_data g_read_cb = NULL;
// static uint8_t image_data_buffer[BITMAP_LEN] = {0};
static uint8_t* image_data_buffer = NULL;
lisa_image_transfer_status_e g_img_status = LISA_IMAGE_FRAME_NONE;


static uint16_t
_image_pack_check(void *dat, uint16_t len)
{
	uint16_t check = 0;
	uint8_t *data = (uint8_t *)dat;
	while (len--)
		check += *data++;
	return check;
}

static bool
_frame_check_head(image_frame_desc_t *head)
{
	// memset(head, 0, sizeof(image_frame_desc_t));

	if (1 != g_read_cb((uint8_t *)&(head->image_frame_data[0]), 1, LISA_WAIT_FOREVER)) {
		return false;
	} else {
		if (IMAGE_FRM_HDR_TAG_L != head->image_frame_data[0]) return false;
	}
	if (1 != g_read_cb((uint8_t *)&(head->image_frame_data[1]), 1, LISA_WAIT_FOREVER)) {
		return false;
	} else {
		if (IMAGE_FRM_HDR_TAG_H != head->image_frame_data[1] && TTS_FRM_HDR_TAG_H != head->image_frame_data[1] && OCR_FRM_HDR_TAG_H != head->image_frame_data[1]) return false;
	}
	return true;
}

static bool
_frame_read_data(image_frame_desc_t *head, img_frame_t* _frame_data)
{
	uint16_t to_recv = 0;

	if (IMAGE_FRM_HDR_CONTINUE_LEN != g_read_cb((uint8_t *)&(head->image_frame_data[2]), IMAGE_FRM_HDR_CONTINUE_LEN, LISA_WAIT_FOREVER)) {
		LISA_LOGE(TAG, "queue read IMAGE_FRM_HDR_LEN -2(4) error\n.");
		return false;
	}

	_frame_data->height = (head->image_frame_data[CV_FRAME_FORMAT_INDEX_HEIGHT] & 0xff) | (head->image_frame_data[CV_FRAME_FORMAT_INDEX_HEIGHT+1] << 8);
	_frame_data->width = (head->image_frame_data[CV_FRAME_FORMAT_INDEX_WIDTH] & 0xff) | (head->image_frame_data[CV_FRAME_FORMAT_INDEX_WIDTH+1] << 8);
	_frame_data->depth = head->image_frame_data[CV_FRAME_FORMAT_INDEX_DEPTH];
	_frame_data->fuid = (head->image_frame_data[CV_FRAME_FORMAT_INDEX_FUID] & 0xff) | (head->image_frame_data[CV_FRAME_FORMAT_INDEX_FUID+1] << 8);
	_frame_data->checksum = (head->image_frame_data[CV_FRAME_FORMAT_INDEX_RESERVED] & 0xff) | (head->image_frame_data[CV_FRAME_FORMAT_INDEX_RESERVED+1] << 8);

	to_recv = _frame_data->height * _frame_data->width * _frame_data->depth;
	uint16_t recv_len = 0;
	uint16_t once_len = 0;
	uint16_t recv_total_len = 0;
	if(to_recv > IMAGE_FRM_MAX_LEN) {
		LISA_LOGW(TAG, "framesize(%d)\n", to_recv);
		return false;
	}

	while (recv_total_len < to_recv) {
		once_len = to_recv - recv_total_len;
		recv_len = g_read_cb(image_data_buffer + recv_total_len, once_len, LISA_WAIT_FOREVER);
		if (recv_len > 0) recv_total_len += recv_len;
	}

	if (recv_total_len <= to_recv) {
		memcpy((uint8_t *)&(head->image_frame_data[IMAGE_FRM_HDR_LEN]), image_data_buffer, recv_total_len);
	}

#ifndef NOT_CHECK_CSK_SPI
	uint16_t check_sum = _image_pack_check((void *)image_data_buffer, to_recv);
	if (_frame_data->checksum != check_sum) {
		LISA_LOGW(TAG, "id:%d, checksum:%d, error:%d", _frame_data->fuid,  _frame_data->checksum, check_sum);
		return false;
	}
#endif

#if 0//LISTEN_LOG_LEVEL == LISTEN_LOG_LEVEL_VERBOSE
	printf("DART RECVED :\n");
	for (int i = 0; i < to_recv+IMAGE_FRM_HDR_LEN; i++) {
		LISA_LOGE(TAG, "%02x ", head->image_frame_data[i]);
		if ((i + 1) % 16 == 0) printf("\n");
	}
	printf("\n");
#endif

	return true;
}

static bool
_tts_frame_read_data(image_frame_desc_t *head, tts_frame_t* _frame_data) {
	uint16_t to_recv = 0;

	if (TTS_FRM_HDR_CONTINUE_LEN != g_read_cb((uint8_t *)&(head->image_frame_data[2]), TTS_FRM_HDR_CONTINUE_LEN, LISA_WAIT_FOREVER)) {
		LISA_LOGE(TAG, "queue read IMAGE_FRM_HDR_LEN -2(4) error\n.");
		return false;
	}

	_frame_data->fmt = head->image_frame_data[TTS_FRAME_FORMAT_INDEX_FMT];
	_frame_data->channel = head->image_frame_data[TTS_FRAME_FORMAT_INDEX_CHANNEL];
	_frame_data->sample = head->image_frame_data[TTS_FRAME_FORMAT_INDEX_SAMPLE];
	_frame_data->depth = head->image_frame_data[TTS_FRAME_FORMAT_INDEX_DEPTH];
	_frame_data->len = (head->image_frame_data[TTS_FRAME_FORMAT_INDEX_LENGTH] & 0xff) 
		| ((head->image_frame_data[TTS_FRAME_FORMAT_INDEX_LENGTH+1] << 8) & 0xffff)
		| ((head->image_frame_data[TTS_FRAME_FORMAT_INDEX_LENGTH+2] << 16) & 0xffffff)
		| ((head->image_frame_data[TTS_FRAME_FORMAT_INDEX_LENGTH+3] << 32));

	_frame_data->checksum = (head->image_frame_data[TTS_FRAME_FORMAT_INDEX_CHECKSUM] & 0xff) | (head->image_frame_data[TTS_FRAME_FORMAT_INDEX_CHECKSUM+1] << 8);

	to_recv = _frame_data->len;
	uint16_t recv_len = 0;
	uint16_t once_len = 0;
	uint16_t recv_total_len = 0;
	if(to_recv > IMAGE_FRM_MAX_LEN) {
		LISA_LOGW(TAG, "framesize(%d)\n", to_recv);
		return false;
	}

	uint8_t* tts_audio = lisa_mem_alloc(to_recv);
	if (tts_audio == NULL) {
		LISA_LOGE(TAG, "malloc fail size(%d)\n", to_recv);
		return false;
	}

	_frame_data->data = tts_audio;
	while (recv_total_len < to_recv) {
		once_len = to_recv - recv_total_len;
		recv_len = g_read_cb(image_data_buffer + recv_total_len, once_len, LISA_WAIT_FOREVER);
		if (recv_len > 0) recv_total_len += recv_len;
	}

	if (recv_total_len <= to_recv) {
		memcpy(tts_audio, image_data_buffer, recv_total_len);
	}

#ifndef NOT_CHECK_CSK_SPI
	uint16_t check_sum = _image_pack_check((void *)image_data_buffer, to_recv);
	if (_frame_data->checksum != check_sum) {
		LISA_LOGE(TAG, "checksum:%d, error:%d", _frame_data->checksum, check_sum);
		return false;
	}
#endif

	return true;
}

static bool
_ocr_frame_read_data(image_frame_desc_t *head, ocr_frame_t* _frame_data) {
	uint16_t to_recv = 0;

	if (OCR_FRM_HDR_CONTINUE_LEN != g_read_cb((uint8_t *)&(head->image_frame_data[2]), OCR_FRM_HDR_CONTINUE_LEN, LISA_WAIT_FOREVER)) {
		LISA_LOGE(TAG, "queue read IMAGE_FRM_HDR_LEN -2(4) error\n.");
		return false;
	}

	_frame_data->len = (head->image_frame_data[OCR_FRAME_FORMAT_INDEX_LENGTH] & 0xff) 
		| ((head->image_frame_data[OCR_FRAME_FORMAT_INDEX_LENGTH+1] << 8) & 0xffff)
		| ((head->image_frame_data[OCR_FRAME_FORMAT_INDEX_LENGTH+2] << 16) & 0xffffff)
		| ((head->image_frame_data[OCR_FRAME_FORMAT_INDEX_LENGTH+3] << 32));

	_frame_data->checksum = (head->image_frame_data[OCR_FRAME_FORMAT_INDEX_CHECKSUM] & 0xff) | (head->image_frame_data[OCR_FRAME_FORMAT_INDEX_CHECKSUM+1] << 8);

	to_recv = _frame_data->len;
	uint16_t recv_len = 0;
	uint16_t once_len = 0;
	uint16_t recv_total_len = 0;
	if(to_recv > IMAGE_FRM_MAX_LEN) {
		LISA_LOGW(TAG, "framesize(%d)\n", to_recv);
		return false;
	}

	uint8_t* ocr_result = lisa_mem_alloc(to_recv + 1);
	if (ocr_result == NULL) {
		LISA_LOGE(TAG, "malloc fail size(%d)\n", to_recv + 1);
		return false;
	}

	_frame_data->data = ocr_result;
	while (recv_total_len < to_recv) {
		once_len = to_recv - recv_total_len;
		recv_len = g_read_cb(image_data_buffer + recv_total_len, once_len, LISA_WAIT_FOREVER);
		if (recv_len > 0) recv_total_len += recv_len;
	}

	if (recv_total_len <= to_recv) {
		memcpy(ocr_result, image_data_buffer, recv_total_len);
	}
	ocr_result[to_recv] = '\0';

#ifndef NOT_CHECK_CSK_SPI
	uint16_t check_sum = _image_pack_check((void *)image_data_buffer, to_recv);
	if (_frame_data->checksum != check_sum) {
		LISA_LOGE(TAG, "checksum:%d, error:%d", _frame_data->checksum, check_sum);
		return false;
	}
#endif
	return true;
}

lisa_err_t lisa_evs_parser_spi_data()
{
    if (!g_read_cb) return false;

    img_frame_t _image_frame_data;
	tts_frame_t _tts_frame_data;
	ocr_frame_t _orc_frame_data;
	image_frame_desc_t *head = NULL;
	head = (image_frame_desc_t *)&proto_recv_algo;

    // parser head
	if (_frame_check_head(head) == false) return false;

    // parser body
    switch ((head->image_frame_data[0] & 0xff) | (head->image_frame_data[1] << 8)) {
    case IMAGE_FRM_HDR_TAG: // parser image
        if (_frame_read_data(head, &_image_frame_data) == false) return false;
		uint32_t image_len = IMAGE_FRM_HDR_LEN + _image_frame_data.height * _image_frame_data.width * _image_frame_data.depth;
		if(g_image_cb) {
			g_image_cb(head->image_frame_data, image_len, g_img_status);
			if(LISA_IMAGE_FRAME_START == g_img_status) {
				g_img_status = LISA_IMAGE_FRAME_CONTINUE;
			}
		}
        break;
    case TTS_FRM_HDR_TAG:   // parser tts
        if (_tts_frame_read_data(head, &_tts_frame_data) == false) return false;
		if (g_tts_cb) g_tts_cb(&_tts_frame_data);
		if (_tts_frame_data.data) lisa_mem_free(_tts_frame_data.data);
        break;
    case OCR_FRM_HDR_TAG:   // parser ocr_result
        if (_ocr_frame_read_data(head, &_orc_frame_data) == false) return false;
		if (g_ocr_cb) g_ocr_cb(&_orc_frame_data);
		if (_orc_frame_data.data) lisa_mem_free(_orc_frame_data.data);
        break;
    default:
        break;
    }
    return LISA_OK;
}

lisa_err_t lisa_evs_parser_spi_init(read_spi_data read_cb)
{
    if (!read_cb) return LISA_FAIL;
    g_read_cb = read_cb;
	if (!image_data_buffer) {
		image_data_buffer = lisa_mem_calloc(1, BITMAP_LEN);
	}
	if (!proto_recv_algo.image_frame_data) {
		proto_recv_algo.image_frame_data = lisa_mem_calloc(1, IMAGE_FRM_MAX_LEN);
	}
	return LISA_OK;
}

int lisa_evs_register_image_callback(image_data_callback image_cb)
{
	g_image_cb = image_cb;
	return 0;
}

int lisa_evs_register_tts_callback(tts_data_callback tts_cb)
{
	g_tts_cb = tts_cb;
	return 0;
}

int lisa_evs_register_ocr_callback(ocr_data_callback ocr_cb)
{
	g_ocr_cb = ocr_cb;
	return 0;
}
