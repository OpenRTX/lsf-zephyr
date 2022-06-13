#pragma once

#include "network_adapter/network_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t network_adapter_output(struct netif *netif, struct pbuf *in_pbuf);

#ifdef __cplusplus
}
#endif
