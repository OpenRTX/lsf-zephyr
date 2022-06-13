#include <string.h>
// #include "FreeRTOS.h"
// #include "task.h"
// #include "semphr.h"
// #include "spi_drv.h"
// #include "common/trace.h"
// #include "common/common.h"
// #include "netdev_api.h"

// #include "venus_log.h"
// #include "hosted_gpio.h"
// #include "netdev_api.h"

#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"
// #include "dhcpserver/dhcpserver.h"
// #include "dhcpserver/dhcpserver_options.h"

// #include "ethernetif.h"
// #include "esp_event.h"

// #include "hosted/priv/hosted_priv.h"
#include "network_adapter/priv/wlanif.h"
#include "network_adapter/priv/network_adapter_priv.h"

// static SemaphoreHandle_t sta_sem = NULL;
static network_adapter_if_t s_network_adapter_if[NETWORK_ADATPTER_MAX];

// const char *NA_IP_EVENT_BASE = "ip_event";

static void network_adapter_input(struct network_handle *netdev_handle);
static int convert_str_to_bytes(uint8_t *out, const char *s);
static void netif_status_event_handler(struct netif *netif);
static void netif_dhcps_cb(u8_t client_ip[4]);

static inline network_adapter_if_t *network_adapter_get_if(network_adapter_if_type_t net_ada_if_type)
{
    return &s_network_adapter_if[net_ada_if_type];
}

static inline  network_adapter_if_t* net_ada_get_adaif_by_wlanif(struct netif *netif)
{
    for (int i = 0; i < NETWORK_ADATPTER_MAX; i++) {
        if (s_network_adapter_if[i].wlanif == netif) {
            return &s_network_adapter_if[i];
        }
    }
    return NULL;
}

static inline  network_adapter_if_t* net_ada_get_adaif_by_nethandle(struct network_handle *netdev_handle)
{
    for (int i = 0; i < NETWORK_ADATPTER_MAX; i++) {
        if (s_network_adapter_if[i].net_handle == netdev_handle) {
            return &s_network_adapter_if[i];
        }
    }
    return NULL;
}

void network_adapter_register_sta(void)
{
    static bool is_network_open = false;  //Fixme: virtrul network could not be closed now.
    network_adapter_if_t *net_ada_sta_if = network_adapter_get_if(NETWORK_ADATPTER_STA);
    if (is_network_open == false) {
        net_ada_sta_if->net_handle = network_open(STA_INTERFACE, network_adapter_input);
        is_network_open = true;
    } else {
        net_ada_sta_if->net_handle->ndev->state = NETDEV_STATE_UP;
    }
    char mac_addr_strs[20];
    uint8_t mac_addr_bytes[6];
    wifi_get_mac(WIFI_MODE_STA, mac_addr_strs);
    convert_str_to_bytes(mac_addr_bytes, mac_addr_strs);
    net_ada_sta_if->wlanif = wlanif_create_sta(mac_addr_bytes, netif_status_event_handler);

    dhcp_start(net_ada_sta_if->wlanif);
}

void network_adapter_unregister_sta(void)
{
    network_adapter_if_t *net_ada_sta_if = network_adapter_get_if(NETWORK_ADATPTER_STA);
    dhcp_release(net_ada_sta_if->wlanif);
    dhcp_stop(net_ada_sta_if->wlanif);
    dhcp_cleanup(net_ada_sta_if->wlanif);
    net_ada_sta_if->net_handle->ndev->state = NETDEV_STATE_DOWN; //Fixme: virtrul network could not be closed now.
    wlanif_delete_sta();
}

void network_adapter_register_ap(void)
{
    static bool is_network_open = false;  //Fixme: virtrul network could not be closed now.
    network_adapter_if_t *net_ada_ap_if = network_adapter_get_if(NETWORK_ADATPTER_AP);
    if (is_network_open == false) {
        net_ada_ap_if->net_handle = network_open(SOFTAP_INTERFACE, network_adapter_input);
        is_network_open = true;
    } else {
        net_ada_ap_if->net_handle->ndev->state = NETDEV_STATE_UP;
    }
    char mac_addr_strs[20];
    uint8_t mac_addr_bytes[6];
    wifi_get_mac(WIFI_MODE_AP, mac_addr_strs);
    convert_str_to_bytes(mac_addr_bytes, mac_addr_strs);
    net_ada_ap_if->wlanif = wlanif_create_ap(mac_addr_bytes, NULL);
    dhcps_set_new_lease_cb(netif_dhcps_cb);
    dhcps_set_option_info(SUBNET_MASK, (void*)&net_ada_ap_if->wlanif->netmask, sizeof(ip4_addr_t));
    dhcps_start(net_ada_ap_if->wlanif, net_ada_ap_if->wlanif->ip_addr);
}

