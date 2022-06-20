#include <lisa_websocket.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_mutex.h>
#include <lisa_queue.h>
#include <lisa_thread.h>
#include <lisa_typedef.h>
#include <lisa_time.h>
#include <ze_websocket_client.h>
#include "ze_transport_ws.h"
#include <lisa_semaphore.h>

#define WS_SEND_RETRY_TXT_COUNT (10)
#define WS_SEND_RETRY_BIN_COUNT (5)
#define WS_SEND_RETRY_DELAY (50000)  // 20ms
#define SEND_STACK_SIZE (10 * 1024)
#define RECV_STACK_SIZE (8 * 512)
#define WS_QUEUE_COUNT 150
#define	WS_SEND_DATA_TIMEOUT	100
#define WS_DEFAULT_BUFFER_SIZE	2048

#define TAG "lisa_ws"

typedef struct {
	lisa_ws_data_type_e m_type;  // 0 txt 1 audio
	char *m_msg;
	int m_size;
	uint32_t flash_index;
} lisa_ws_msg_t;

static lisa_ws_t *g_lisa_ws = NULL;
static ze_websocket_client_handle_t s_client = NULL;
static bool g_connected = false;
static bool g_stop = false;
static uint32_t remain_len = 0;
static bool is_recombination = false;
static uint8_t *recom_buf = NULL;

K_MUTEX_DEFINE(s_ws_conn_mutex);

static void _lisa_ws_cb(ze_websocket_client_handle_t *client, ze_websocket_event_id_t event_id,
		ze_websocket_event_data_t *data);

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

static void _callback_ws_on_connected(void)
{
	if (g_lisa_ws == NULL) {
		return;
	}
	lisa_ws_event_t ws_event;
	ws_event.what = LISA_WS_ON_CONNECTED;
	ws_event.header = NULL;
	ws_event.user = g_lisa_ws->user;
	g_lisa_ws->inter_on_event(&ws_event);
}

static void _callback_ws_on_data(const void* data, uint32_t data_len, lisa_ws_data_type_e type)
{
	if (g_lisa_ws == NULL) {
		return;
	}
	lisa_ws_data_t ws_data;
	ws_data.type = type;
	ws_data.user = g_lisa_ws->user;
	ws_data.buf = data;
	ws_data.len = data_len;
	g_lisa_ws->inter_on_data(&ws_data);
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

	ze_websocket_client_config_t websocket_cfg = {
			.task_stack = SEND_STACK_SIZE,
			.task_prio = OS_PRIORITY_HIGH,
			.user_context = handle,
			.headers = "Origin: http://www.listenai.com\r\n",
			.disable_auto_reconnect = false,
			.buffer_size = WS_DEFAULT_BUFFER_SIZE,
	};
	char url[512] = {0};
	sprintf(url, "%s://%s:%s%s", handle->u_info.scheme, handle->u_info.host, handle->u_info.port, handle->u_info.path);
	LISA_LOGD(TAG, "ws url = %s", url);
	websocket_cfg.uri = url;
	ze_websocket_client_handle_t client = ze_websocket_client_init(&websocket_cfg);
	if (client == NULL) {
		LISA_LOGE(TAG, "websocket client init failed");
		goto ERR_TXQUE;
	}

	ze_websocket_register_events(client, _lisa_ws_cb);

	g_lisa_ws = handle;
	s_client = client;
	return handle;

ERR_TXQUE:
	lisa_mem_free(handle);
	return NULL;
}

