#include <logging/log.h>
#include <stdbool.h>
#include <zephyr.h>
#include <ztest.h>

#include "kernel.h"
#include "sys_clock.h"
#include "tc_util.h"
#include "wifi_manager.h"
#include "ztest_assert.h"

void test_wifi_mgr_storage_delete_all_ap(void)
{
	wifi_mgr_sta_config_t *saved_infos;
	int saved_infos_num = wifi_mgr_storage_search_ap(&saved_infos, SEARCH_ALL, NULL);

	for (int i = 0; i < saved_infos_num; i++) {
		wifi_mgr_storage_delete_ap(&(saved_infos[i]));
	}

	k_free(saved_infos);
}

static void check_saved_ssid_is_exist(char *ssid)
{
	wifi_mgr_sta_config_t *saved_infos;
	int saved_infos_num = wifi_mgr_storage_search_ap(&saved_infos, SEARCH_ALL, NULL);

	zassert_true(saved_infos_num > 0, "wifi search ap info failed\n");

	wifi_mgr_sta_config_t *hit_infos;
	int hit_num = wifi_mgr_storage_search_ap(&hit_infos, SEARCH_BY_SSID,
						 ssid);

	zassert_true(hit_num > 0, "Can't find storage ap %s", ssid);

	k_free(hit_infos);
	k_free(saved_infos);
}

void test_wifi_mgr_storage_save_ap_force(wifi_mgr_sta_config_t *in_sta_config)
{
	int ret = 0;
	wifi_mgr_sta_config_t cur_sta_config = { 0 };

	memcpy((void *)&cur_sta_config, (void *)in_sta_config, sizeof(cur_sta_config));
	wifi_mgr_sta_config_t *sta_config = &cur_sta_config;

	ret = wifi_mgr_storage_save_ap_force(NULL);
	zassert_equal(ret, -EINVAL, "force save ap shouldn't succeed, ret: %d", ret);

	/* test api force replace the old one */
	sta_config->encryption_mode = CSK_WIFI_AUTH_OPEN;
	ret = wifi_mgr_storage_save_ap_force(sta_config);
	zassert_equal(ret, 0, "save ap force failed, ret:%d", ret);

	wifi_mgr_sta_config_t *hit_infos;
	int hit_num = wifi_mgr_storage_search_ap(&hit_infos, SEARCH_BY_SSID,
						 sta_config->ssid);

	zassert_equal(hit_num, 1, "Can't find storage ap %s", sta_config->ssid);
	zassert_equal(hit_infos->encryption_mode, CSK_WIFI_AUTH_OPEN, "Looks like ap overwrite failed, encryption_mode: %d", hit_infos->encryption_mode);
	k_free(hit_infos);

}

void test_wifi_mgr_storage_save_ap_assert(wifi_mgr_sta_config_t *sta_config)
{
	int ret = 0;

	ret = wifi_mgr_storage_save_ap(sta_config); /* Save AP info */
	zassert_true((ret == 0) || (ret == -EEXIST),
		     "wifi save ap info failed");

	check_saved_ssid_is_exist(sta_config->ssid);
}
