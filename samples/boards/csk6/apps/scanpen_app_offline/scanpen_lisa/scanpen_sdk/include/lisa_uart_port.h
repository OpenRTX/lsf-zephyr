#ifndef __LISA_EVS_UART_PORT_H__
#define __LISA_EVS_UART_PORT_H__

#include <zephyr.h>
#include <lisa_mem.h>
#include <lisa_time.h>

void prot_send_data(void *data, int size);
int prot_read_data(uint8_t *data, unsigned int len, int wait_ms);
void prot_reset_fifo(void);
uint8_t prot_pack_check(void *dat, int len);
void prot_send_ready(void);
void prot_uart_init(void);
void prot_recv_task(void *arg);
void prot_send_task(void *arg);

#endif // __LISA_EVS_UART_PORT_H__
