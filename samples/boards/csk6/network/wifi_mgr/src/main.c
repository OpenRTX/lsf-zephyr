/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kernel.h"
#include "sys/printk.h"
#include <stdbool.h>
#include <stdlib.h>
#include <sys/reboot.h>

#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include "wifi_manager/wifi_manager.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(wifi_manager_sample, LOG_LEVEL_DBG);

K_SEM_DEFINE(wifi_conn_sem, 0, 1);
K_SEM_DEFINE(ipv4_sem, 0, 1);

static wifi_mgr_sta_config_t sta_config = {
	.ssid = CONFIG_EXAMPLE_WIFI_SSID,
	.pwd = CONFIG_EXAMPLE_WIFI_PASSWORD,
	.encryption_mode = CSK_WIFI_AUTH_WPA2_PSK,
};
static struct net_mgmt_event_callback dhcp_cb;
static void handler_cb(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
		       struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
		return;
	}

	char buf[NET_IPV4_ADDR_LEN];

	LOG_DBG("Your address: %s\n",
		net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip, buf, sizeof(buf)));
	LOG_DBG("Lease time: %u seconds\n", iface->config.dhcpv4.lease_time);
	LOG_DBG("Subnet: %s\n",
		net_addr_ntop(AF_INET, &iface->config.ip.ipv4->netmask, buf, sizeof(buf)));
	LOG_DBG("Gateway: %s\n",
		net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
	k_sem_give(&ipv4_sem);
}

static void connected_cb(wifi_mgr_sta_config_t *connected_info, void *arg)
{
	LOG_INF("Connected to AP successfully\n");
	LOG_DBG("ssid: %s  bssid: %s  channel: %d  rssi: %d\n", connected_info->ssid,
		connected_info->bssid, connected_info->channel, connected_info->rssi);
	k_sem_give(&wifi_conn_sem);
}

static void scandone_cb(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg)
{
	LOG_DBG("WiFi scan done\n");
}

static void delete_all_old_ap_storage(void)
{
	wifi_mgr_sta_config_t *saved_infos;
	int saved_infos_num = wifi_mgr_storage_search_ap(&saved_infos, SEARCH_ALL, NULL);
	for (int i = 0; i < saved_infos_num; i++) {
		wifi_mgr_storage_delete_ap(&(saved_infos[i]));
	}

	k_free(saved_infos);
}

void main(void)
{
	int ret;
	ret = wifi_mgr_init();
	if (ret != 0) {
		LOG_ERR("wifi manager init failed\n");
		abort();
	}

	//uncomment below function when you change the CONFIG_EXAMPLE_WIFI_SSID but you don't want to erase nvs manually.
	// delete_all_old_ap_storage();

	ret = wifi_mgr_storage_save_ap(&sta_config); /* Save AP info */
	if (ret != 0) {
		LOG_DBG("wifi save ap info failed\n");
	}
	ret = wifi_mgr_sta_add_connected_cb(connected_cb, NULL);
	ret = wifi_mgr_add_scan_done_cb(scandone_cb, NULL);

	wifi_mgr_sta_config_t *saved_infos;
	int saved_infos_num = wifi_mgr_storage_search_ap(&saved_infos, SEARCH_ALL, NULL);
	if (saved_infos_num < 0) {
		LOG_DBG("wifi search ap info failed\n");
		abort();
	}

	if (saved_infos_num > 0) {
		wifi_mgr_sta_config_t *hit_infos;
		int hit_num =
			wifi_mgr_storage_search_ap(&hit_infos, SEARCH_BY_SSID, sta_config.ssid);
		if (hit_num > 0) {
			wifi_mgr_autoconn_config_t autoconn_conf = {
				.interval_ms = 3000,
			};
			wifi_mgr_auto_connect_start(&autoconn_conf);
			LOG_DBG("Found ssid in NVS storage, ssid: %s, bssid: %s, password: %s",
				hit_infos[0].ssid, hit_infos[0].bssid, hit_infos[0].pwd);
			k_free(hit_infos);
			goto __out;
		}
	}

__out:
	k_free(saved_infos);
	k_sem_take(&wifi_conn_sem, K_FOREVER); /* Wait for WiFi connected */

	net_mgmt_init_event_callback(&dhcp_cb, handler_cb, NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&dhcp_cb);
	struct net_if *iface = net_if_get_default();
	if (!iface) {
		LOG_DBG("wifi interface not available\n");
		return;
	}
	net_dhcpv4_start(iface);
}
