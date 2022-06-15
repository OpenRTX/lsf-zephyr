
#ifndef __UART_PROTOCOL_H__
#define __UART_PROTOCOL_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "lisa_evs_error.h"
#include <lisa_typedef.h>
#include <lisa_err.h>
#include <lisa_thread.h>
#include <lisa_mem.h>
#include <lisa_semaphore.h>
#include <lisa_log.h>
#include "lisa_ring_buffer.h"
#include "lisa_evs_uart_parser.h"

// @描  述:	语音引擎结果回调函数
// @参  数：[O]msg		消息类型，详见listen_algo_msg_e定义
//			[O]param1	整数类型参数，通常是长度或者子消息类型
//			[O]param2	指针类型参数，通常是JSON字符串类型（如果存在）
// @返回值：无
typedef void (*listen_algo_callback_t)(listen_algo_msg_e msg, int param1, uint8_t *param2, int param3);

lisa_err_t lisa_evs_parser_uart_init(lisa_evs_uart_cb_t uart_cb, listen_algo_config_t config);
lisa_err_t lisa_evs_parser_uart_deinit();


typedef struct{
    unsigned char *data;
	int size;
} lisa_uart_msg_t;

void start_uart_app();
void send_uart_scan_start();
void send_uart_scan_end();

#endif //__UART_PROTOCOL_H__
