#pragma once

#include "commands.h"
#include "esp_event_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOSTED_ERR_OK         0
#define HOSTED_ERR_TIMEOUT   -1
#define HOSTED_ERR_NO_MEM    -2
#define HOSTED_ERR_INVALID_STATUS    -3

typedef enum {
    HOSTED_LINK_EVT_ACTIVE,
    HOSTED_LINK_EVT_INACTIVE,
} hosted_link_event_t;

typedef enum {
    HOSTED_WIFI_EVT_AP_STARTED,                /**< ESP32 station start */
    HOSTED_WIFI_EVT_AP_STOPED,                 /**< ESP32 station stop */
    HOSTED_WIFI_EVT_AP_STACONNECTED,                 /**< ESP32 station stop */
    HOSTED_WIFI_EVT_AP_STADISCONNECTED,                 /**< ESP32 station stop */
    HOSTED_WIFI_EVT_AP_FAILED,
    HOSTED_WIFI_EVT_STA_CONNECTED,            /**< ESP32 station connected to AP */
    HOSTED_WIFI_EVT_STA_DISCONNECTED,         /**< ESP32 station disconnected from AP */
    HOSTED_WIFI_EVT_STA_FAILED,
} hosted_wifi_event_t;  //network adapter wifi event

typedef enum {
    WIFI_ERR_AP_NOT_FOUND,
    WIFI_ERR_INVALID_PASSWORD
} hosted_wifi_err_reason_t;

ESP_EVENT_DECLARE_BASE(HOSTED_LINK_EVENT_BASE);
ESP_EVENT_DECLARE_BASE(HOSTED_WIFI_EVENT_BASE);

int hosted_init(void);
int hosted_wifi_sta_connect(esp_hosted_sta_config_t *sta_config, uint32_t xTicksToWait);
int hosted_wifi_sta_disconnect(uint32_t xTicksToWait);
int hosted_wifi_ap_start(esp_hosted_softap_config_t *ap_config, uint32_t xTicksToWait);
int hosted_wifi_ap_stop(uint32_t xTicksToWait);


//Only available on event handler
hosted_wifi_err_reason_t hosted_wifi_get_failed_reason(void *event_data);
uint8_t *hosted_wifi_ap_get_sta_mac(void *event_data);

#ifdef __cplusplus
}
#endif
