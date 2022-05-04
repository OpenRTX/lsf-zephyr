#include <stdio.h>
#include <string.h>

#include "image_frame_protocol.h"
#include "jpec/include/jpec.h"
#include <lisa_mem.h>
#include <lisa_log.h>

/**
 *  0   2   3    5    6   7    8     9   11    13      14     16
 * TAG VER FUID TYPE FMT XSFT YSFT WIDTH HEIGH DEPTH RESVERSD DATA
 *  2   1   2    1    1   1    1     2     2     1       2      N
 * 5A46 00 NUM
 */

#define TAG "lisa_img"

static uint16_t data_build_up(uint8_t high, uint8_t low)
{ return (((uint16_t)(high)&0x00ff) << 8) | (((uint16_t)(low)&0x00ff)); }
//大小端
static uint8_t data_get_high(uint16_t x) { return (uint8_t)((x & 0xff00) >> 8); }
static uint8_t data_get_low(uint16_t x) { return (uint8_t)(x & 0x00ff); }

int image_frame_data_parser(
		const uint8_t *in_data, uint32_t in_data_len, img_frame_protocol_t *out_img_frame_msg)
{
	if (in_data == NULL || in_data_len <= CROP_DATA_HEADER_LEN || out_img_frame_msg == NULL) {
		LISA_LOGE(TAG, "in data error.");
		return -1;
	}

	out_img_frame_msg->tag = data_build_up(in_data[1], in_data[0]);
	if (out_img_frame_msg->tag != IMAGE_FRAME_MASSAGE_TAG) {
		LISA_LOGE(TAG, "check data header error.");
		return -2;
	}

	out_img_frame_msg->xsft = (int8_t) in_data[7];
	out_img_frame_msg->ysft = (int8_t) in_data[8];
	out_img_frame_msg->width = data_build_up(in_data[10], in_data[9]);
	out_img_frame_msg->height = data_build_up(in_data[12], in_data[11]);
	out_img_frame_msg->depth = in_data[13];
	out_img_frame_msg->fuid = data_build_up(in_data[4], in_data[3]);

	uint16_t out_data_len =
			out_img_frame_msg->width * out_img_frame_msg->height * out_img_frame_msg->depth;
	if (out_data_len <= 0) {
		LISA_LOGE(TAG, "out data error.");
		return -3;
	}

	out_img_frame_msg->data = (uint8_t *)lisa_mem_alloc(out_data_len);

	uint8_t *tmp_data_ptr = (uint8_t *)in_data;
	memcpy(out_img_frame_msg->data, tmp_data_ptr + CROP_DATA_HEADER_LEN, out_data_len);
	return 0;
}

