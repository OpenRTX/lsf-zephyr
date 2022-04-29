/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/printk.h>
#include <sys/reboot.h>

#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include "wifi_manager/wifi_manager.h"

K_SEM_DEFINE(wifi_conn_sem, 0, 1);
K_SEM_DEFINE(ipv4_sem, 0, 1);

static struct net_mgmt_event_callback dhcp_cb;
static void handler_cb(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
        return;
    }

    char buf[NET_IPV4_ADDR_LEN];

    printk("Your address: %s\n", net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip, buf, sizeof(buf)));
    printk("Lease time: %u seconds\n", iface->config.dhcpv4.lease_time);
    printk("Subnet: %s\n", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->netmask, buf, sizeof(buf)));
    printk("Router: %s\n", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
    k_sem_give(&ipv4_sem);
}

static void connected_cb(wifi_mgr_sta_config_t *connected_info, void *arg)
{
    printk("Connected to AP successfully\n");
    printk("ssid: %s  bssid: %s  channel: %d  rssi: %d\n",
            connected_info->ssid, connected_info->bssid, connected_info->channel, connected_info->rssi);
    k_sem_give(&wifi_conn_sem);
}

static void scandone_cb(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg)
{
    printk("WiFi scan done\n");
}

void main(void)
{
    int ret = wifi_mgr_init();
    if (ret != 0) {
        printk("wifi manager init failed\n");
        abort();
    }

    wifi_mgr_sta_add_connected_cb(connected_cb, NULL);
    wifi_mgr_add_scan_done_cb(scandone_cb, NULL);
    
    wifi_mgr_scan_info_t *aps_info;
    int ap_num = wifi_mgr_scan_ap(&aps_info, false);
    printk("-------------------------------Scan AP list---------------------------------\n");
    for (int i = 0; i < ap_num; i++) {
        printk("|ssid: %-20s | bssid: %-18s | channel: %-3d | rssi: %-3d|\n",
                aps_info[i].ssid, aps_info[i].bssid, aps_info[i].channel, aps_info[i].rssi);
    }
    printk("----------------------------------------------------------------------------\n");

    wifi_mgr_sta_config_t *saved_infos;
    int saved_infos_num = wifi_mgr_storage_search_ap(&saved_infos, SEARCH_ALL, NULL);
    if (saved_infos_num < 0) {
        printk("wifi search ap info failed\n");
        abort();
    }
    printk("---------------------------wifi storage info list---------------------------\n");
    for (int i = 0; i < saved_infos_num; i++) {
        printk("|ssid: %-20s | bssid: %-18s | channel: %-3d | rssi: %-3d|\n",
                saved_infos[i].ssid, saved_infos[i].bssid, saved_infos[i].channel, saved_infos[i].rssi);
    }
    printk("----------------------------------------------------------------------------\n");

    static wifi_mgr_sta_config_t sta_config = {
        .ssid = CONFIG_EXAMPLE_WIFI_SSID,
        .pwd = CONFIG_EXAMPLE_WIFI_PASSWORD,
        .encryption_mode = CSK_WIFI_AUTH_WPA2_PSK,
    };

    if (saved_infos_num > 0) {
        wifi_mgr_sta_config_t *hit_infos;
        int hit_num = wifi_mgr_storage_search_ap(&hit_infos, SEARCH_BY_SSID, sta_config.ssid);
        if (hit_num > 0) {
            wifi_mgr_autoconn_config_t autoconn_conf = {
                .interval_ms = 3000,
            };
            wifi_mgr_auto_connect_start(&autoconn_conf);
            printk("Found ssid in NVS storage, ssid: %s, bssid: %s, password: %s\nWiFi auto connect start ...\n",
                    hit_infos[0].ssid, hit_infos[0].bssid, hit_infos[0].pwd);
            goto __out;
        }
    }


    printk("WiFi connecting to AP ...\n");
    ret = wifi_mgr_sta_connect(&sta_config, false);
    if (ret != 0) {
        printk("wifi station connect failed\n");
        abort();
    }

    printk("WiFi saving AP info into NVS storage\n");
    ret = wifi_mgr_storage_save_ap(&sta_config); /* Save AP info */
    if (ret != 0) {
        printk("wifi save ap info failed\n");
        abort();
    }

__out:
    k_free(aps_info);
    k_free(saved_infos);
    k_sem_take(&wifi_conn_sem, K_FOREVER); /* Wait for WiFi connected */

    net_mgmt_init_event_callback(&dhcp_cb, handler_cb, NET_EVENT_IPV4_DHCP_BOUND);
    net_mgmt_add_event_callback(&dhcp_cb);
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        printk("wifi interface not available\n");
        return;
    }
    net_dhcpv4_start(iface);
    
    k_sem_take(&ipv4_sem, K_FOREVER);
    for (int i = 3; i >= 0; i--) {
        printk("Waiting for Reboot ...%d\n", i);
        k_msleep(1000);
    }
    sys_reboot(SYS_REBOOT_WARM);
}
