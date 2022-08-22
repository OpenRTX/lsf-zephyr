/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_csk_wifi.h"
#include "logging/log.h"

LOG_MODULE_DECLARE(test_csk6_wifi, CONFIG_LOG_DEFAULT_LEVEL);

static csk_wifi_event_cb_t wifi_event_cb;
K_MSGQ_DEFINE(test_csk_wifi_msgq, sizeof(csk_wifi_event_t), 1, 4);

int csk_wifi_msgq_give(csk_wifi_event_t events)
{
	return k_msgq_put(&test_csk_wifi_msgq, &events, K_NO_WAIT);
}

int csk_wifi_msgq_take(csk_wifi_event_t *events, k_timeout_t timeout)
{
	return k_msgq_get(&test_csk_wifi_msgq, events, timeout);
}

static void wifi_event_handler(csk_wifi_event_t events, void *event_data, uint32_t data_len, void *arg)
{
	if (events & CSK_WIFI_EVT_STA_CONNECTED) {
		LOG_DBG("file:%s, func:%s, connected\n", __FILE__, __FUNCTION__);
	} else if (events & CSK_WIFI_EVT_STA_DISCONNECTED) {
		LOG_DBG("file:%s, func:%s, disconnected\n", __FILE__, __FUNCTION__);
	} else {
	}
	csk_wifi_msgq_give(events);
}



static int connect_target_wifi_with_retry(csk_wifi_sta_config_t *sta_config, int retry_times)
{
	int ret = 0;
	csk_wifi_result_t wifi_result;
	int retry_count = 0;

	do {
		LOG_DBG("connecting to wifi: %s", sta_config->ssid);
		ret = csk_wifi_sta_connect(sta_config, &wifi_result, K_FOREVER);
		printk("func:%s, line:%d, ssid: %s, len: %d\n", __FUNCTION__, __LINE__,
		       sta_config->ssid, sizeof(sta_config->ssid));
		if (ret == 0) {
			break;
		} else {
			retry_count++;
			k_msleep(1000);
		}
	} while (retry_count <= retry_times);
	if (wifi_result > 0) {
		ret = wifi_result;
	}
	return ret;
}

void test_csk_wifi_setup(void)
{
	int ret = -1;

	ret = csk_wifi_init();
	zassert_equal(ret, 0, "csk_wifi_init failed");
	wifi_event_cb.handler = &wifi_event_handler;
	wifi_event_cb.events = CSK_WIFI_EVT_STA_CONNECTED | CSK_WIFI_EVT_STA_DISCONNECTED;
	wifi_event_cb.arg = NULL;
	csk_wifi_add_callback(&wifi_event_cb);
}

void test_csk_wifi_get_mac(void)
{
	uint8_t mac_addr[6] = {0};
	int ret = csk_wifi_get_mac(CSK_WIFI_MODE_STA, mac_addr);
	zassert_equal(ret, 0, "csk_wifi_get_mac failed, %d", ret);
	LOG_INF("wifi station mac addr: %x:%x:%x:%x:%x:%x",
			mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
			mac_addr[4], mac_addr[5]);
}

void test_csk_wifi_scan(void)
{
	int ap_num = 0;
	csk_wifi_result_t result;
	csk_wifi_scan_info_t *aps_info;

	ap_num = csk_wifi_scan_ap(&aps_info, &result, K_FOREVER);
	zassert_not_equal(ap_num, 0, "ap_num is 0");

	for (int idx = 0; idx < ap_num; idx++) {
		LOG_DBG("found the target ssid:%s ", aps_info[idx].ssid);
	}

	LOG_DBG("result: %d", result);
	zassert_equal(result, CSK_WIFI_SUCCESS, "scan failed");
	zassert_not_null(aps_info, "aps_info is null");


}