int image_frame_data_build(const img_frame_protocol_t * const in_img_frame_msg,
							uint8_t **out_data,
							uint32_t *ou_data_len,
							evs_rec_img_compress compress_fun,
							bool is_debug)
{
	if (in_img_frame_msg == NULL || in_img_frame_msg->data == NULL ||
		out_data == NULL || *out_data == NULL)
		return -1;

	uint32_t compress_before_len = in_img_frame_msg->width * in_img_frame_msg->height;
	uint32_t compress_after_len = compress_before_len;
	uint8_t *compress_data_ptr = (*out_data) + CROP_DATA_HEADER_LEN + (is_debug? 2:0);
	if (!is_debug && !compress_fun) {
		LISA_LOGE(TAG, "compress function is null, build image frame error.");
		return -1;
	}

	lisa_evs_color_space_e color_space = EVS_COMPRESS_GRAYSCALE;
	switch (in_img_frame_msg->fmt) {
		// 灰度图
		case 0x00: color_space = EVS_COMPRESS_GRAYSCALE; break;
		default: break;
	}
	if (compress_fun) {
		int compress_ret = compress_fun(in_img_frame_msg->data,
								in_img_frame_msg->width,
								in_img_frame_msg->height,
								in_img_frame_msg->depth,
								color_space,
								75,
								&compress_data_ptr,
								&compress_after_len);
		if (compress_ret != 0) {
			LISA_LOGE(TAG, "compress image data fail.");
			return -1;
		}
	}

	if(is_debug) {
		uint8_t img_frame_header[CROP_DATA_HEADER_LEN + 2] = {
			// 标记 TAG
			data_get_low(in_img_frame_msg->tag), data_get_high(in_img_frame_msg->tag),
			// 版本 VER
			in_img_frame_msg->ver,
			// 图像编号 FUID
			data_get_low(in_img_frame_msg->fuid), data_get_high(in_img_frame_msg->fuid),
			// 图像类型 TYPE
			in_img_frame_msg->type,
			// 图像格式 FMT
			in_img_frame_msg->fmt,
			// X偏移 XSFT
			in_img_frame_msg->xsft,
			// Y偏移 YSFT
			in_img_frame_msg->ysft,
			// 图像宽度 WIDTH
			data_get_low(in_img_frame_msg->width), data_get_high(in_img_frame_msg->width),
			// 图像高度 HEIGHT
			data_get_low(in_img_frame_msg->height), data_get_high(in_img_frame_msg->height),
			// 像素字长 DEPTH
			in_img_frame_msg->depth,
			// 保留 RESERVED
			(uint8_t)(compress_after_len & 0x000000ff),
			(uint8_t)((compress_after_len & 0x0000ff00) >> 8),
			(uint8_t)((compress_after_len & 0x00ff0000) >> 16),
			(uint8_t)((compress_after_len & 0xff000000) >> 24)
		};

		*ou_data_len = CROP_DATA_HEADER_LEN + 2 + compress_after_len;
		memcpy(*out_data, img_frame_header, CROP_DATA_HEADER_LEN + 2);
		memcpy(compress_data_ptr, in_img_frame_msg->data, compress_after_len);
	} else {
		uint8_t img_frame_header[CROP_DATA_HEADER_LEN] = {
			// 标记 TAG
			data_get_low(in_img_frame_msg->tag), data_get_high(in_img_frame_msg->tag),
			// 版本 VER
			in_img_frame_msg->ver,
			// 图像编号 FUID
			data_get_low(in_img_frame_msg->fuid), data_get_high(in_img_frame_msg->fuid),
			// 图像类型 TYPE
			in_img_frame_msg->type,
			// 图像格式 FMT
			in_img_frame_msg->fmt,
			// X偏移 XSFT
			in_img_frame_msg->xsft,
			// Y偏移 YSFT
			in_img_frame_msg->ysft,
			// 图像宽度 WIDTH
			data_get_low(in_img_frame_msg->width), data_get_high(in_img_frame_msg->width),
			// 图像高度 HEIGHT
			data_get_low(in_img_frame_msg->height), data_get_high(in_img_frame_msg->height),
			// 像素字长 DEPTH
			in_img_frame_msg->depth,
			// 保留 RESERVED
			data_get_low(compress_after_len), data_get_high(compress_after_len)
		};

		*ou_data_len = CROP_DATA_HEADER_LEN + compress_after_len;
		memcpy(*out_data, img_frame_header, CROP_DATA_HEADER_LEN);
	}
	return 0;
}

int inner_image_jpeg_compress_fun(
					const uint8_t *const image_data,
					int image_width,
					int image_height,
					int components,
					lisa_evs_color_space_e color_space,
					int quality,
					uint8_t **compress_data,
					uint32_t *compress_len)
{
	int ret = -1;
	jpec_enc_t *e = jpec_enc_new2(image_data, image_width, image_height, quality);
	if (e) {
		int len = 0;
	  const uint8_t *jpeg = jpec_enc_run(e, &len);
		if (jpeg && len > 0) {
			memcpy(*compress_data, jpeg, len);
			*compress_len = len;
			ret = 0;
		}
		jpec_enc_del(e);
	}
	return ret;
}
