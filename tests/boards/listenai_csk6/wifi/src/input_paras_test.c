#include "test_csk_wifi.h"

void test_wifi_timeout_para(void)
{
    int ret;
    csk_wifi_result_t result;
    csk_wifi_event_t event;

    csk_wifi_sta_config_t sta_config = {
		.ssid = CONFIG_TEST_VALID_WIFI_SSID,
		.pwd = CONFIG_TEST_VALID_WIFI_PASSWORD,
		.encryption_mode = CSK_WIFI_AUTH_WPA2_PSK
	};

    ret = csk_wifi_sta_connect(&sta_config, &result, K_SECONDS(1));
    zassert_equal(ret, -ENOTSUP, "wifi connect timeout = 1s test failed, ret:%d", ret);
    ret = csk_wifi_sta_connect(&sta_config, &result, K_SECONDS(5));
    zassert_equal(ret, -ENOTSUP, "wifi connect timeout = 5s test failed, ret:%d", ret);
    ret = csk_wifi_sta_connect(&sta_config, &result, K_SECONDS(10));
    zassert_equal(ret, -ENOTSUP, "wifi connect timeout = 10s test failed, ret:%d", ret);

    /* Test wifi csk_wifi_sta_connect() timeout parameter is FOREVER*/
    ret = csk_wifi_sta_connect(&sta_config, &result, K_FOREVER);
    zassert_equal(ret, 0, "wifi connect timeout = NO_WAIT test failed, ret:%d", ret);
    ret = csk_wifi_connected_msgq_take(&event, K_SECONDS(30));
    zassert_equal(ret, 0, "wait wifi event failed, ret:%d", ret);
    zassert_equal(event, CSK_WIFI_EVT_STA_CONNECTED, "wifi event invalid, event: 0x%x", event);
    ret = csk_wifi_sta_disconnect(&result, K_FOREVER);
    zassert_equal(ret, 0, "wifi disconnect failed, ret:%d", ret);
    ret = csk_wifi_disconnected_msgq_take(&event, K_SECONDS(30));
    zassert_equal(ret, 0, "wait wifi event failed, ret:%d", ret);
    zassert_equal(event, CSK_WIFI_EVT_STA_DISCONNECTED, "wifi event invalid, event: 0x%x", event);

    /* Test wifi csk_wifi_sta_connect() timeout parameter is NO_WAIT*/
    ret = csk_wifi_sta_connect(&sta_config, &result, K_FOREVER);
    zassert_equal(ret, 0, "wifi connect timeout = NO_WAIT test failed, ret:%d", ret);
    ret = csk_wifi_connected_msgq_take(&event, K_SECONDS(30));
    zassert_equal(ret, 0, "wait wifi event failed, ret:%d", ret);
    zassert_equal(event, CSK_WIFI_EVT_STA_CONNECTED, "wifi event invalid, event: 0x%x", event);
    ret = csk_wifi_sta_disconnect(&result, K_FOREVER);
    zassert_equal(ret, 0, "wifi disconnect failed, ret:%d", ret);
    ret = csk_wifi_disconnected_msgq_take(&event, K_SECONDS(30));
    zassert_equal(ret, 0, "wait wifi event failed, ret:%d", ret);
    zassert_equal(event, CSK_WIFI_EVT_STA_DISCONNECTED, "wifi event invalid, event: 0x%x", event);
}