void network_adapter_unregister_ap(void)
{
    network_adapter_if_t *net_ada_ap_if = network_adapter_get_if(NETWORK_ADATPTER_AP);
    dhcps_stop(net_ada_ap_if->wlanif);
    net_ada_ap_if->net_handle->ndev->state = NETDEV_STATE_DOWN;  //Fixme: virtrul network could not be closed now.
    wlanif_delete_ap();
}

static void network_adapter_input(struct network_handle *netdev_handle)
{
    network_adapter_if_t *net_adapter_if = net_ada_get_adaif_by_nethandle(netdev_handle);
	struct net_pbuf *rx_buffer = network_read(net_adapter_if->net_handle, 0);
	if (rx_buffer) {
        ethernetif_input(net_adapter_if->wlanif, rx_buffer->payload, rx_buffer->len);
        if (rx_buffer->payload) {
		    vPortFree(rx_buffer->payload);
		    rx_buffer->payload = NULL;
        }
        if (rx_buffer) {
            vPortFree(rx_buffer);
		    rx_buffer = NULL;
        }
	}
}

err_t network_adapter_output(struct netif *netif, struct pbuf *in_pbuf)
{
    err_t ret = ERR_OK;
    struct net_pbuf *net_pbuf = NULL;
    struct pbuf *in_pbuf_temp = NULL;

    network_adapter_if_t *net_adapter_if = net_ada_get_adaif_by_wlanif(netif);
    if (net_adapter_if == NULL) {
        return ERR_IF;
    }
    net_pbuf = pvPortMalloc(sizeof(struct net_pbuf));
    net_pbuf->payload = pvPortMalloc(sizeof(uint8_t) * in_pbuf->tot_len);
    net_pbuf->len = 0;
    if ((net_pbuf->payload == NULL) || (net_pbuf == NULL)) {
        printf("output no mem\n");
        ret = ERR_MEM;
        goto __cleanup;
    }

    for (in_pbuf_temp = in_pbuf; in_pbuf_temp != NULL; in_pbuf_temp = in_pbuf->next) {
        memcpy(&net_pbuf->payload[net_pbuf->len], in_pbuf_temp->payload, in_pbuf_temp->len);
        net_pbuf->len += in_pbuf_temp->len;
    }

    if (net_pbuf->len != in_pbuf->tot_len) {
        printf("copy package error\n");
        ret = ERR_BUF;
        goto __cleanup;
    }

    int send_ret = network_write(net_adapter_if->net_handle, net_pbuf);
    if (send_ret != 0) {
        printf("output package error %p\n", net_adapter_if->net_handle);
        ret = ERR_BUF;
        goto __cleanup;
    }

    return ERR_OK;

__cleanup:
    if (net_pbuf->payload != NULL) {
        vPortFree(net_pbuf->payload);
        net_pbuf->payload = NULL;
    }

    if (net_pbuf != NULL) {
        vPortFree(net_pbuf);
        net_pbuf = NULL;
    }
    return ret;
}

static void netif_status_event_handler(struct netif *netif)
{
	struct dhcp *dhcp_info = netif_dhcp_data(netif);
	if (dhcp_info->state == DHCP_STATE_BOUND) {
        if (netif->ip_addr.addr == 0) {
            return;
        }
        esp_event_post(NA_IP_EVENT_BASE, NA_IP_EVT_STA_GOT_IP, (void *)&netif->ip_addr, sizeof(uint32_t), portMAX_DELAY);
	}
}

static void netif_dhcps_cb(u8_t client_ip[4])
{
    esp_event_post(NA_IP_EVENT_BASE, NA_IP_EVT_AP_ASSIGNED_IP, (void *) client_ip, 4, portMAX_DELAY);
}

static int convert_str_to_bytes(uint8_t *out, const char *s)
{
	int mac[MAC_LEN] = {0};
	int num_bytes = 0;

	if (!s || (strlen(s) < MIN_MAC_STRING_LEN))  {
		return -1;
	}

	num_bytes =  sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x",
			&mac[0],&mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	if ((num_bytes < MAC_LEN)  ||
		(mac[0] > 0xFF) ||
		(mac[1] > 0xFF) ||
		(mac[2] > 0xFF) ||
		(mac[3] > 0xFF) ||
		(mac[4] > 0xFF) ||
		(mac[5] > 0xFF)) {
		return -1;
	}

	out[0] = mac[0]&0xff;
	out[1] = mac[1]&0xff;
	out[2] = mac[2]&0xff;
	out[3] = mac[3]&0xff;
	out[4] = mac[4]&0xff;
	out[5] = mac[5]&0xff;

	return 0;
}

void network_adapter_init(void)
{
    network_stack_init();
}