void test_csk_wifi_connect_with_correct_ssid_and_pwd(void)
{
	csk_wifi_sta_config_t sta_config = {
		.ssid = CONFIG_TEST_VALID_WIFI_SSID,
		.pwd = CONFIG_TEST_VALID_WIFI_PASSWORD,
		.encryption_mode = CSK_WIFI_AUTH_WPA2_PSK
	};
	csk_wifi_event_t event = CSK_WIFI_EVT_AP_STARTED;
	int retry_count = 5;
	int ret = 0;

	ret = connect_target_wifi_with_retry(&sta_config, retry_count);
	zassert_equal(ret, 0, "csk wifi msg take failed");

	ret = csk_wifi_msgq_take(&event, K_SECONDS(5));
	zassert_equal(ret, 0, "csk wifi msg take failed");

	zassert_equal(event, CSK_WIFI_EVT_STA_CONNECTED, "connect to target wifi failed");

	LOG_DBG("ssid: %s  pwd: %s  bssid: %s  channel: %d  rssi: %d",
		sta_config.ssid, sta_config.pwd, sta_config.bssid, sta_config.channel,
		sta_config.rssi);

	char ssid[CSK_WIFI_SSID_LEN + 1] = CONFIG_TEST_VALID_WIFI_SSID;
	char pwd[CSK_WIFI_PWD_LEN + 1] = CONFIG_TEST_VALID_WIFI_PASSWORD;

	zassert_mem_equal(sta_config.ssid, ssid, sizeof(sta_config.rssi), NULL);
	zassert_mem_equal(sta_config.pwd, pwd, sizeof(sta_config.pwd), NULL);
	zassert_equal(sta_config.encryption_mode, CSK_WIFI_AUTH_WPA2_PSK, "connected with wrong encryption mode");


}

void test_csk_wifi_connect_with_not_exist_ssid(void)
{
	csk_wifi_sta_config_t sta_config = {
		.ssid = CONFIG_TEST_INVALID_WIFI_SSID,
		.pwd = CONFIG_TEST_INVALID_WIFI_PASSWORD,
		.encryption_mode = CSK_WIFI_AUTH_WPA2_PSK
	};
	csk_wifi_event_t event = -1;
	int retry_count = 1;
	int ret = 0;

	ret = connect_target_wifi_with_retry(&sta_config, retry_count);
	zassert_equal(ret, CSK_WIFI_ERR_AP_NOT_FOUND, "event not equal ssid not found %d", ret);
	ret = csk_wifi_msgq_take(&event, K_NO_WAIT);
	zassert_equal(ret, -ENOMSG, "event not equal disconnected %d", ret);

	LOG_DBG("ssid: %s  pwd: %s  bssid: %s  channel: %d  rssi: %d",
		sta_config.ssid, sta_config.pwd, sta_config.bssid, sta_config.channel,
		sta_config.rssi);


}

void test_csk_wifi_connect_with_correct_ssid_but_wrong_pwd(void)
{
	csk_wifi_sta_config_t sta_config = {
		.ssid = CONFIG_TEST_VALID_WIFI_SSID,
		.pwd = CONFIG_TEST_INVALID_WIFI_PASSWORD,
		.encryption_mode = CSK_WIFI_AUTH_WPA2_PSK
	};
	csk_wifi_event_t event = CSK_WIFI_EVT_AP_STARTED;
	int retry_count = 1;
	int ret = 0;

	ret = connect_target_wifi_with_retry(&sta_config, retry_count);
	zassert_equal(ret, CSK_WIFI_ERR_INVALID_PASSWORD, "ret is not equal to CSK_WIFI_ERR_INVALID_PASSWORD %d", ret);
	ret = csk_wifi_msgq_take(&event, K_NO_WAIT);
	zassert_equal(ret, -ENOMSG, "csk wifi msg take failed %d", ret);

	LOG_DBG("ssid: %s  pwd: %s  bssid: %s  channel: %d  rssi: %d",
		sta_config.ssid, sta_config.pwd, sta_config.bssid, sta_config.channel,
		sta_config.rssi);


}


void test_main(void)
{
	ztest_test_suite(csk_wifi_test,
			 ztest_unit_test(test_csk_wifi_setup),
			 ztest_unit_test(test_csk_wifi_get_mac),
			 ztest_unit_test(test_wifi_timeout_para),
			 ztest_unit_test(test_csk_wifi_scan),
			 ztest_unit_test(test_csk_wifi_connect_with_correct_ssid_but_wrong_pwd),
			 ztest_unit_test(test_csk_wifi_connect_with_not_exist_ssid),
			 ztest_unit_test(test_csk_wifi_connect_with_correct_ssid_and_pwd)

			 );
	ztest_run_test_suite(csk_wifi_test);
}
