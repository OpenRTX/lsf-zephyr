#include <stdbool.h>
#include <stdint.h>
#include <zephyr.h>
#include <ztest.h>

#include "kernel.h"
#include "sys_clock.h"
#include "tc_util.h"
#include "wifi_manager/wifi_manager.h"
#include "ztest_assert.h"

extern void test_wifi_mgr_storage_save_ap_assert(wifi_mgr_sta_config_t *sta_config);


#define TEST_WIFI_MGR_COMMON_QUEUE_WAIT_TIMEOUT    (K_MSEC(500))

K_MSGQ_DEFINE(connected, 4, 1, 1);
K_SEM_DEFINE(connecting, 0, 1);
K_SEM_DEFINE(disconnected, 0, 1);

static wifi_mgr_sta_config_t correct_sta_config = {
	.ssid = CONFIG_CORRECT_WIFI_SSID,
	.pwd = CONFIG_CORRECT_WIFI_PASSWORD,
	.encryption_mode = CSK_WIFI_AUTH_WPA2_PSK,
};


void test_lib_wifi_mgr_clear_connect_status(void)
{
	k_msgq_purge(&connected);
}

static void connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
	int ret = 0;

	// TC_PRINT("fun: %s, connection_info->status: %d \n", __FUNCTION__, connection_info->status);
	switch (connection_info->status) {
	case WIFI_MGR_STA_DISCONNECTED:
		k_sem_give(&disconnected);
		break;
	case WIFI_MGR_STA_CONNECTING:
		k_sem_give(&connecting);
		break;
	case WIFI_MGR_STA_CONNECTED:
		do {
			ret = k_msgq_put(&connected, &(connection_info->status), K_SECONDS(1));
			if (ret != 0) {
				k_msgq_purge(&connected);
			}
		} while (ret != 0);
		break;
	default:
		break;
	}

}
void test_lib_wifi_mgr_enable_connect_cb(void)
{
	int ret = 0;

	ret = wifi_mgr_sta_add_connection_cb(connection_cb, NULL);
	zassert_equal(ret, 0, "add connected_callback_failed");
}

void test_lib_wifi_mgr_disable_connect_cb(void)
{
	int ret = 0;
	
	ret = wifi_mgr_sta_remove_connection_cb(connection_cb);
}

int test_lib_wifi_mgr_sta_get_connected_info(wifi_mgr_sta_config_t *got_sta_info)
{
	int ret = 0;

	ret = wifi_mgr_sta_get_connected_info(got_sta_info);
	zassert_equal(ret, 0, "get connected info failed , ret: %d", ret);

	return ret;

}

void wifi_auto_connect_start_assert(int interval_ms)
{
	int ret = 0;
	wifi_mgr_autoconn_config_t autoconn_conf = {
		.interval_ms = interval_ms,
	};

	test_lib_wifi_mgr_clear_connect_status();
	ret = wifi_mgr_auto_connect_start(&autoconn_conf);
	zassert_equal(ret, 0, "auto connect start failed, ret: %d", ret);
}

void wifi_auto_connect_stop_assert()
{
	int ret = 0;

	ret = wifi_mgr_auto_connect_stop();
	zassert_equal(ret, 0, "auto connect stop failed, ret: %d", ret);
}

void test_lib_wifi_mgr_is_connecting_assert(k_timeout_t timeout)
{
	int ret = 0;

	k_sem_reset(&connecting);
	ret = k_sem_take(&connecting, timeout);
	zassert_equal(ret, 0, "get connecting event failed, ret: %d", ret);
}

void test_lib_wifi_mgr_is_disconnected_assert(k_timeout_t timeout)
{
	int ret = 0;

	k_sem_reset(&disconnected);
	ret = k_sem_take(&disconnected, timeout);
	zassert_equal(ret, 0, "get disconnected event failed, ret: %d", ret);
}

wifi_mgr_connection_status_t test_lib_wifi_mgr_get_sta_connect_status_while_no_register_callback(int timeout_seconds)
{
	int cur_wait_time = 0;
	wifi_mgr_connection_status_t connect_status = WIFI_MGR_STA_MAX;

	do {
		if (cur_wait_time >= timeout_seconds) {
			break;
		}
		k_sleep(K_SECONDS(1));
		cur_wait_time++;
		connect_status = wifi_mgr_sta_get_status();
	} while (connect_status != WIFI_MGR_STA_CONNECTED);

	return connect_status;
}

