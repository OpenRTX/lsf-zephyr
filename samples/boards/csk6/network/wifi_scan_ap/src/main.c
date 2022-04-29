/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/printk.h>
#include <sys/sys_heap.h>

#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(csk_wifi_sta, LOG_LEVEL_DBG);

#include "csk6/csk_wifi.h"

static csk_wifi_event_cb_t wifi_event_cb;

static void wifi_event_handler(csk_wifi_event_t events, void *event_data, uint32_t data_len, void *arg)
{
    if (events & CSK_WIFI_EVT_SCAN_DONE) {
        printk("scan done\n");
    } else {
        abort();
    }
}

void main(void)
{
    csk_wifi_init();
    wifi_event_cb.handler = &wifi_event_handler;
    wifi_event_cb.events = CSK_WIFI_EVT_SCAN_DONE;
    wifi_event_cb.arg = NULL;
    csk_wifi_add_callback(&wifi_event_cb);

    csk_wifi_result_t result;

    while (1) {
        csk_wifi_scan_info_t *ap_list;
        int count = csk_wifi_scan_ap(&ap_list, &result, K_FOREVER);
        if (count < 0) {
            continue;
        }
        printk("-----------------------------------Scan AP list------------------------------------\n");
        for (int i = 0; i < count; i++) {
            printk("|ssid: %-20s | bssid: %-18s | channel: %-3d | rssi: %-3d|\n",
                    ap_list[i].ssid, ap_list[i].bssid, ap_list[i].channel, ap_list[i].rssi);
        }
        printk("-----------------------------------------------------------------------------------\n");
        k_free(ap_list);
        ap_list = NULL;
        k_msleep(1000);
    }
}
