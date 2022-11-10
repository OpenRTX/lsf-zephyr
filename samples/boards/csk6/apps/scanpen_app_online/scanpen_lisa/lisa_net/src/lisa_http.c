#include <lisa_http.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>
#include <ze_http_client.h>
#include <string.h>

#define TAG "hal_http"

#define RESP_BUF_SIZE (4096)
#define DOWNLOAD_SIZE (4096)

static char *s_http_response = NULL;
static uint32_t s_resp_len = 0;

typedef void *(*lisa_http_get_header)();

zport_err_t _lisa_download_zport_cb(ze_http_client_event_t *evt)
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
			if (!ze_http_client_is_chunked_response(evt->client)) {
				if (s_http_response == NULL || s_resp_len == 0) {
					s_http_response = lisa_mem_alloc(evt->data_len);
					memcpy(s_http_response, evt->data, evt->data_len);
				} else {
					s_http_response = lisa_mem_realloc(s_http_response, evt->data_len + s_resp_len);
					memcpy(s_http_response + s_resp_len, evt->data, evt->data_len);
				}
				s_resp_len += evt->data_len;
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
	const ze_http_client_config_t config = {
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
		char *next_str = NULL;
		next_str = strtok(header, "\r\n");
		char *header_list[10] = {0};
		int header_size = 0;
		while (next_str) {
			const int len = strlen(next_str);
			header_list[header_size] = lisa_mem_alloc(len + 1);
			memcpy(header_list[header_size], next_str, len);
			header_list[header_size][len] = '\0';
			header_size += 1;
			if (header_size >= 10) break;
			next_str = strtok(NULL, "\r\n");
		}

		for (int i = 0; i < header_size; i++) {
			char *temp_ptr = NULL;
			temp_ptr = strtok(header_list[i], ":");
			char *key = NULL;
			char *value = NULL;
			while (temp_ptr) {
				const int temp_len = strlen(temp_ptr);
				if (key == NULL) {
					key = lisa_mem_alloc(temp_len + 1);
					memcpy(key, temp_ptr, temp_len);
					key[temp_len] = '\0';
				} else if (value == NULL) {
					value = lisa_mem_alloc(temp_len + 1);
					memcpy(value, temp_ptr, temp_len);
					value[temp_len] = '\0';
				}
				temp_ptr = strtok(NULL, ":");
			}
			if (key && value) {
				ze_http_client_set_header(client, key, value);
			}
			if (key) lisa_mem_free(key);
			if (value) lisa_mem_free(value);
			lisa_mem_free(header_list[i]);
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
		if (s_http_response && s_resp_len) {
			s_http_response = lisa_mem_realloc(s_http_response, s_resp_len + 1);
			s_http_response[s_resp_len] = '\0';
			if (ins && ins->inter_on_data) {
				lisa_http_data_t data;
				data.buf = s_http_response;
				data.len = s_resp_len;
				data.user = ins->user;
				ins->inter_on_data(&data);
			}
		}
	} else {
		LISA_LOGE(TAG, "HTTP GET request failed: %x %s line: %d", err, __func__, __LINE__);
		ret = -1;
	}
	if (s_http_response) {
		lisa_mem_free(s_http_response);
		s_http_response = NULL;
		s_resp_len = 0;
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

	const ze_http_client_config_t config = {
			.event_handler = _lisa_download_zport_cb,
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
		char *next_str = NULL;
		next_str = strtok(header, "\r\n");
		char *header_list[10] = {0};
		int header_size = 0;
		while (next_str) {
			const int len = strlen(next_str);
			header_list[header_size] = lisa_mem_alloc(len + 1);
			memcpy(header_list[header_size], next_str, len);
			header_list[header_size][len] = '\0';
			header_size += 1;
			if (header_size >= 10) break;
			next_str = strtok(NULL, "\r\n");
		}

		for (int i = 0; i < header_size; i++) {
			char *temp_ptr = NULL;
			temp_ptr = strtok(header_list[i], ":");
			char *key = NULL;
			char *value = NULL;
			while (temp_ptr) {
				const int temp_len = strlen(temp_ptr);
				if (key == NULL) {
					key = lisa_mem_alloc(temp_len + 1);
					memcpy(key, temp_ptr, temp_len);
					key[temp_len] = '\0';
				} else if (value == NULL) {
					value = lisa_mem_alloc(temp_len + 1);
					memcpy(value, temp_ptr, temp_len);
					value[temp_len] = '\0';
				}
				temp_ptr = strtok(NULL, ":");
			}
			if (key && value) {
				ze_http_client_set_header(client, key, value);
			}
			if (key) lisa_mem_free(key);
			if (value) lisa_mem_free(value);
			lisa_mem_free(header_list[i]);
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

lisa_http_err_e lisa_http_cleanup(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_OK;
	if (ins) {
		lisa_mem_free(ins);
		return http_ret;
	}
	return http_ret;
};