wifi_mgr_connection_status_t test_lib_wifi_mgr_get_sta_connect_status(k_timeout_t wait_time_ms)
{
	int ret = 0;

	wifi_mgr_connection_status_t ret_status = WIFI_MGR_STA_MAX;
	wifi_mgr_connection_status_t queue_get_data = WIFI_MGR_STA_MAX;

	ret = k_msgq_get(&connected, &queue_get_data, wait_time_ms);
	if (ret == 0) {
		ret_status = queue_get_data;
	}

	return ret_status;
}

static void test_lib_wifi_mgr_sta_disable_and_check(void)
{
	int ret = 0;

	ret = wifi_mgr_sta_disable();
	zassert_equal(ret, 0, "wifi_mgr_sta_disable ret: %d", ret);
	test_wifi_mgr_storage_save_ap_assert(&correct_sta_config);

	wifi_mgr_autoconn_config_t autoconn_conf = {
		.interval_ms = 1000,
	};

	test_lib_wifi_mgr_clear_connect_status();
	ret = wifi_mgr_auto_connect_start(&autoconn_conf);
	zassert_equal(ret, -EIO, "wifi_mgr_auto_connect_start shouldn't return success");

	wifi_mgr_scan_info_t *aps_info;
	int ap_num = wifi_mgr_scan_ap(&aps_info, false);

	if (ap_num > 0) {
		k_free(aps_info);

	}
	zassert_equal(ap_num, -EIO, "wifi_mgr_scan_ap shouldn't return success");

	test_lib_wifi_mgr_clear_connect_status();
	ret = wifi_mgr_sta_connect(&correct_sta_config, false);
	zassert_equal(ret, -EIO, "wifi_mgr_sta_connect ret is not right, ret:%d", ret);


	ret = wifi_mgr_storage_delete_ap(&correct_sta_config);
	zassert_equal(ret, 0, "wifi_mgr_storage_delete_ap ret: %d", ret);
}


void test_lib_wifi_mgr_sta_disable_and_call_api_then_enable(void)
{
	int ret = 0;
	wifi_mgr_autoconn_config_t autoconn_conf = {
		.interval_ms = 1000,
	};

	test_lib_wifi_mgr_sta_disable_and_check();

	ret = wifi_mgr_sta_enable();
	zassert_equal(ret, 0, "wifi mgr sta enable ret: %d", ret);

	test_wifi_mgr_storage_save_ap_assert(&correct_sta_config);

	test_lib_wifi_mgr_enable_connect_cb();

	test_lib_wifi_mgr_clear_connect_status();
	ret = wifi_mgr_auto_connect_start(&autoconn_conf);
	zassert_equal(ret, 0, "wifi_mgr_auto_connect_start return ret:%d", ret);

	ret = test_lib_wifi_mgr_get_sta_connect_status(K_MSEC(10 * 1000));
	zassert_equal(ret, WIFI_MGR_STA_CONNECTED, "test_lib_wifi_mgr_sta_connect_status_assert");

	ret = wifi_mgr_sta_disconnect(false);
	zassert_equal(ret, 0, "wifi_mgr_sta_disconnect failed, ret: %d", ret);

}


void test_lib_wifi_mgr_auto_connect_then_disable_finally_enable(void)
{
	int ret = 0;
	wifi_mgr_autoconn_config_t autoconn_conf = {
		.interval_ms = 1000,
	};

	test_wifi_mgr_storage_save_ap_assert(&correct_sta_config);
	test_lib_wifi_mgr_enable_connect_cb();
	test_lib_wifi_mgr_clear_connect_status();

	ret = wifi_mgr_auto_connect_start(&autoconn_conf);
	zassert_equal(ret, 0, "auto connect start failed, ret: %d", ret);
	ret = wifi_mgr_sta_disable();
	zassert_equal(ret, 0, "wifi_mgr_sta_disable ret: %d", ret);

	ret = test_lib_wifi_mgr_get_sta_connect_status(K_MSEC(10 * 1000));
	zassert_not_equal(ret, WIFI_MGR_STA_CONNECTED, "test_lib_wifi_mgr_get_sta_connect_status is wrong: %d", ret);

	ret = wifi_mgr_sta_enable();
	zassert_equal(ret, 0, "wifi mgr sta enable ret: %d", ret);

}




