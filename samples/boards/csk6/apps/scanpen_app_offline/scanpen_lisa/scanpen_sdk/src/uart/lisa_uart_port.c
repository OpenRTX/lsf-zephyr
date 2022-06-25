#include "lisa_uart_recv.h"
#include "lisa_uart_send.h"
#include <lisa_log.h>
#include "lisa_uart_port.h"
#include <lisa_ring_buffer.h>
#include <lisa_semaphore.h>
#include <lisa_thread.h>

#undef	TAG
#define TAG	"[PROT]"

#define PROT_SEND_QSIZE	(2)
#define PROT_RECV_SIZE	(1024)

typedef struct {
	void *addr;
	int size;
	bool mem_alloc;
} mem_item_t;

static lisa_semaphore_t *s_uart_data_sema = NULL;
static struct lisa_ring_buffer *s_recv_ring_buffer = NULL;


/**
 * @brief get uart receive data
 *
 * @param data
 * @param len
 * @param wait_ms
 * @return int
 */
int prot_read_data(uint8_t *data, unsigned int len, int wait_ms)
{
	int ret = lisa_ring_buffer_get(s_recv_ring_buffer, data, len);
	while (ret <= 0) {
		lisa_semaphore_take(s_uart_data_sema, LISA_WAIT_FOREVER);
		ret = lisa_ring_buffer_get(s_recv_ring_buffer, data, len);
	}
	return ret;
}

/**
 * @brief RX, from driver to ringbuffer
 *
 * @param data
 * @param len
 */
void prot_recv_data(char *data, uint32_t size)
{
	bool need_wake = false;
	if (lisa_ring_buffer_len(s_recv_ring_buffer) <= 0) {
		need_wake = true;
	}

	int ret = lisa_ring_buffer_put(s_recv_ring_buffer, data, size);
	if (need_wake) {
		lisa_semaphore_give(s_uart_data_sema);
	}
	if (ret != size) {
		LISA_LOGE(TAG, "write stream full \n");
	}
	lisa_thread_mdelay(10);
}

/**
 * @brief RX, from driver to ringbuffer
 *
 * @param data
 * @param len
 */

void prot_send_data(void *data, int size)
{
 // TODO: to driver

}

void prot_reset_fifo(void)
{

}

uint8_t prot_pack_check(void *dat, int len)
{
	uint8_t check = 0;
	while (len--)
		check += *(uint8_t *)dat++;
	return check;
}

void prot_uart_init(void)
{
	s_uart_data_sema = lisa_semaphore_create(2);
	#define RINGBUFFER_SIZ (4*1024)
	unsigned char *buffer = lisa_mem_alloc(RINGBUFFER_SIZ);
	s_recv_ring_buffer = lisa_ring_buffer_init(buffer, RINGBUFFER_SIZ);
}

void prot_send_ready(void)
{

}

#include <kernel.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <string.h>
#include <devicetree.h>
#include <logging/log.h>
#include <drivers/uart.h>

#define UART2		DT_NODELABEL(uart2)

uint8_t rx_buffer[100];

static void uart_rx_callback(const struct device *dev, void *user_data){
	LISA_LOGI(TAG, "#####uart_rx_callback##");
	/* 进入中断处理调用 */
    uart_irq_update(dev);					
	while (uart_irq_is_pending(dev)) {		/*查询是否有中断挂起*/
		if (uart_irq_rx_ready(dev)) {
			int recv_len = 100;
			recv_len = uart_fifo_read(dev, rx_buffer, recv_len);	/* 读取字符串*/
			LISA_LOGI(TAG, "devices %s, rx:%d", dev->name, recv_len);
			// prot_recv_data(rx_buffer, recv_len);
			uart_fifo_fill(dev, rx_buffer, recv_len);		/* 发送字符串*/
		}
	}
}

void prot_recv_task(void *arg)
{
	prot_uart_init();

	const struct device *uart2 = DEVICE_DT_GET(UART2);

	if(!device_is_ready(uart2)){
		LISA_LOGE(TAG, "device:%s is not ready!", uart2->name);
		return;
	}
	LISA_LOGI(TAG, "devices %s is ready", uart2->name);

	/* Define the application callback function */
	uart_irq_callback_user_data_set(uart2, uart_rx_callback, NULL);

	/* Enable rx interrupts */
	uart_irq_rx_enable(uart2);
	char tx_buffer[5] = {1, 2, 3, 4, 5};
	while(1){
		// semaphore block here
		// frame_recv_proc(arg);
		lisa_thread_mdelay(2000);
	}
}