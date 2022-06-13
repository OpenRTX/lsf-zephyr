#pragma once

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

void network_stack_init(void);
struct netif *wlanif_create_sta(uint8_t *mac_addr_bytes, netif_status_callback_fn netif_status_cb);
struct netif *wlanif_create_ap(uint8_t *mac_addr_bytes, netif_status_callback_fn netif_status_cb);
void wlanif_delete_sta(void);
void wlanif_delete_ap(void);

#ifdef __cplusplus
}
#endif