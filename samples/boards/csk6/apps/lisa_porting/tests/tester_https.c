#include "tester_lisa.h"
#include <lisa_http.h>
#include <lisa_log.h>
#include <lisa_mem.h>

#define TAG "http_test"
#define HTTP_POST_URL ("http://www.rt-thread.com/service/echo")
#define HTTP_GET_URL ("http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/test/test.txt")
#define HTTP_DOWNLOAD_URL                                                                   \
	("http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/test/scanpen/ota_test/" \
	 "flashboot.bin")
#define HTTP_TEST_HEAD ("Content-Type:application/x-www-form-urlencoded;charset=utf-8")
#define BODY                                                                                    \
	"client_id=ad9f3c5d-38fd-4383-8d8d-8d460590de22&scope=user_ivs_all&scope_data={\"user_ivs_" \
	"all\":{\"device_id\":\"12345\"}}"
#define HTTP_TIME_OUT (10 * 1000)
unit_test_result_t g_result = LISA_TEST_PASS;

static void *_lisa_evs_request_device_code_heads(void)
{
	return HTTP_TEST_HEAD;
}

static uint32_t s_total_len = 0;

static void http_data_cb(lisa_http_data_t *data)
{
	if (!data || !data->buf || data->len <= 0) {
		g_result = LISA_TEST_FAILED;
	} else {
		s_total_len += data->len;
		LISA_LOGE(TAG, "http response : len %d", s_total_len);
	}
}

static void http_event_cb(lisa_http_event_t *event) {}

static unit_test_result_t http_test_method(lisa_http_method_e method)
{
	lisa_http_request_t *http_param =
			(lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	http_param->method = method;
	if (method == LISA_HTTP_GET) {
		http_param->url = HTTP_GET_URL;
	} else if (method == LISA_HTTP_POST) {
		http_param->url = HTTP_POST_URL;
	}
	http_param->headers = (void *)_lisa_evs_request_device_code_heads;
	if (method == LISA_HTTP_GET) {
		http_param->body = NULL;
		http_param->body_len = 0;
	} else if (method == LISA_HTTP_POST) {
		http_param->body = BODY;
		http_param->body_len = strlen(BODY);
	}
	http_param->timeout_ms = HTTP_TIME_OUT;
	http_param->on_data = http_data_cb;
	http_param->on_event = http_event_cb;
	http_param->user = NULL;

	lisa_http_t *http_test = lisa_http_init(http_param);
	if (!http_test) {
		LISA_LOGE(TAG, "lisa_http_init failed");
		g_result = LISA_TEST_FAILED;
	}

	if (lisa_http_perform(http_test) != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "lisa_http_perform failed");
		g_result = LISA_TEST_FAILED;
	}

	if (http_test) {
		lisa_http_cleanup(http_test);
	}

	if (http_param) {
		lisa_mem_free(http_param);
	}
	return g_result;
}

static void http_test_download(lisa_http_method_e method)
{
	lisa_http_request_t *http_param =
			(lisa_http_request_t *)lisa_mem_calloc(1, sizeof(lisa_http_request_t));
	http_param->method = method;
	http_param->url = HTTP_DOWNLOAD_URL;

	http_param->headers = (void *)_lisa_evs_request_device_code_heads;
	if (method == LISA_HTTP_GET) {
		http_param->body = NULL;
		http_param->body_len = 0;
	} else if (method == LISA_HTTP_POST) {
		http_param->body = BODY;
		http_param->body_len = strlen(BODY);
	}
	http_param->timeout_ms = HTTP_TIME_OUT;
	http_param->on_data = http_data_cb;
	http_param->on_event = http_event_cb;
	http_param->user = NULL;

	lisa_http_t *http_test = lisa_http_init(http_param);
	if (!http_test) {
		LISA_LOGE(TAG, "lisa_http_init failed");
		g_result = LISA_TEST_FAILED;
	}

	if (lisa_http_download(http_test) != LISA_HTTP_OK) {
		LISA_LOGE(TAG, "lisa_http_perform failed");
		g_result = LISA_TEST_FAILED;
	}

	if (http_test) {
		lisa_http_cleanup(http_test);
	}

	if (http_param) {
		lisa_mem_free(http_param);
	}
	return g_result;
}

unit_test_result_t lisa_https_test()
{
	// http post
	http_test_method(LISA_HTTP_POST);
	s_total_len = 0;
	http_test_method(LISA_HTTP_GET);
	s_total_len = 0;
	// http_test_download(LISA_HTTP_GET);

	return g_result;
}
