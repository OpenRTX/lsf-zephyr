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
#define SEND_STACK_SIZE (5 * 1024)
#define RECV_STACK_SIZE (5 * 512)
#define WS_QUEUE_COUNT 150

#define TAG "RWS"

typedef struct {
	lisa_ws_data_type_e m_type;  // 0 txt 1 audio
	char *m_msg;
	int m_size;
	uint32_t flash_index;
} lisa_ws_msg_t;

static lisa_ws_t *s_lisa_ws = NULL;
static lisa_thread_t *s_pid_send = NULL;
static lisa_thread_t *s_recv_thread = NULL;
static lisa_mutex_t *s_mutex = NULL;
static lisa_semaphore_t *s_ws_sema = NULL;
static bool s_break_conn = false;
static bool s_connected = false;
static bool s_stop = false;

static lisa_queue_t *s_tx_msg_queue = NULL;
static lisa_queue_t *s_rx_msg_queue = NULL;

/**
 * @brief websocket 断连回调
 *
 */
static void _callback_ws_disconnect()
{
	if (s_lisa_ws) {
		lisa_ws_event_t ws_event;
		ws_event.what = LISA_WS_ON_DISCONNECTED;
		ws_event.header = NULL;
		ws_event.user = s_lisa_ws->user;
		s_lisa_ws->inter_on_event(&ws_event);
	}
}

/**
 * @brief  初始化websocket实例
 *
 * @param req
 * @return lisa_ws_t*
 */
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
	s_mutex = lisa_mutex_create();
	s_ws_sema = lisa_semaphore_create(2);
	s_tx_msg_queue = lisa_queue_create(WS_QUEUE_COUNT, "ws_tx_msg", sizeof(lisa_ws_msg_t));
	if (s_tx_msg_queue == NULL) {
		LISA_LOGE(TAG, "create tx queue failed!");
		goto ERR_TXQUE;
	}

	s_rx_msg_queue = lisa_queue_create(WS_QUEUE_COUNT, "ws_rx_msg", sizeof(lisa_ws_msg_t));
	if (s_rx_msg_queue == NULL) {
		LISA_LOGE(TAG, "create rx queue failed!");
		goto ERR_RXQUE;
	}

	s_lisa_ws = handle;
	return handle;

