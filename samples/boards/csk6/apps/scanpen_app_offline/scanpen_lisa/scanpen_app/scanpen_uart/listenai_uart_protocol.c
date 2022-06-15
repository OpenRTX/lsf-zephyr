#include "listenai_uart_protocol.h"


static lisa_thread_t *g_uart_task_protocol_algo_proc_thread = NULL;
static lisa_thread_t *g_uart_task_protocol_algo_thread = NULL;

static struct lisa_ring_buffer *g_uart_simulation_buffer = NULL;

static lisa_semaphore_t *g_uart_simulation_sema = NULL;

#define TAG "listenai_uart_protocol"

static void
algo_callback(listen_algo_msg_e msg, int param1, char *param2, int param3)
{
	switch (msg) {
		case e_listen_algo_msg_soft_version: {
			LISA_LOGI(TAG, "type=%d:%d.%d.%d\n", param1, param2[0], param2[1], param2[2]);
		} break;
		case e_listen_algo_scan_info: {
			char *status = param2;
			LISA_LOGI(TAG, "scan work status=%d\n", status[0]);
		} break;
		default:
			break;
	}
}

void print_hex(unsigned char *in, unsigned int length)
{
    int i=0;
    int index=0;
    // LISA_LOGI(TAG, "[index]:%d\n", g_index);
    for (int i=0;i< (int)(length+1)/16 + 1; i++)
    {
        if (i==0)
        {
            printk("[00000000] ");
            for (int j=0;j< 16; j++)
            {
                printk("0x%02d ", j);
            }
            printk("\n");
        }
       printk("[%08x] ", i * 16);
       index = (length - i*16) < 16? (length - i*16):16;
       for (int j=0;j< index; j++)
       {
       	   printk("0x%02x ", in[i*16+j]);
       }
       printk("\n");
    }
}


void _send_uart_simulation_once(struct lisa_ring_buffer *ring_buffer, lisa_uart_msg_t* data)
{
	bool need_wake = false;

	if (lisa_ring_buffer_len(ring_buffer) <= 0) {
		
		need_wake = true;
	}
	int ret = lisa_ring_buffer_put(ring_buffer, data->data, data->size);

	if (need_wake) lisa_semaphore_give(g_uart_simulation_sema);
	if (ret != data->size) {
		LISA_LOGI(TAG, "write stream full \n");
	}

	lisa_thread_mdelay(10);
}

void send_uart_simulation_once(lisa_uart_msg_t* data)
{
	return _send_uart_simulation_once(g_uart_simulation_buffer, data);
}

void send_uart_scan_start()
{
	uint8_t scan_start[] = { 0x58, 0x46, 0x0c, 0x00, 0x00, 0x56, 0xff, 0x00, 0x50, 0x01, 0x00, 0xb0 };
	lisa_uart_msg_t recv_image;

	recv_image.data = scan_start;
	recv_image.size = sizeof(scan_start)/sizeof(scan_start[0]);

	send_uart_simulation_once(&recv_image);
}

void send_uart_scan_end()
{
	uint8_t scan_end[] = { 0x58, 0x46, 0x0c, 0x00, 0x00, 0x56, 0xff, 0x00, 0x50, 0x00, 0x00, 0xb1 };
	lisa_uart_msg_t recv_image;

	recv_image.data = scan_end;
	recv_image.size = sizeof(scan_end)/sizeof(scan_end[0]);

	send_uart_simulation_once(&recv_image);
}

void send_uart_scan_continue()
{
	uint8_t scan_end[] = { 0x58, 0x46,  0x0c, 0x00, 0x00, 0x56, 0xff, 0x00, 0x50, 0x02, 0x00, 0xb1 };
	lisa_uart_msg_t recv_image;

	recv_image.data = scan_end;
	recv_image.size = sizeof(scan_end)/sizeof(scan_end[0]);

	send_uart_simulation_once(&recv_image);
}


int recv_uart_simulation_once(struct lisa_ring_buffer *ring_buffer, uint8_t *data, unsigned int len, int wait_ms )
{
	int ret = lisa_ring_buffer_get(ring_buffer, data, len);
	while (ret <= 0)
	{
		lisa_semaphore_take(g_uart_simulation_sema, LISA_WAIT_FOREVER);
		ret = lisa_ring_buffer_get(ring_buffer, data, len);
	}
	return ret;
}


unsigned int read_uart_data_cb(void *data, unsigned int len, int wait_ms)
{
	// read_data(UART_COM_ALGO, data, len, wait_ms);
	return recv_uart_simulation_once(g_uart_simulation_buffer, data, len, wait_ms);
}

unsigned int write_uart_data_cb(void *data, unsigned int len)
{
	// uart_send(UART_COM_ALGO, data, len);
	// print_hex(data, len);
	return len;
}

void
uart_cb_init()
{
	listen_algo_config_t config = {.algo_cb = algo_callback};
	lisa_evs_uart_cb_t uart_cb = {.read_uart_data = read_uart_data_cb, .write_uart_data = write_uart_data_cb};
	lisa_evs_parser_uart_init(uart_cb, config);
}

void
uart_cb_deinit()
{
	lisa_evs_parser_uart_deinit();
}

void
uart_task_protocol_algo_thread(void *argv)
{
	LISA_LOGI(TAG, "uart_task_protocol_algo_thread[in]\n");
	while (1) {
		lisa_evs_uart_frame_recv();
	}
	LISA_LOGI(TAG, "uart_task_protocol_algo_thread[out]\n");
}

void uart_task_protocol_algo_proc_thread(void *argv)
{
	LISA_LOGI(TAG, "uart_task_protocol_algo_proc_thread[in]\n");
	while (1) {
		lisa_evs_uart_frame_send();
	}
	LISA_LOGI(TAG, "uart_task_protocol_algo_proc_thread[out]\n");
}

void create_uart_task_protocol_algo_proc_thread()
{
	//1. spi_task_protocol_thread
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "uart_task_protocol_algo_proc_thread";
	thread_attr.stack_size = 5 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL; // OS_PRIORITY_NORMAL

	g_uart_task_protocol_algo_proc_thread = lisa_thread_create(&thread_attr, uart_task_protocol_algo_proc_thread, NULL);
	if (g_uart_task_protocol_algo_proc_thread == NULL) {
		LISA_LOGE(TAG, "thread_create: g_uart_task_protocol_algo_proc_thread failed");
		return;
	
	}
}

void create_uart_protocol_algo_thread()
{
	//1. spi_task_protocol_thread
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "uart_task_protocol_algo_thread";
	thread_attr.stack_size = 5 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL; // OS_PRIORITY_NORMAL

	g_uart_task_protocol_algo_thread = lisa_thread_create(&thread_attr, uart_task_protocol_algo_thread, NULL);
	if (g_uart_task_protocol_algo_thread == NULL) {
		LISA_LOGE(TAG, "thread_create: uart_task_protocol_algo_thread failed");
		return;
	}	
}


void start_uart_app()
{
	if (!g_uart_simulation_buffer) {
		unsigned char* buffer = lisa_mem_alloc(64 * 1024);
		g_uart_simulation_buffer = lisa_ring_buffer_init(buffer, 64 * 1024);
		if (g_uart_simulation_buffer == NULL)
		{
			LISA_LOGE(TAG, "malloc error\n");
		}
		g_uart_simulation_sema = lisa_semaphore_create(1);
		uart_cb_init();
		create_uart_task_protocol_algo_proc_thread();
		create_uart_protocol_algo_thread();
	}
	
	// lisa_ring_buffer_free(g_uart_simulation_buffer);

}