static void _lisa_ws_cb(ze_websocket_client_handle_t *client, ze_websocket_event_id_t event_id,
		ze_websocket_event_data_t *data)
{
	lisa_ws_data_type_e data_type = LISA_WS_DATA_MAX;
	switch (event_id) {
		case WEBSOCKET_EVENT_CONNECTED:
			LISA_LOGD(TAG, "WEBSOCKET_EVENT_CONNECTED");
			g_connected = true;
			k_mutex_unlock(&s_ws_conn_mutex);
			_callback_ws_on_connected();
			break;
		case WEBSOCKET_EVENT_DISCONNECTED:
			g_stop = true;
			g_connected = false;
			k_mutex_lock(&s_ws_conn_mutex, K_NO_WAIT);
			_callback_ws_disconnect();
			LISA_LOGD(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
			break;
		case WEBSOCKET_EVENT_DATA:
			if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
				data_type = LISA_WS_TEXT;
			} else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
				data_type = LISA_WS_BIN;
			} else if (data->op_code == WS_TRANSPORT_OPCODES_PING) {
				LISA_LOGD(TAG, "ws ping");
			} else if (data->op_code == WS_TRANSPORT_OPCODES_PONG) {
				LISA_LOGD(TAG, "ws pong");
			} else {
				LISA_LOGE(TAG, "opcode error, 0x%x", data->op_code);
				return;
			}
			LISA_LOGD(TAG, "recv data len %d  opcode: 0x%x\n", data->data_len, data->op_code);
			if (data->data_len <= 0) {
				return;
			}
			LISA_LOGD(TAG, "received=%.*s", data->data_len, (char *)data->data_ptr);

			if (is_recombination) {
				memcpy(recom_buf + (data->payload_len - remain_len), data->data_ptr, data->data_len);
				remain_len -= data->data_len;
				if (remain_len == 0) {
					is_recombination = false;
					printk("%s\n", recom_buf);
					_callback_ws_on_data(recom_buf, data->payload_len, data_type);
					lisa_mem_free(recom_buf);
					recom_buf = NULL;
				}
			} else {
				if (data->data_len == data->payload_len) {
					_callback_ws_on_data(data->data_ptr, data->data_len, data_type);
				} else {
					recom_buf = lisa_mem_alloc(data->payload_len);
					if (recom_buf == NULL) {
						LISA_LOGE(TAG, "no memory, recv data failed");
						break;
					}
					remain_len = data->payload_len;
					memcpy(recom_buf + (data->payload_len - remain_len), data->data_ptr, data->data_len);
					remain_len -= data->data_len;
					is_recombination = true;
				}
			}
			break;
		case WEBSOCKET_EVENT_ERROR:
			LISA_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
			break;

		default: break;
	}
}

lisa_ws_err_e lisa_ws_connect(lisa_ws_t *ins)
{
	if (!ins) {
		return LISA_WS_COMMON_ERR;
	}
	if (g_connected) {
		LISA_LOGE(TAG, "websocket already running");
		return LISA_WS_COMMON_ERR;
	}

	if (s_client == NULL) {
		return LISA_WS_COMMON_ERR;
	}
	int start_ret = ze_websocket_client_start(s_client);
	if (start_ret != ZPORT_OK) {
		LISA_LOGE(TAG, "websocket start error: %d", start_ret);
		return LISA_WS_COMMON_ERR;
	}
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_disconnect(lisa_ws_t *ins)
{
	if (!ins || g_stop) {
		return LISA_WS_COMMON_ERR;
	}
	g_stop = true;
	lisa_thread_mdelay(10);
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_send_text(lisa_ws_t *ins, const uint8_t *text)
{
	if (ins == NULL) {
		return LISA_WS_COMMON_ERR;
	}
	int ret = k_mutex_lock(&s_ws_conn_mutex, K_SECONDS(10));
	if (ret != 0) {
		return LISA_WS_DISCONECT;
	}
	int send_ret = ze_websocket_client_send_text(s_client, text, strlen(text), WS_SEND_DATA_TIMEOUT);
	if (send_ret < 0) {
		return LISA_WS_COMMON_ERR;
	}
	k_mutex_unlock(&s_ws_conn_mutex);
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_send_binary(lisa_ws_t *ins, const void *buf, uint32_t len)
{
	if (ins == NULL) {
		return LISA_WS_COMMON_ERR;
	}
	int ret = k_mutex_lock(&s_ws_conn_mutex, K_SECONDS(10));
	if (ret != 0) {
		return LISA_WS_DISCONECT;
	}
	int send_ret = ze_websocket_client_send_bin(s_client, buf, len, WS_SEND_DATA_TIMEOUT);
	if (send_ret < 0) {
		return LISA_WS_COMMON_ERR;
	}
	k_mutex_unlock(&s_ws_conn_mutex);
	return LISA_WS_OK;
}

lisa_ws_err_e lisa_ws_cleanup(lisa_ws_t *ins)
{
	if (ins) {
		lisa_ws_disconnect(ins);
		lisa_mem_free(ins);
		g_lisa_ws = NULL;
		return LISA_WS_OK;
	}
	return LISA_WS_COMMON_ERR;
}
