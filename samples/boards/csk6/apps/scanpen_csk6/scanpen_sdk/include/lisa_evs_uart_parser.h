#ifndef __LISA_EVS_SDK_UART_PARSER_H__
#define __LISA_EVS_SDK_UART_PARSER_H__

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "lisa_evs_error.h"
#include <lisa_typedef.h>
#include <lisa_err.h>

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
	e_scan_mode_reboot,
	e_scan_mode_max,
} scan_mode_e;

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
} work_status_e;

typedef struct lisa_evs_uart_cb
{
    unsigned int (*read_uart_data)(void *data, unsigned int len, int wait_ms);
    unsigned int (*write_uart_data)(void *data, unsigned int len);

} lisa_evs_uart_cb_t;

// @描  述:	算法引擎回调消息枚举
typedef enum {
	e_listen_algo_msg_soft_version = 12,
	e_listen_algo_scan_info = 13,
	e_listen_algo_tts_encode = 14,
} listen_algo_msg_e;

// @描  述:	语音引擎结果回调函数
// @参  数：[O]msg		消息类型，详见listen_algo_msg_e定义
//			[O]param1	整数类型参数，通常是长度或者子消息类型
//			[O]param2	指针类型参数，通常是JSON字符串类型（如果存在）
// @返回值：无
typedef void (*listen_algo_callback_t)(listen_algo_msg_e msg, int param1, uint8_t *param2, int param3);

// @描  述:	引擎配置结构体
typedef struct {
	listen_algo_callback_t algo_cb;  // 消息回调接口
} listen_algo_config_t;

typedef enum {
	version_query_fw_stitch          = 0x00,
    version_query_master             = 0x01,
    version_query_stitch             = 0x02,
    version_query_chipid             = 0x03,
    version_query_cutline            = 0x04,
    version_query_ocr                = 0x05,
    version_query_tts_engine         = 0x06,
    version_query_tts_pronunciation  = 0x07,
    version_query_all_version        = 0x10,
} all_version_query_e;

lisa_err_t lisa_evs_parser_uart_init(lisa_evs_uart_cb_t uart_cb, listen_algo_config_t config);

lisa_err_t lisa_evs_scan_tts_text(const uint8_t* const text, unsigned int len);

lisa_err_t lisa_evs_uart_set_scan_mode(scan_mode_e mode, int wait_ms);

lisa_err_t lisa_evs_uart_get_csk_version(all_version_query_e type);

lisa_err_t lisa_evs_set_tts_speed(uint8_t language, uint8_t speed);

lisa_err_t lisa_evs_set_tts_base_speed(uint8_t language, uint8_t speed);

lisa_err_t lisa_evs_set_multiline_timeout(uint8_t timeout);

lisa_err_t lisa_evs_set_scan_direction(uint8_t direction);

lisa_err_t lisa_evs_scan_tts_auth(uint8_t *code, uint32_t len);

/**
 * @brief  下发手动校准信息
 * @param  x_offset         x轴坐标[-112, 112],必须为16的倍数
 * @param  y_offset         y轴坐标(-140, 140),必须为偶数
 * @return lisa_err_t 
 */
lisa_err_t lisa_evs_uart_adjustment(int16_t x_offset, int16_t y_offset);

lisa_err_t lisa_evs_uart_frame_recv();

lisa_err_t lisa_evs_uart_frame_send();

lisa_err_t lisa_evs_parser_uart_deinit();

#endif //__LISA_EVS_SDK_UART_PARSER_H__
