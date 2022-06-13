#pragma once

// #include "esp_event_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NETWORK_ADATPTER_STA,
    NETWORK_ADATPTER_AP,
    NETWORK_ADATPTER_MAX,
} network_adapter_if_type_t;

struct network_adapter_if {
    struct netif *wlanif;
    struct network_handle *net_handle;
};

typedef struct network_adapter_if network_adapter_if_t;

typedef enum {
    NA_IP_EVT_STA_GOT_IP,                   /*!< station got IP from connected AP */
    NA_IP_EVT_AP_ASSIGNED_IP,               /*!< SoftAP assigned IP to connected station device */
} na_ip_event_t;  //network adapter ip event

// ESP_EVENT_DECLARE_BASE(NA_IP_EVENT_BASE);


void network_adapter_init(void);
void network_adapter_register_sta(void);
void network_adapter_unregister_sta(void);
void network_adapter_register_ap(void);
void network_adapter_unregister_ap(void);


err_t network_adapter_output(struct netif *netif, struct pbuf *in_pbuf);

#ifdef __cplusplus
}
#endif
