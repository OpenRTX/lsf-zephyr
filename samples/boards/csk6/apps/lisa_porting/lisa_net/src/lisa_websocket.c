#include <lisa_websocket.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_mutex.h>
#include <lisa_queue.h>
#include <lisa_thread.h>
#include <lisa_typedef.h>
#include <lisa_time.h>
#include <ze_websocket_client.h>
#include <lisa_semaphore.h>

#define WS_SEND_RETRY_TXT_COUNT (10)
#define WS_SEND_RETRY_BIN_COUNT (5)
#define WS_SEND_RETRY_DELAY (50000)  // 20ms
#define SEND_STACK_SIZE (10 * 1024)
#define RECV_STACK_SIZE (8 * 512)
#define WS_QUEUE_COUNT 150

static K_THREAD_STACK_DEFINE(lisa_ws_send_stack_area, SEND_STACK_SIZE);
static K_THREAD_STACK_DEFINE(lisa_ws_recv_stack_area, RECV_STACK_SIZE);

#define TAG "RWS"

typedef struct {
	lisa_ws_data_type_e m_type;  // 0 txt 1 audio
	char *m_msg;
	int m_size;
	uint32_t flash_index;
} lisa_ws_msg_t;

static lisa_ws_t *g_lisa_ws = NULL;
static lisa_thread_t *g_pid_send = NULL;
static lisa_thread_t *g_recv_thread = NULL;
static lisa_mutex_t *g_mutex = NULL;
static lisa_semaphore_t *s_ws_sema = NULL;
static bool g_break_conn = false;
static bool g_connected = false;
static bool g_stop = false;

static lisa_queue_t *g_tx_msg_queue = NULL;
static lisa_queue_t *g_rx_msg_queue = NULL;

static void _callback_ws_disconnect()
{
	if (g_lisa_ws) {
		lisa_ws_event_t ws_event;
		ws_event.what = LISA_WS_ON_DISCONNECTED;
		ws_event.header = NULL;
		ws_event.user = g_lisa_ws->user;
		g_lisa_ws->inter_on_event(&ws_event);
	}
}

lisa_ws_t *lisa_ws_init(lisa_ws_request_t *req)
{
	lisa_ws_t *handle = (lisa_ws_t *)lisa_mem_calloc(1, sizeof(lisa_ws_t));
	if (handle == NULL) {
		LISA_LOGE(TAG, "handle create failed. ");
		return NULL;
	}

	LISA_LOGD(TAG, "port %s", req->port);
	LISA_LOGD(TAG, "scheme %s", req->scheme);
	LISA_LOGD(TAG, "host %s", req->host);
	LISA_LOGD(TAG, "path %s", req->path);

	strcpy((char *)handle->u_info.scheme, req->scheme);
	strcpy((char *)handle->u_info.host, req->host);
	strcpy((char *)handle->u_info.path, req->path);
	strcpy((char *)handle->u_info.port, req->port);
	handle->user = req->user;
	handle->inter_on_event = req->on_event;

	handle->inter_on_data = req->on_data;
	g_mutex = lisa_mutex_create();
	s_ws_sema = lisa_semaphore_create(2);
	g_tx_msg_queue = lisa_queue_create(WS_QUEUE_COUNT, "ws_tx_msg", sizeof(lisa_ws_msg_t));
	if (g_tx_msg_queue == NULL) {
		LISA_LOGE(TAG, "create tx queue failed!");
		goto ERR_TXQUE;
	}

	g_rx_msg_queue = lisa_queue_create(WS_QUEUE_COUNT, "ws_rx_msg", sizeof(lisa_ws_msg_t));
	if (g_rx_msg_queue == NULL) {
		LISA_LOGE(TAG, "create rx queue failed!");
		goto ERR_RXQUE;
	}

	g_lisa_ws = handle;
	return handle;

ERR_RXQUE:
	if (g_tx_msg_queue) {
		lisa_queue_delete(g_tx_msg_queue);
		g_tx_msg_queue = NULL;
	}
ERR_TXQUE:
	lisa_mem_free(handle);
	return handle;
}

static void _recv_thread(void *param)
{
	lisa_ws_t *ins = (lisa_ws_t *)param;
	lisa_ws_data_t ws_data;
	ws_data.type = LISA_WS_TEXT;
	ws_data.user = ins->user;
	static lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	while (!g_stop && lisa_queue_receive(g_rx_msg_queue, &chunk, sizeof(lisa_ws_msg_t),
							  LISA_WAIT_FOREVER) == LISA_OK) {
		ws_data.buf = chunk.m_msg;
		ws_data.len = chunk.m_size;
		ins->inter_on_data(&ws_data);
		if (chunk.m_msg) {
			lisa_mem_free(chunk.m_msg);
		}
		memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	}

	lisa_ws_msg_t recv_chunk;
	memset(&recv_chunk, 0, sizeof(lisa_ws_msg_t));
	while (lisa_queue_receive(g_rx_msg_queue, &recv_chunk, sizeof(lisa_ws_msg_t), 0) == LISA_OK) {
		if (recv_chunk.m_msg != NULL) {
			lisa_mem_free(recv_chunk.m_msg);
		}
		memset(&recv_chunk, 0, sizeof(lisa_ws_msg_t));
	}

	if (g_recv_thread) {
		lisa_thread_t *temp_thread = g_recv_thread;
		g_recv_thread = NULL;
		lisa_thread_delete(temp_thread);
	}
}

