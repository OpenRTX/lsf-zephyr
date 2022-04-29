/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tester_lisa.h"
#include <lisa_websocket.h>
#include <lisa_thread.h>
#include <lisa_log.h>
#include <lisa_time.h>

#define TAG "websocket_test"

#define WS_TEST_SCHEME "ws"
#define WS_TEST_HOST_1 "82.157.123.54"
#define WS_TEST_HOST_2 "ivs.iflyos.cn"
#define WS_TEST_PORT_1 "9010"
#define WS_TEST_PORT_2 "80"
#define WS_TEST_PATH_1 "/ajaxchattest"
#define WS_TEST_PATH_2 "/embedded/v1?device_id=12345&token=0000000000000000"
#define WS_SEND_RETRY_DELAY (10000)
#define WS_SEND_RETRY_SLEEP_TIME (100)
#define WS_TEST_CONTENT "lisa_test"

static char g_ws_test_binary[10] = {0x6c, 0x69, 0x73, 0x61, 0x5f, 0x74, 0x65, 0x73, 0x74, 0x00};
static unit_test_result_t g_result = LISA_TEST_PASS;
static lisa_ws_event_e g_event = LISA_WS_ON_DISCONNECTED;

static uint32_t g_test_step = 0;
static uint32_t g_test_step2 = 0;

static void get_ws_data_cb_2(lisa_ws_data_t *data)
{
    
}

static void get_ws_event_cb_2(lisa_ws_event_t *event)
{
    if (event->what == LISA_WS_ON_CONNECTED) {
        LISA_LOGE(TAG, "lisa ws connect");
        g_test_step2++;
	} else if (event->what == LISA_WS_ON_DISCONNECTED) {
        LISA_LOGE(TAG, "lisa ws disconnect");
		g_test_step2++;
	} else {
		LISA_LOGE(TAG, "event %d", event->what);
	}
}

static void get_ws_data_cb(lisa_ws_data_t *data)
{
    if (g_test_step == 0) {
        if (data->buf && strncmp(WS_TEST_CONTENT, data->buf, strlen(WS_TEST_CONTENT)) == 0) {
            g_test_step = 1;
            return;
        } else {
            g_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "ws send and recv test failed 1");
        }
    } else if (g_test_step == 1) {
        char* temp = (char*)data->buf;
        for (int i = 0; i < 10; i++) {
            if (g_ws_test_binary[i] != temp[i]) {
                g_test_step = -1;
                g_result = LISA_TEST_FAILED;
                LISA_LOGE(TAG, "ws send and recv test failed 2");
                return;
            }
        }
        g_test_step = 2;
        return;
    }
    // g_result = LISA_TEST_FAILED;
    // g_test_step = -1;
}

static void get_ws_event_cb(lisa_ws_event_t *event)
{
    g_event = event->what;
	if (event->what == LISA_WS_ON_CONNECTED) {
        LISA_LOGE(TAG, "lisa ws connect");
	} else if (event->what == LISA_WS_ON_DISCONNECTED) {
		LISA_LOGE(TAG, "lisa ws disconnect");
	} else {
		LISA_LOGE(TAG, "event %d", event->what);
	}
}

unit_test_result_t lisa_websocket_test()
{
    g_event = LISA_WS_ON_DISCONNECTED;
    g_result = LISA_TEST_PASS;
    g_test_step = 0;
    lisa_ws_request_t lisa_ws_req;
    lisa_ws_req.scheme = WS_TEST_SCHEME;
	lisa_ws_req.host = WS_TEST_HOST_1;
	lisa_ws_req.port = WS_TEST_PORT_1;
	lisa_ws_req.path = WS_TEST_PATH_1;
	lisa_ws_req.timeout_ms = WS_SEND_RETRY_DELAY;
	lisa_ws_req.on_data = get_ws_data_cb;
	lisa_ws_req.on_event = get_ws_event_cb;
	lisa_ws_req.user = NULL;
	lisa_ws_t* lisa_ws_ins = lisa_ws_init(&lisa_ws_req);
    if (!lisa_ws_ins) {
        LISA_LOGE(TAG, "lisa_ws_int failed");
        return LISA_TEST_FAILED;
    } 

    if (lisa_ws_connect(lisa_ws_ins)) {
		LISA_LOGE(TAG, "lisa evs websocket connect error");
		return LISA_TEST_FAILED;
	}

    int check_conn_state = 0;
    while (g_event == LISA_WS_ON_DISCONNECTED) {
        lisa_thread_mdelay(WS_SEND_RETRY_SLEEP_TIME);
        check_conn_state++;
        if (check_conn_state >= (WS_SEND_RETRY_DELAY / WS_SEND_RETRY_SLEEP_TIME)) {
            g_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "ws connect failed");
            break;
        }
    }

    if (g_result == LISA_TEST_FAILED) goto ERR;

    lisa_ws_send_text(lisa_ws_ins, WS_TEST_CONTENT);

    int wait_count = 0;
    while (g_test_step == 0) {
        wait_count++;
        lisa_thread_mdelay(100);
        if (wait_count > 10) {
            g_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "ws send and recv test failed 2");
            break;
        }
    }
    
    if (g_result == LISA_TEST_FAILED) goto ERR;
    
    lisa_ws_send_binary(lisa_ws_ins, g_ws_test_binary, 10);
    wait_count = 0;
    while (g_test_step == 1) {
        wait_count++;
        lisa_thread_mdelay(100);
        if (wait_count > 10) {
            g_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "ws send and recv test failed 3");
            break;
        }
    }
    
ERR:
    if (lisa_ws_ins) {
		lisa_ws_disconnect(lisa_ws_ins);
        lisa_ws_cleanup(lisa_ws_ins);
        lisa_ws_ins = NULL;
    }
    if (g_result == LISA_TEST_FAILED) return g_result;


    lisa_ws_req.scheme = WS_TEST_SCHEME;
	lisa_ws_req.host = WS_TEST_HOST_2;
	lisa_ws_req.port = WS_TEST_PORT_2;
	lisa_ws_req.path = WS_TEST_PATH_2;
	lisa_ws_req.timeout_ms = WS_SEND_RETRY_DELAY;
	lisa_ws_req.on_data = get_ws_data_cb_2;
	lisa_ws_req.on_event = get_ws_event_cb_2;
	lisa_ws_req.user = NULL;
	lisa_ws_ins = lisa_ws_init(&lisa_ws_req);
    if (!lisa_ws_ins) {
        LISA_LOGE(TAG, "lisa_ws_int failed");
        return LISA_TEST_FAILED;
    } 

    if (lisa_ws_connect(lisa_ws_ins)) {
		LISA_LOGE(TAG, "lisa evs websocket connect error");
		return LISA_TEST_FAILED;
	}

    wait_count = 0;
    while (g_test_step2 < 2) {
        wait_count++;
        lisa_thread_mdelay(50);
        if (wait_count > 1000) {
            g_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "ws conn disconnect cb failed");
            break;
        }
    }

    if (lisa_ws_ins) {
        lisa_ws_disconnect(lisa_ws_ins);
        lisa_ws_cleanup(lisa_ws_ins);
    }
    return g_result;
}
