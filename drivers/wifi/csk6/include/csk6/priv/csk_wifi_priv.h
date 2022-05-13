#pragma once

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include <kernel.h>
#include <net/ethernet.h>
#include <net/net_pkt.h>
#include <net/net_if.h>

#if CONFIG_ESP_HOSTED
#include "spi_drv.h"
#include "netdev_api.h"
#include "hosted/hosted.h"
#include "csk6/csk_wifi.h"
#elif CONFIG_XR819S
#include "net/wlan/wlan_defs.h"
#include "net/wlan/wlan.h"
#include "net/wlan/wlan_ext_req.h"
#else
#error "Please select a wifi module driver"
#endif

#include "csk6/csk_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MODE_AP    10
#define EVENT_SUBTYPE(event)  ((uint16_t)((event) & 0xFFFF))

struct csk_wifi_data {
    const struct device *dev;
    void *virtual_netif;
    struct net_if *netif;
    uint8_t mac_addr[6];
    union {
#if CONFIG_ESP_HOSTED
        esp_hosted_sta_config_t sta_config;
        esp_hosted_softap_config_t ap_config;
#else
        csk_wifi_sta_config_t sta_config;
        csk_wifi_softap_config_t ap_config;
#endif
    };
#ifdef CONFIG_ESP_HOSTED
   hosted_event_callback_t hosted_event_cb;
#else
    bool is_wait_result;
#endif
};

typedef enum {
    WIFI_CMD_STA_INIT,
    WIFI_CMD_STA_CONNECT,
    WIFI_CMD_STA_DISCONNECT,
    WIFI_CMD_AP_INIT,
    WIFI_CMD_AP_START,
    WIFI_CMD_AP_STOP,
    WIFI_CMD_SCAN,
} wifi_cmd_t;

typedef struct {
    int result;
    void *arg;
} wifi_ret_msg_t;

typedef struct {
    csk_wifi_scan_info_t *ap_list;
    int count;
} wifi_scan_list_t;

#if CONFIG_ESP_HOSTED
const struct device * wifi_get_dev(wifi_mode_t mode);
#else
const struct device * wifi_get_dev(int mode);
#endif
int wifi_send_command(const struct device *dev, wifi_cmd_t command, k_timeout_t timeouts);
int wifi_take_result(wifi_ret_msg_t *ret_msg, k_timeout_t timeout);

void wifi_dispatch_callbacks(csk_wifi_event_t events, void *event_data, uint32_t event_data_len);
#if CONFIG_ESP_HOSTED
wifi_auth_mode_t wifi_get_encryption_mode(csk_wifi_encryption_mode_t mode);
#else
csk_wifi_encryption_mode_t wifi_get_encryption_mode(csk_wifi_encryption_mode_t mode);
#endif

#ifdef __cplusplus
}
#endif
