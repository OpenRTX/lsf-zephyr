#ifndef __LISA_EVS_IMAGE_FRAME_PROTOCOL__
#define __LISA_EVS_IMAGE_FRAME_PROTOCOL__

#include <stdint.h>
#include "lisa_evs_rec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CROP_DATA_HEADER_LEN (16)
#define IMAGE_FRAME_MASSAGE_TAG (0x5A46)	// TAG
#define IMAGE_FRAME_MASSAGE_VER (0x00)		// 版本
#define IMAGE_FRAME_MASSAGE_TYPE (0x00)		// 增量图像
#define IMAGE_FRAME_MASSAGE_TYPE_ORI (0x01)	// 原始图像
#define IMAGE_FRAME_MASSAGE_FMT (0x30)		// 送到云端的图像格式

//图像帧数据协议
typedef struct {
	uint16_t tag;  //标记
	uint8_t ver;  //版本
	uint16_t fuid;  //图像编号
	uint8_t type;  //图像类型
	uint8_t fmt;  //图像格式
	int8_t xsft;  // x偏移
	int8_t ysft;  // y偏移
	uint16_t width;  //图像宽度
	uint16_t height;  //图像高度
	uint8_t depth;  //图像深度
	uint16_t reserved;  //保留  --- PS: 此位当用作云端协议时，用作压缩后数据长度
	uint8_t *data;  //数据
} img_frame_protocol_t;

/**
 * @brief 图片帧数据解析
 * @param in_data               [in] csk裁剪后数据
 * @param in_data_len           [in] csk裁剪后数据长度
 * @param out_img_frame_msg     [out] 解析后数据信息
 * @return ** int           0 成功 非0失败
 */
int image_frame_data_parser(
		const uint8_t *in_data, uint32_t in_data_len, img_frame_protocol_t *out_img_frame_msg);

/**
 * @brief 图片帧数据组包
 * @param in_img_frame_msg		[in]  图像数据未经压缩的图像帧
 * @param out_data				[out] 图像数据经过压缩的图像帧
 * @param ou_data_len			[out] 图像数据经过压缩的图像帧数据长度
 * @param compress_fun			[in]  图像数据压缩函数
 * @param is_debug				[in]  Debug模式
 * @return ** uint8_t*
 */
int image_frame_data_build(
					const img_frame_protocol_t * const in_img_frame_msg,
					uint8_t **out_data,
					uint32_t *out_data_len,
					evs_rec_img_compress compress_fun,
					bool is_debug);
/**
 * @brief 内部JPEG压缩方法
 * @return int
 */
int inner_image_jpeg_compress_fun(
					const uint8_t *const image_data,
					int image_width,
					int image_height,
					int components,
					lisa_evs_color_space_e color_space,
					int quality,
					uint8_t **compress_data,
					uint32_t *compress_len);
#ifdef __cplusplus
}
#endif

#endif