static void _lisa_ws_cb(ze_websocket_client_handle_t *client, ze_websocket_event_id_t event_id,
		ze_websocket_event_data_t *data)
{
	switch (event_id) {
		case WEBSOCKET_EVENT_CONNECTED:
			LISA_LOGD(TAG, "WEBSOCKET_EVENT_CONNECTED");
			g_connected = true;
			lisa_semaphore_give(s_ws_sema);
			break;
		case WEBSOCKET_EVENT_DISCONNECTED:
			g_stop = true;
			g_connected = false;
			_callback_ws_disconnect();
			LISA_LOGD(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
			lisa_semaphore_give(s_ws_sema);
			break;
		case WEBSOCKET_EVENT_DATA:
			LISA_LOGD(TAG, "recv data len %d\n", data->data_len);
			if (data->data_len <= 0) {
				return;
			}
			LISA_LOGD(TAG, "received=%.*s", data->data_len, (char *)data->data_ptr);
			unsigned char *msg_copy = (unsigned char *)lisa_mem_alloc(data->data_len);
			if (msg_copy == NULL) {
				LISA_LOGE(TAG, "recv txt malloc failed!");
				return;
			}
			memcpy(msg_copy, data->data_ptr, data->data_len);

			static lisa_ws_msg_t recv_chunk;
			memset(&recv_chunk, 0, sizeof(lisa_ws_msg_t));
			recv_chunk.m_msg = (char *)msg_copy;
			recv_chunk.m_size = data->data_len;
			lisa_err_t st = lisa_queue_push(g_rx_msg_queue, (void *)&recv_chunk, sizeof(lisa_ws_msg_t), 0);
			if (st != LISA_OK) {
				LISA_LOGE(TAG, "send recv msg failed! ret: %d", st);
				lisa_mem_free(msg_copy);
			}

			break;
		case WEBSOCKET_EVENT_ERROR:
			LISA_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
			break;
	}
}

static void _send_thread(void *param)
{
	lisa_ws_t *ins = (lisa_ws_t *)param;

	lisa_mutex_lock(g_mutex, LISA_WAIT_FOREVER);

	g_break_conn = false;
	ze_websocket_client_config_t websocket_cfg = {
			.task_stack = SEND_STACK_SIZE,
			.task_prio = OS_PRIORITY_HIGH,
			.user_context = ins,
			.headers = "Origin: http://www.listenai.com\r\n",
			.disable_auto_reconnect = true,
	};
	char url[512] = {0};

	sprintf(url, "%s://%s:%s%s", ins->u_info.scheme, ins->u_info.host, ins->u_info.port, ins->u_info.path);
	LISA_LOGD(TAG, "ws url = %s", url);
	websocket_cfg.uri = url;

	ze_websocket_client_handle_t client = ze_websocket_client_init(&websocket_cfg);
	if (client == NULL) {
		LISA_LOGE(TAG, "websocket client init failed");
		goto EXIT;
	}

	ze_websocket_register_events(client, _lisa_ws_cb);
	int start_ret = ze_websocket_client_start(client);
	if (start_ret != ZPORT_OK) {
		LISA_LOGE(TAG, "websocket start error: %d", start_ret);
		goto EXIT;
	}
	lisa_semaphore_take(s_ws_sema, LISA_WAIT_FOREVER);
	if (!g_connected) {
		goto EXIT;
	}
	
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "ws_recv_thread";
	thread_attr.stack_size = K_THREAD_STACK_SIZEOF(lisa_ws_recv_stack_area);
	thread_attr.stack = lisa_ws_recv_stack_area;
	thread_attr.priority = OS_PRIORITY_ABOVE_NORMAL;  // OS_PRIORITY_ABOVE_NORMAL
	g_recv_thread = lisa_thread_create(&thread_attr, _recv_thread, (void *)ins);
	if (g_recv_thread == NULL) {
		LISA_LOGE(TAG, "create recv thread failed");
		goto EXIT;
	}

	g_stop = false;
	lisa_ws_event_t ws_event;
	ws_event.what = LISA_WS_ON_CONNECTED;
	ws_event.header = NULL;
	ws_event.user = ins->user;
	ins->inter_on_event(&ws_event);

	uint64_t start;
	while (!g_stop) {
		static lisa_ws_msg_t chunk;
		memset(&chunk, 0, sizeof(lisa_ws_msg_t));
		lisa_err_t st = lisa_queue_receive(g_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 20);
		if (st == LISA_OK) {
			if (chunk.m_size > 0) {
				int ret = 0;
				if (chunk.m_type == LISA_WS_TEXT) {
					int send_ret = ze_websocket_client_send_text(client, chunk.m_msg, chunk.m_size, 100);
					if (send_ret <= 0) {
						LISA_LOGD(TAG, "send ret %d", send_ret);
					}
					
					if (chunk.m_msg != NULL) {
						lisa_mem_free(chunk.m_msg);
					}
				} else {
					start = lisa_os_get_ticks();
					ret = ze_websocket_client_send_bin(client, chunk.m_msg, chunk.m_size, 100);
					if (ret != chunk.m_size)
						LISA_LOGE(TAG, "send bin ret: %d cost: %ld\n", ret,
								lisa_os_get_ticks() - start);
					if (chunk.m_msg != NULL) {
						lisa_mem_free(chunk.m_msg);
					}
				}
			}
		}
	}
	if (g_recv_thread) {
		lisa_ws_msg_t recv_chunk;
		memset(&recv_chunk, 0, sizeof(lisa_ws_msg_t));
		recv_chunk.m_msg = NULL;
		recv_chunk.m_size = 0;
		lisa_queue_push(g_rx_msg_queue, (void *)&recv_chunk, sizeof(lisa_ws_msg_t), 0);
	}

	lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	while (lisa_queue_receive(g_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0) == LISA_OK) {
		if (chunk.m_msg != NULL) {
			lisa_mem_free(chunk.m_msg);
		}
		memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	}

EXIT:
	if (g_connected && client) {
		ze_websocket_client_stop(client);
	}
	if (client) {
		ze_websocket_client_destroy(client);
	}
	if (g_break_conn && g_connected) {
		_callback_ws_disconnect();
	}
	g_connected = false;
	lisa_mutex_unlock(g_mutex);
	if (g_pid_send) {
		lisa_thread_t *temp_thread = g_pid_send;
		g_pid_send = NULL;
		lisa_thread_delete(temp_thread);
	}
	return;
}

lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins)
{
	if (!ins) {
		return LISA_WS_COMMON_ERR;
	}
	if (g_connected || g_pid_send) {
		LISA_LOGE(TAG, "websocket already running");
		return LISA_WS_COMMON_ERR;
	}
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "ws_send_thread";
	thread_attr.stack_size = K_THREAD_STACK_SIZEOF(lisa_ws_send_stack_area);
	thread_attr.stack = lisa_ws_send_stack_area;
	thread_attr.priority = OS_PRIORITY_HIGH;  // OS_PRIORITY_HIGH
	g_pid_send = lisa_thread_create(&thread_attr, _send_thread, (void *)ins);
	if (g_pid_send == NULL) {
		LISA_LOGE(TAG, "thread_create failed");
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins)
{
	if (!ins || g_stop) {
		return LISA_WS_COMMON_ERR;
	}
	g_break_conn = true;
	g_stop = true;
	lisa_mutex_lock(g_mutex, LISA_WAIT_FOREVER);
	lisa_mutex_unlock(g_mutex);
	lisa_thread_mdelay(10);
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text)
{
	if (ins == NULL) {
		return LISA_WS_COMMON_ERR;
	}
	static lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	chunk.m_type = LISA_WS_TEXT;
	int len = strlen(text);
	char *msg_copy = lisa_mem_alloc(len + 1);
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "send txt malloc failed!");
		return LISA_WS_COMMON_ERR;
	}
	strcpy(msg_copy, text);
	chunk.m_msg = msg_copy;
	chunk.m_size = len;
	lisa_err_t st = lisa_queue_push(g_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send txt failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len)
{
	if (ins == NULL) {
		return LISA_WS_COMMON_ERR;
	}
	static lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));

	chunk.m_type = LISA_WS_BIN;
	char *msg_copy = lisa_mem_alloc(len);
	if (msg_copy == NULL) {
		LISA_LOGE(TAG, "send byte malloc failed!");
		return LISA_WS_COMMON_ERR;
	}
	memcpy(msg_copy, buf, len);
	chunk.m_msg = msg_copy;
	chunk.m_size = len;
	lisa_err_t st = lisa_queue_push(g_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send binary to queue failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins)
{
	if (ins) {
		lisa_ws_disconnect(ins);
		while (g_pid_send != NULL) {
			lisa_thread_mdelay(100);
		}
		while (g_recv_thread != NULL) {
			lisa_thread_mdelay(100);
		}
		lisa_queue_delete(g_rx_msg_queue);
		g_rx_msg_queue = NULL;
		lisa_queue_delete(g_tx_msg_queue);
		g_tx_msg_queue = NULL;
		lisa_mutex_delete(g_mutex);
		g_mutex = NULL;
		lisa_semaphore_delete(s_ws_sema);
		s_ws_sema = NULL;
		lisa_mem_free(ins);
		g_lisa_ws = NULL;
		return LISA_WS_OK;
	}
	return LISA_WS_COMMON_ERR;
}