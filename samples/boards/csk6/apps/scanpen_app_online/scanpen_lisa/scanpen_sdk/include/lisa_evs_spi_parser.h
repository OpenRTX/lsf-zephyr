#ifndef __LISA_EVS_SDK_SPI_PARSER_H__
#define __LISA_EVS_SDK_SPI_PARSER_H__

#include <stdbool.h>
#include <stdint.h>
#include <lisa_err.h>
#include "lisa_evs_error.h"

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
		uint16_t checksum;
		uint8_t *data;  //数据
} img_frame_t;

typedef struct {
		uint16_t tag; //标记
		uint8_t fmt; //FMT
		uint8_t channel; //通道
		uint8_t sample; //采样率
		uint8_t depth; //
		uint32_t len; // 数据长度
		uint16_t checksum; 
		uint8_t *data;
} tts_frame_t;

typedef struct {
		uint16_t tag; //标记
		uint32_t len; // 数据长度
		uint16_t checksum; 
		uint8_t *data;
} ocr_frame_t;

typedef enum {
	LISA_IMAGE_FRAME_NONE,
	LISA_IMAGE_FRAME_START,
	LISA_IMAGE_FRAME_CONTINUE,
	LISA_IMAGE_FRAME_END,
} lisa_image_transfer_status_e;


/**
* @brief 			读取spi数据回调，同步读取
* @param data		将spi数据拷贝到此data中
* @param len		需要读取的数据长度
* @param wait_ms	等待时间
* @return int		实际读取到的长度
*/
typedef	int (*read_spi_data)(uint8_t *data, unsigned int len, int wait_ms);

/**
 * @brief 			图片数据回调
 * @param data		图片数据
 * @param len		数据长度
 * @param statue 	
 */
typedef void (*image_data_callback)(uint8_t *data, int len, lisa_image_transfer_status_e status);
typedef void (*tts_data_callback)(tts_frame_t* data);
typedef void (*ocr_data_callback)(ocr_frame_t* data);

int lisa_evs_register_image_callback(image_data_callback image_cb);

/**
 * @brief  设置tts合成音频回调
 * @param  tts_cb      tts合成音频回调
 * @return int 
 */
int lisa_evs_register_tts_callback(tts_data_callback tts_cb);

/**
 * @brief  设置ocr结果回调
 * @param  ocr_cb     ocr结果回调
 * @return int 
 */
int lisa_evs_register_ocr_callback(ocr_data_callback ocr_cb);

/**
 * @brief  spi数据解析，循环调用
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_parser_spi_data();

/**
 * @brief  spi解析初始化，必须实现
 * @param  read_cb		spi读取数据回调
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_parser_spi_init(read_spi_data read_cb);

#endif //__LISA_EVS_SDK_SPI_PARSER_H__
