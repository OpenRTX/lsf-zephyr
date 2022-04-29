#include <lisa_http.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>
#include <ze_http_client.h>
#include <string.h>

#define TAG "hal_http"

#define RESP_BUF_SIZE (4096)
#define DOWNLOAD_SIZE (4096)

typedef void *(*lisa_http_get_header)();

zport_err_t _lisa_http_zport_cb(ze_http_client_event_t *evt)
{
	lisa_http_t *ins = (lisa_http_t *)evt->user_data;
	switch (evt->event_id) {
		case HTTP_EVENT_ERROR:
			// LISA_LOGD(TAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			// LISA_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			// LISA_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			// LISA_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
			// evt->header_value);
			break;
		case HTTP_EVENT_ON_DATA:
			LISA_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			if (!ze_http_client_is_chunked_response(evt->client)) {
				// Write out data
				if (ins && ins->inter_on_data) {
					lisa_http_data_t data;
					data.buf = evt->data;
					data.len = evt->data_len;
					data.user = ins->user;
					ins->inter_on_data(&data);
				}
			}

			break;
		case HTTP_EVENT_ON_FINISH:
			// LISA_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
			break;
		case HTTP_EVENT_DISCONNECTED:
			// LISA_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
			break;
	}
	return ZPORT_OK;
}

zport_err_t _lisa_download_zport_cb(ze_http_client_event_t *evt)
{
	return ZPORT_OK;
}

lisa_http_t *lisa_http_init(lisa_http_request_t *req)
{
	int ret = 0;
	lisa_http_t *http_session = NULL;

	http_session = (lisa_http_t *)lisa_mem_calloc(1, sizeof(lisa_http_t));
	if (http_session == NULL) {
		LISA_LOGE(TAG, "no memory for lisa_http_t.\n");
		return NULL;
	}

	strcpy(http_session->url, req->url);
	if (req->body_len > 0) {
		memcpy(http_session->body, req->body, req->body_len);
	}
	http_session->header_addr = req->headers;
	http_session->body_len = req->body_len;
	http_session->method = req->method;
	http_session->timeout_ms = req->timeout_ms;
	http_session->status_code = 0;
	http_session->inter_on_data = req->on_data;
	http_session->user = req->user;

	return http_session;
}

lisa_http_err_e lisa_http_perform(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_COMMON_ERR;
	lisa_http_data_t http_data;
	lisa_http_event_t http_event;
	int ret = 0;

	ze_http_client_config_t config = {
			.event_handler = _lisa_http_zport_cb,
			.url = ins->url,
			.user_data = ins,
			.timeout_ms = ins->timeout_ms,
	};

	ze_http_client_handle_t client = ze_http_client_init(&config);
	if (client == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		return LISA_FAIL;
	}

	lisa_http_get_header header_cb = (lisa_http_get_header)ins->header_addr;
	char *header = header_cb();
	if (header) {
		char header_item[512];
		char header_next[512];
		char header_temp[512];
		int32_t spl_ret = sscanf(header, "%[^\r\n]\r\n%s", header_item, header_next);
		while (spl_ret > 0) {
			char key[128];
			char value[256];
			int32_t result = sscanf(header_item, "%[^:]:%s", key, value);
			if (result == 2) {
				ze_http_client_set_header(client, key, value);
			}
			memset(header_item, 0, 512);
			if (spl_ret == 2) {
				spl_ret = sscanf(header_next, "%[^\r\n]\r\n%s", header_item, header_temp);
				strcpy(header_next, header_temp);
			} else {
				break;
			}
		}
	}

	if (ins->method == LISA_HTTP_GET) {
		ze_http_client_set_method(client, HTTP_METHOD_GET);
	} else if (ins->method == LISA_HTTP_POST) {
		ze_http_client_set_method(client, HTTP_METHOD_POST);
		ze_http_client_set_post_field(client, ins->body, ins->body_len);
	}

	zport_err_t err = ze_http_client_perform(client);
	if (err == ZPORT_OK) {
		LISA_LOGD(TAG, "HTTP GET Status = %d, content_length = %ld",
				ze_http_client_get_status_code(client), ze_http_client_get_content_length(client));
	} else {
		LISA_LOGE(TAG, "HTTP GET request failed: %x %s line: %d", err, __func__, __LINE__);
		ret = -1;
	}

	ze_http_client_cleanup(client);

	return (ret == 0 ? LISA_HTTP_OK : LISA_HTTP_COMMON_ERR);
}

lisa_http_err_e lisa_http_download(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_COMMON_ERR;
	lisa_http_data_t http_data;
	lisa_http_event_t http_event;
	int ret = 0;

	ze_http_client_config_t config = {
			.event_handler = _lisa_http_zport_cb,
			.url = ins->url,
			.user_data = ins,
			.timeout_ms = ins->timeout_ms,
	};

	ze_http_client_handle_t client = ze_http_client_init(&config);
	if (client == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		return LISA_FAIL;
	}

	lisa_http_get_header header_cb = (lisa_http_get_header)ins->header_addr;
	char *header = header_cb();
	if (header) {
		char header_item[512];
		char header_next[512];
		char header_temp[512];
		int32_t spl_ret = sscanf(header, "%[^\r\n]\r\n%s", header_item, header_next);
		while (spl_ret > 0) {
			char key[128];
			char value[256];
			int32_t result = sscanf(header_item, "%[^:]:%s", key, value);
			if (result == 2) {
				ze_http_client_set_header(client, key, value);
			}
			memset(header_item, 0, 512);
			if (spl_ret == 2) {
				spl_ret = sscanf(header_next, "%[^\r\n]\r\n%s", header_item, header_temp);
				strcpy(header_next, header_temp);
			} else {
				break;
			}
		}
	}

	if (ins->method == LISA_HTTP_GET) {
		ze_http_client_set_method(client, HTTP_METHOD_GET);
	} else if (ins->method == LISA_HTTP_POST) {
		ze_http_client_set_method(client, HTTP_METHOD_POST);
		ze_http_client_set_post_field(client, ins->body, ins->body_len);
	}

	zport_err_t err = ze_http_client_perform(client);
	if (err == ZPORT_OK) {
		LISA_LOGD(TAG, "HTTP GET Status = %d, content_length = %ld",
				ze_http_client_get_status_code(client), ze_http_client_get_content_length(client));
	} else {
		LISA_LOGE(TAG, "HTTP GET request failed: %x %s line: %d", err, __func__, __LINE__);
		ret = -1;
	}

	ze_http_client_cleanup(client);

	return (ret == 0 ? LISA_HTTP_OK : LISA_HTTP_COMMON_ERR);
}

lisa_http_err_e lisa_http_write(lisa_http_t *ins, const void *buffer, size_t length)
{
	lisa_http_err_e http_ret = LISA_HTTP_OK;
	return http_ret;
}

lisa_http_err_e lisa_http_cancel(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_OK;
	return http_ret;
}

lisa_http_err_e lisa_http_cleanup(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_OK;
	if (ins) {
		lisa_mem_free(ins);
		return http_ret;
	}
	return http_ret;
};
