#ifndef __LISA_UART_PROTOCOL_H__
#define __LISA_UART_PROTOCOL_H__

#include <lisa_err.h>
#include <lisa_typedef.h>
#include "lisa_uart_typdef.h"
#include "lisa_evs_uart_parser.h"

typedef void(*uart_frame_recv_cb)(frame_desc_u *head);

typedef struct lisa_uart_protocol {
    lisa_evs_uart_cb_t uart_cb;
    uart_frame_recv_cb frame_recv_cb;
} lisa_uart_protocol_t;


lisa_uart_protocol_t* lisa_uart_protocol_init(lisa_evs_uart_cb_t cb);
lisa_err_t lisa_uart_add_recv_frame_cb(uart_frame_recv_cb cb);
void lisa_uart_pack_send(void *dat, int len);
bool lisa_uart_frame_recv();

#endif 