ERR_RXQUE:
	if (s_tx_msg_queue) {
		lisa_thread_t *temp_thread = s_tx_msg_queue;
		s_tx_msg_queue = NULL;
		lisa_queue_delete(temp_thread);
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
	while (!s_stop && lisa_queue_receive(s_rx_msg_queue, &chunk, sizeof(lisa_ws_msg_t),
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
	while (lisa_queue_receive(s_rx_msg_queue, &recv_chunk, sizeof(lisa_ws_msg_t), 0) == LISA_OK) {
		if (recv_chunk.m_msg != NULL) {
			lisa_mem_free(recv_chunk.m_msg);
		}
		memset(&recv_chunk, 0, sizeof(lisa_ws_msg_t));
	}

	if (s_recv_thread) {
		lisa_thread_t *temp_thread = s_recv_thread;
		s_recv_thread = NULL;
		lisa_thread_delete(temp_thread);
	}
}

static void _lisa_ws_cb(ze_websocket_client_handle_t *client, ze_websocket_event_id_t event_id,
		ze_websocket_event_data_t *data)
{
	switch (event_id) {
		case WEBSOCKET_EVENT_CONNECTED:
			LISA_LOGD(TAG, "WEBSOCKET_EVENT_CONNECTED");
			s_connected = true;
			lisa_semaphore_give(s_ws_sema);
			break;
		case WEBSOCKET_EVENT_DISCONNECTED:
			s_stop = true;
			s_connected = false;
			_callback_ws_disconnect();
			LISA_LOGD(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
			lisa_semaphore_give(s_ws_sema);
			break;
		case WEBSOCKET_EVENT_DATA:
			// LISA_LOGD(TAG, "recv data len %d\n", data->data_len);
			if (data->data_len <= 0) {
				return;
			}
			// LISA_LOGD(TAG, "received=%.*s", data->data_len, (char *)data->data_ptr);
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
			lisa_err_t st =
					lisa_queue_push(s_rx_msg_queue, (void *)&recv_chunk, sizeof(lisa_ws_msg_t), 0);
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

	lisa_mutex_lock(s_mutex, LISA_WAIT_FOREVER);

	s_break_conn = false;
	ze_websocket_client_config_t websocket_cfg = {
			.task_stack = SEND_STACK_SIZE,
			.task_prio = OS_PRIORITY_HIGH,
			.user_context = ins,
			.headers = "Origin: http://www.listenai.com\r\n",
			.disable_auto_reconnect = true,
	};
	char url[512] = {0};

	sprintf(url, "%s://%s:%s%s", ins->u_info.scheme, ins->u_info.host, ins->u_info.port,
			ins->u_info.path);
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
	if (!s_connected) {
		goto EXIT;
	}

	lisa_thread_attr_t thread_attr;
	thread_attr.name = "ws_recv_thread";
	thread_attr.stack_size = RECV_STACK_SIZE;
	thread_attr.priority = OS_PRIORITY_ABOVE_NORMAL;  // OS_PRIORITY_ABOVE_NORMAL
	s_recv_thread = lisa_thread_create(&thread_attr, _recv_thread, (void *)ins);
	if (s_recv_thread == NULL) {
		LISA_LOGE(TAG, "create recv thread failed");
		goto EXIT;
	}

	s_stop = false;
	lisa_ws_event_t ws_event;
	ws_event.what = LISA_WS_ON_CONNECTED;
	ws_event.header = NULL;
	ws_event.user = ins->user;
	ins->inter_on_event(&ws_event);

	uint64_t start;
	while (!s_stop) {
		static lisa_ws_msg_t chunk;
		memset(&chunk, 0, sizeof(lisa_ws_msg_t));
		lisa_err_t st = lisa_queue_receive(s_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 20);
		if (st == LISA_OK) {
			if (chunk.m_size > 0) {
				int ret = 0;
				if (chunk.m_type == LISA_WS_TEXT) {
					int send_ret =
							ze_websocket_client_send_text(client, chunk.m_msg, chunk.m_size, 100);
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
	if (s_recv_thread) {
		lisa_ws_msg_t recv_chunk;
		memset(&recv_chunk, 0, sizeof(lisa_ws_msg_t));
		recv_chunk.m_msg = NULL;
		recv_chunk.m_size = 0;
		lisa_queue_push(s_rx_msg_queue, (void *)&recv_chunk, sizeof(lisa_ws_msg_t), 0);
	}

	lisa_ws_msg_t chunk;
	memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	while (lisa_queue_receive(s_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0) == LISA_OK) {
		if (chunk.m_msg != NULL) {
			lisa_mem_free(chunk.m_msg);
		}
		memset(&chunk, 0, sizeof(lisa_ws_msg_t));
	}

EXIT:
	if (s_connected && client) {
		ze_websocket_client_stop(client);
	}
	if (client) {
		ze_websocket_client_destroy(client);
	}
	if (s_break_conn && s_connected) {
		_callback_ws_disconnect();
	}
	s_connected = false;
	lisa_mutex_unlock(s_mutex);
	if (s_pid_send) {
		lisa_thread_t *temp_thread = s_pid_send;
		s_pid_send = NULL;
		lisa_thread_delete(temp_thread);
	}
	return;
}

/**
 * @brief 进行websocket连接
 *
 * @param ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins)
{
	if (!ins) {
		return LISA_WS_COMMON_ERR;
	}
	if (s_connected || s_pid_send) {
		LISA_LOGE(TAG, "websocket already running");
		return LISA_WS_COMMON_ERR;
	}
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "ws_send_thread";
	thread_attr.stack_size = SEND_STACK_SIZE;
	thread_attr.priority = OS_PRIORITY_HIGH;  // OS_PRIORITY_HIGH
	s_pid_send = lisa_thread_create(&thread_attr, _send_thread, (void *)ins);
	if (s_pid_send == NULL) {
		LISA_LOGE(TAG, "thread_create failed");
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

/**
 * @brief 断开websocket连接
 *
 * @param ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins)
{
	if (!ins || s_stop) {
		return LISA_WS_COMMON_ERR;
	}
	s_break_conn = true;
	s_stop = true;
	lisa_mutex_lock(s_mutex, LISA_WAIT_FOREVER);
	lisa_mutex_unlock(s_mutex);
	lisa_thread_mdelay(10);
	return LISA_WS_OK;
}

/**
 * @brief 通过websocket发送文本
 *
 * @param ins
 * @param text
 * @return lisa_ws_err_e
 */
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
	lisa_err_t st = lisa_queue_push(s_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send txt failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

/**
 * @brief 通过websocket发送二进制流
 *
 * @param ins
 * @param buf
 * @param len
 * @return lisa_ws_err_e
 */
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
	lisa_err_t st = lisa_queue_push(s_tx_msg_queue, &chunk, sizeof(lisa_ws_msg_t), 0);
	if (st != LISA_OK) {
		lisa_mem_free(msg_copy);
		LISA_LOGE(TAG, "send binary to queue failed! ret: %d", st);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

/**
 * @brief 销毁websocket实例
 *
 * @param ins
 * @return lisa_ws_err_e
 */
lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins)
{
	if (ins) {
		lisa_ws_disconnect(ins);
		while (s_pid_send != NULL) {
			lisa_thread_mdelay(100);
		}
		while (s_recv_thread != NULL) {
			lisa_thread_mdelay(100);
		}
		lisa_queue_delete(s_rx_msg_queue);
		s_rx_msg_queue = NULL;
		lisa_queue_delete(s_tx_msg_queue);
		s_tx_msg_queue = NULL;
		lisa_mutex_delete(s_mutex);
		s_mutex = NULL;
		lisa_semaphore_delete(s_ws_sema);
		s_ws_sema = NULL;
		lisa_mem_free(ins);
		s_lisa_ws = NULL;
		return LISA_WS_OK;
	}
	return LISA_WS_COMMON_ERR;
}