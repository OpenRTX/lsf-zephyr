#ifndef __LISA_EVS_SDK_UART_RECV_H__
#define __LISA_EVS_SDK_UART_RECV_H__

#include "lisa_uart_typdef.h"
#include "lisa_uart_protocol.h"
#include <lisa_queue.h>

typedef struct lisa_uart_recv {
    lisa_queue_t* recv_queue;

} lisa_uart_recv_t;

lisa_uart_recv_t* lisa_uart_recv_create(lisa_uart_protocol_t* protocol, listen_algo_config_t config);
lisa_err_t lisa_uart_recv();


#endif