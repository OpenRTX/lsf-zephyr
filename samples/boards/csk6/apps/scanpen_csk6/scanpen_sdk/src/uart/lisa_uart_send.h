#ifndef __LISA_EVS_SDK_UART_SEND_H__
#define __LISA_EVS_SDK_UART_SEND_H__

#include <lisa_err.h>
#include <lisa_typedef.h>
#include "lisa_uart_typdef.h"

// @描  述:	语音算法引擎初始化接口
// @参  数：[I]config	配置参数结构体指针
//			[I]wait_ms	整数类型参数, 表示等待下位机返回结果的时间
// @返回值：错误码(0: 成功)
lisa_err_t lisa_uart_send_init(int wait_ms);

// @描  述:	语音算法逆初始化接口
// @参  数：[I]wait_ms	整数类型参数, 表示等待下位机返回结果的时间
// @返回值：错误码(0: 成功)
lisa_err_t lisa_uart_send_deinit(int wait_ms);

// @描  述:	开始运行算法引擎
// @参  数：[I]mode		运行模式，详见listen_algo_mode_e定义
//			[I]wait_ms	整数类型参数, 表示等待下位机返回结果的时间
// @返回值：错误码(0: 成功)
lisa_err_t lisa_uart_send_start(listen_algo_mode_e mode, int wait_ms);

// @描  述:	停止运行算法引擎
// @参  数：[I]wait_ms	整数类型参数, 表示等待下位机返回结果的时间
// @返回值：错误码(0: 成功)
// lisa_err_t lisa_uart_send_stop(int wait_ms);

lisa_err_t lisa_uart_send_heartbeat(void);

// lisa_err_t lisa_csk_all_version(void);

lisa_err_t lisa_uart_set_scan_mode(scan_mode_e mode, int wait_ms);

lisa_err_t lisa_uart_scan_tts_text(const uint8_t* const text, uint32_t len);

lisa_err_t lisa_uart_get_csk_version(all_version_query_e type);

lisa_err_t lisa_uart_set_scan_param(listen_scan_param_type_e type, uint16_t data);

lisa_err_t lisa_uart_scan_tts_auth(uint8_t *code, uint32_t len);

lisa_err_t lisa_uart_adjustment(int16_t x_offset, int16_t y_offset);

lisa_err_t listen_send_uart_frame();

#endif //
