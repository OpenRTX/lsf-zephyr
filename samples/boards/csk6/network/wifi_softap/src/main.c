/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include "csk6/csk_wifi.h"

static csk_wifi_event_cb_t wifi_event_cb;

static void wifi_event_handler(csk_wifi_event_t events, void *event_data, uint32_t data_len, void *arg)
{
    if (events & CSK_WIFI_EVT_AP_STARTED) {
        printk("[WiFi softap] started\n");
    } else if (events & CSK_WIFI_EVT_AP_STOPPED) {
        printk("[WiFi softap] stopped\n");
    } else if (events & CSK_WIFI_EVT_AP_STACONNECTED) {
        printk("[WiFi softap] station joined\n");
    } else if (events & CSK_WIFI_EVT_AP_STADISCONNECTED) {
        printk("[WiFi softap] station left\n");
    }
}

void main(void)
{
    csk_wifi_init();
    wifi_event_cb.handler = &wifi_event_handler;
    wifi_event_cb.events = CSK_WIFI_EVT_AP_STARTED |
                            CSK_WIFI_EVT_AP_STOPPED |
                            CSK_WIFI_EVT_AP_STACONNECTED |
                            CSK_WIFI_EVT_AP_STADISCONNECTED;
    wifi_event_cb.arg = NULL;
    csk_wifi_add_callback(&wifi_event_cb);

    csk_wifi_softap_config_t ap_config = {
        .ssid = CONFIG_EXAMPLE_WIFI_SSID,
        .pwd = CONFIG_EXAMPLE_WIFI_PASSWORD,
        .encryption_mode = CSK_WIFI_AUTH_WPA2_PSK, /* TODO: add to menuconfig */
        .bandwidth = CSK_WIFI_BW_HT40,
        .channel = CONFIG_EXAMPLE_WIFI_CHANNEL,
        .max_connections = CONFIG_EXAMPLE_WIFI_MAX_CONNECTTION,
    };

    csk_wifi_result_t wifi_result;
    csk_wifi_ap_start(&ap_config, &wifi_result, K_FOREVER);

    if (wifi_result == CSK_WIFI_ERR_AP_FAILED) {
        abort();
    }
    printk("ssid: %s password: %s\n", ap_config.ssid, ap_config.pwd);
}
