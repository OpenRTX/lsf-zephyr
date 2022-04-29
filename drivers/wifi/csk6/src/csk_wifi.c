/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_wifi

#include "csk6/priv/csk_wifi_priv.h"

#define MAC_BYTES_LEN                     6
#define MAC_STRING_LEN                    18

typedef enum {
    WIFI_COMMAND,
    WIFI_EVENT
} wifi_ctrl_msg_type_t;

typedef struct {
    const struct device *dev;
    wifi_ctrl_msg_type_t type;
    union {
        wifi_cmd_t command;
        csk_wifi_event_t event;
    };
    void *data;
    uint32_t data_len;
} csk_wifi_ctrl_msg_t;

K_MSGQ_DEFINE(wifi_result_msgq, sizeof(wifi_ret_msg_t), 1, 4);
K_MSGQ_DEFINE(wifi_interactive_msgq, sizeof(csk_wifi_ctrl_msg_t), 4, 4);

static void wifi_control_thread(void *, void *, void *);

K_SEM_DEFINE(init_sem, 0, 1);
K_THREAD_DEFINE(wifi_ctrl_thread_id, CONFIG_WIFI_CTRL_STACK_SIZE,
                wifi_control_thread, NULL, NULL, NULL,
                CONFIG_WIFI_CTRL_PRIORITY, 0, 0);

static void wifi_virtual_netif_init(const struct device *dev);
static void csk_wifi_receive_pkt(struct network_handle *virtual_netif, void *arg);

struct csk_wifi_config {
    wifi_mode_t mode;
};

/* TODO: Remove this macro */
#ifdef CONFIG_CSK_WIFI_STATION
static struct csk_wifi_data csk_wifi_sta_data;
#endif

#ifdef CONFIG_CSK_WIFI_SOFTAP
static struct csk_wifi_data csk_wifi_ap_data;
#endif

static inline int wifi_give_result(wifi_ret_msg_t *ret_msg)
{
    if (!k_msgq_num_free_get(&wifi_result_msgq)) {
        k_msgq_purge(&wifi_result_msgq); /* clean the message queue */
    }
    return k_msgq_put(&wifi_result_msgq, ret_msg, K_NO_WAIT);
}

int wifi_take_result(wifi_ret_msg_t *ret_msg, k_timeout_t timeout)
{
    return k_msgq_get(&wifi_result_msgq, ret_msg, timeout);
}

int wifi_send_command(const struct device *dev, wifi_cmd_t command, k_timeout_t timeouts)
{
    csk_wifi_ctrl_msg_t msg = {
        .dev = dev,
        .type = WIFI_COMMAND,
        .command = command,
    };
    return k_msgq_put(&wifi_interactive_msgq, &msg, timeouts);
}

wifi_auth_mode_t wifi_get_encryption_mode(csk_wifi_encryption_mode_t mode)
{
    wifi_auth_mode_t ret;
    switch (mode) {
    case CSK_WIFI_AUTH_OPEN:
        ret = WIFI_AUTH_OPEN;
        break;
    case CSK_WIFI_AUTH_WEP:
        ret = WIFI_AUTH_WEP;
        break;
    case CSK_WIFI_AUTH_WPA_PSK:
        ret = WIFI_AUTH_WPA_PSK;
        break;
    case CSK_WIFI_AUTH_WPA2_PSK:
        ret = WIFI_AUTH_WPA2_PSK;
        break;
    case CSK_WIFI_AUTH_WPA_WPA2_PSK:
        ret = WIFI_AUTH_WPA_WPA2_PSK;
        break;
    case CSK_WIFI_AUTH_WPA2_ENTERPRISE:
        ret = WIFI_AUTH_WPA2_ENTERPRISE;
        break;

    case CSK_WIFI_AUTH_WPA3_PSK:
        ret = WIFI_AUTH_WPA3_PSK;
        break;

    case CSK_WIFI_AUTH_WPA2_WPA3_PSK:
        ret = WIFI_AUTH_WPA2_WPA3_PSK;
        break;

    default:
        ret = WIFI_AUTH_MAX;  //Not support
        break;
    }
    return ret;
}

static inline int wifi_send_event(const struct device *dev, csk_wifi_event_t event, void *event_data, uint32_t data_len, k_timeout_t timeouts)
{
    csk_wifi_ctrl_msg_t msg = {
        .dev = dev,
        .type = WIFI_EVENT,
        .event = event,
        .data = event_data,
        .data_len = data_len,
    };
    return k_msgq_put(&wifi_interactive_msgq, &msg, timeouts);
}

static inline int wifi_get_message(csk_wifi_ctrl_msg_t *msg, k_timeout_t timeouts)
{
    return k_msgq_get(&wifi_interactive_msgq, msg, timeouts);
}

const struct device * wifi_get_dev(wifi_mode_t mode)
{
    if (mode == WIFI_MODE_STA) {
    #ifdef CONFIG_CSK_WIFI_STATION
        return csk_wifi_sta_data.dev;
    #else
        return NULL;
    #endif
    } else if (mode == WIFI_MODE_AP) {
    #ifdef CONFIG_CSK_WIFI_SOFTAP
        return csk_wifi_ap_data.dev;
    #else 
        return NULL;
    #endif
    } else {
        return NULL;
    }
}

static int convert_str_to_bytes(uint8_t *out, const char *s)
{
    int mac[MAC_BYTES_LEN] = {0};
    int num_bytes = 0;

    if (!s || (strlen(s) < (MAC_STRING_LEN - 1)))  {
        return -1;
    }
    num_bytes =  sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x",
                        &mac[0],&mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    if ((num_bytes < MAC_BYTES_LEN)  ||
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

static void wifi_virtual_netif_init(const struct device *dev)
{
    const struct csk_wifi_config *config = dev->config;
    struct csk_wifi_data *data = dev->data;
    char mac_addr_strs[MAC_STRING_LEN] = {'\0'};
    wifi_get_mac(config->mode, mac_addr_strs);
    convert_str_to_bytes(data->mac_addr, mac_addr_strs);
    if (config->mode == WIFI_MODE_STA) {
        data->virtual_netif = network_open(STA_INTERFACE, csk_wifi_receive_pkt, data);
    } else if (config->mode == WIFI_MODE_AP) {
        data->virtual_netif = network_open(SOFTAP_INTERFACE, csk_wifi_receive_pkt, data);
    } else {
        abort();
    }
    assert(data->virtual_netif);
}

static csk_wifi_result_t wifi_sta_connect(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
    esp_hosted_control_config_t esp_wifi_config;
    memcpy(&esp_wifi_config.station, &data->sta_config, sizeof(esp_hosted_sta_config_t));
    int conn_ret = wifi_set_ap_config(esp_wifi_config);
    if (conn_ret == SUCCESS) {
        //do nothing
    } else if (conn_ret == INVALID_PASSWORD) {
        ret = CSK_WIFI_ERR_INVALID_PASSWORD;
    } else if (conn_ret == NO_AP_FOUND) {
        ret = CSK_WIFI_ERR_AP_NOT_FOUND;
    } else {
        ret = CSK_WIFI_ERR_STA_FAILED; //Fixme: esp32 bug, report invalid reason
    }
    return ret;
}

static csk_wifi_result_t wifi_sta_get_connected_info(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
    
    esp_hosted_control_config_t connected_info;
    int get_ret = wifi_get_ap_config(&connected_info);
    if (get_ret != 0) {
        ret = CSK_WIFI_ERR_STA_FAILED;
    }
    memcpy(&data->sta_config, &connected_info.station, sizeof(esp_hosted_sta_config_t));
    return ret;
}

static csk_wifi_result_t wifi_sta_disconnect(const struct device *dev)
{
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
    int disconn_ret = wifi_disconnect_ap();
    if (disconn_ret == SUCCESS) {
        //do nothing
    } else {
        ret = CSK_WIFI_ERR_STA_FAILED;
    }
    return ret;
}

static csk_wifi_result_t wifi_sotfap_start(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
    esp_hosted_control_config_t ap_config;
    memcpy(&ap_config.softap, &data->ap_config, sizeof(esp_hosted_softap_config_t));
    int start_ret = wifi_set_softap_config(ap_config);
    if (start_ret == SUCCESS) {
        //do nothing
    } else {
        ret = CSK_WIFI_ERR_AP_FAILED;
    }
    return ret;
}

static csk_wifi_result_t wifi_sotfap_stop(const struct device *dev)
{
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
    int stop_ret = wifi_stop_softap();
    if (stop_ret == SUCCESS) {
        //do nothing
    } else {
        ret = CSK_WIFI_ERR_AP_FAILED;
    }
    return ret;
}

static csk_wifi_result_t wifi_scan_list(int *ap_count, esp_hosted_wifi_scanlist_t **ap_list)
{
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
    *ap_list = wifi_ap_scan_list(ap_count);
    if (ap_list == NULL || *ap_count == 0) {
        ret = CSK_WIFI_ERR_STA_FAILED; //TODO: use more reasonable FAILED value
    }
    return ret;
}

static void wifi_command_handler(const struct device *dev, wifi_cmd_t command)
{
    wifi_ret_msg_t ret_msg = {
        .arg = NULL,
        .result = CSK_WIFI_SUCCESS,
    };

    switch (command) {
        case WIFI_CMD_AP_INIT:
        case WIFI_CMD_STA_INIT: {
            wifi_virtual_netif_init(dev);
        } break;
        case WIFI_CMD_STA_CONNECT: {
            ret_msg.result = wifi_sta_connect(dev);
            csk_wifi_result_t get_ret = wifi_sta_get_connected_info(dev);
            struct csk_wifi_data *data = dev->data;
            if (ret_msg.result == CSK_WIFI_SUCCESS && get_ret == CSK_WIFI_SUCCESS) {
                wifi_send_event(dev, CSK_WIFI_EVT_STA_CONNECTED, NULL, 0, K_FOREVER);
                csk_wifi_sta_config_t sta_config;
                memcpy(sta_config.ssid, data->sta_config.ssid, sizeof(sta_config.ssid));
                memcpy(sta_config.bssid, data->sta_config.bssid, sizeof(sta_config.bssid));
                sta_config.rssi = data->sta_config.rssi;
                sta_config.channel = data->sta_config.channel;
                sta_config.encryption_mode = wifi_get_encryption_mode(data->sta_config.encryption_mode);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTED, &sta_config, sizeof(csk_wifi_sta_config_t));
            }
            wifi_give_result(&ret_msg);
        } break;
        case WIFI_CMD_STA_DISCONNECT: {
            ret_msg.result = wifi_sta_disconnect(dev);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                wifi_send_event(dev, CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0, K_FOREVER);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0);
            }
            wifi_give_result(&ret_msg);
        } break;

        case WIFI_CMD_AP_START: {
            ret_msg.result = wifi_sotfap_start(dev);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                wifi_send_event(dev, CSK_WIFI_EVT_AP_STARTED, NULL, 0, K_FOREVER);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_AP_STARTED, NULL, 0);
            }
            wifi_give_result(&ret_msg);
        } break;

        case WIFI_CMD_AP_STOP: {
            ret_msg.result = wifi_sotfap_stop(dev);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                wifi_send_event(dev, CSK_WIFI_EVT_AP_STOPPED, NULL, 0, K_FOREVER);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_AP_STOPPED, NULL, 0);
            }
            wifi_give_result(&ret_msg);
        } break;

        case WIFI_CMD_SCAN: {
            wifi_scan_list_t scan_list = {0};
            int count = 0;
            esp_hosted_wifi_scanlist_t *ap_list = NULL;
            ret_msg.result = wifi_scan_list(&count, &ap_list);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                csk_wifi_scan_info_t *csk_ap_list = k_calloc(count, sizeof(csk_wifi_scan_info_t));
                if (csk_ap_list == NULL) {
                    abort();
                }
                for (int idx = 0; idx < count; idx++) {
                    memcpy(csk_ap_list[idx].ssid, ap_list[idx].ssid, sizeof(csk_ap_list[idx].ssid));
                    memcpy(csk_ap_list[idx].bssid, ap_list[idx].bssid, sizeof(csk_ap_list[idx].bssid));
                    csk_ap_list[idx].channel = ap_list[idx].channel;
                    csk_ap_list[idx].rssi = ap_list[idx].rssi;
                    csk_ap_list[idx].encryption_mode = wifi_get_encryption_mode(ap_list[idx].encryption_mode);
                }
                k_free(ap_list);
                ap_list = NULL;
                scan_list.ap_list = csk_ap_list;
                scan_list.count = count;
                ret_msg.arg = &scan_list;
                csk_wifi_scan_info_t csk_ap_list_copy[count];
                memset(csk_ap_list_copy, 0, sizeof(csk_wifi_scan_info_t) * count);
                memcpy(csk_ap_list_copy, csk_ap_list, sizeof(csk_wifi_scan_info_t) * count);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_SCAN_DONE, csk_ap_list_copy, sizeof(csk_wifi_scan_info_t) * count);
            }
            wifi_give_result(&ret_msg);
        } break;

        default:
            break;
    }
}

static void wifi_event_handler(const struct device *dev, csk_wifi_event_t event)
{
    struct csk_wifi_data *data = dev->data;
    switch (event) {
        case CSK_WIFI_EVT_STA_CONNECTED: {
            net_if_up(data->netif);
            net_eth_carrier_on(data->netif);
        } break;

        case CSK_WIFI_EVT_STA_DISCONNECTED: {
            net_eth_carrier_off(data->netif);
            net_if_down(data->netif);
        } break;

        case CSK_WIFI_EVT_AP_STARTED: {
            net_if_up(data->netif);
            net_eth_carrier_on(data->netif);
        } break;

        case CSK_WIFI_EVT_AP_STACONNECTED: {
            /* TODO */
        } break;

        case CSK_WIFI_EVT_AP_STADISCONNECTED: {
            /* TODO */
        } break;

        default:
            break;
    }
}

static void wifi_control_thread(void *arg1, void *arg2, void *arg3)
{
    k_sem_take(&init_sem, K_FOREVER);
    csk_wifi_ctrl_msg_t msg;
    while (1) {
        wifi_get_message(&msg, K_FOREVER);
        const struct device *dev = msg.dev;
        if (msg.type == WIFI_COMMAND) {
            /* This function maybe will here block thread for a long time */
            wifi_command_handler(dev, msg.command);
        } else if (msg.type == WIFI_EVENT) {
            wifi_event_handler(dev, msg.event);
        }
    }
}

static void csk_wifi_receive_pkt(struct network_handle *virtual_netif, void *arg)
{
    struct csk_wifi_data *data = arg;
    struct net_if* netif = data->netif;
    struct net_pkt* pkt = NULL;
    struct net_pbuf *wifi_rx_buf = network_read(virtual_netif, 0);
    if (wifi_rx_buf) {
        pkt = net_pkt_rx_alloc_with_buffer(netif, wifi_rx_buf->len,
                                           AF_UNSPEC, 0, K_NO_WAIT);
        if (pkt == NULL) {
            printk("Failed to get net buffer %d\n", wifi_rx_buf->len);
            goto __cleanup;
        }

        if (net_pkt_write(pkt, wifi_rx_buf->payload, wifi_rx_buf->len) < 0) {
            printk("Failed to write pkt\n");
            goto __cleanup;
        }

        if (net_recv_data(netif, pkt) < 0) {
            printk("Failed to push received data\n");
            goto __cleanup;
        }

        if (wifi_rx_buf->payload) {
            k_free(wifi_rx_buf->payload);
            wifi_rx_buf->payload = NULL;
        }
        if (wifi_rx_buf) {
            k_free(wifi_rx_buf);
            wifi_rx_buf = NULL;
        }
    }
    return;

__cleanup:
    if (wifi_rx_buf->payload) {
        k_free(wifi_rx_buf->payload);
        wifi_rx_buf->payload = NULL;
    }
    if (wifi_rx_buf) {
        k_free(wifi_rx_buf);
        wifi_rx_buf = NULL;
    }
    if (pkt) {
        net_pkt_unref(pkt);
    }
}

static void csk_wifi_netif_init(struct net_if *netif)
{
    const struct device *dev = net_if_get_device(netif);
    const struct csk_wifi_config *config = dev->config;
    struct csk_wifi_data *data = dev->data;
    data->netif = netif;
    net_if_flag_set(data->netif, NET_IF_NO_AUTO_START);
    net_if_set_link_addr(data->netif, data->mac_addr, 6, NET_LINK_ETHERNET);
    ethernet_init(data->netif);  //Must be init in here!!!!!!!!!!!
    if (config->mode == WIFI_MODE_STA) {
        if (wifi_send_command(dev, WIFI_CMD_STA_INIT, K_NO_WAIT) != 0) {
            abort();
        }
    } else if (config->mode == WIFI_MODE_AP) {
        if (wifi_send_command(dev, WIFI_CMD_AP_INIT, K_NO_WAIT) != 0) {
            abort();
        }
    } else {
        abort();
    }
}

static int csk_wifi_send_pkt(const struct device *dev, struct net_pkt *pkt)
{
    int ret = 0;
    struct csk_wifi_data *data = dev->data;
    const int pkt_len = net_pkt_get_len(pkt);
    struct net_pbuf *wifi_tx_buf = k_malloc(sizeof(struct net_pbuf));
    if (wifi_tx_buf == NULL) {
        return -ENOMEM;
    }
    wifi_tx_buf->len = pkt_len;
    wifi_tx_buf->payload = k_malloc(pkt_len);
    if (wifi_tx_buf->payload == NULL) {
        k_free(wifi_tx_buf);
        return -ENOMEM;
    }
    if (net_pkt_read(pkt, wifi_tx_buf->payload, pkt_len) < 0) {
        ret = -EIO;
        goto __cleanup;
    }
    int send_ret = network_write(data->virtual_netif, wifi_tx_buf);
    if (send_ret != 0) {
        ret = -EIO;
        goto __cleanup;
    }
    return 0;

__cleanup:
    if (wifi_tx_buf->payload) {
        k_free(wifi_tx_buf->payload);
        wifi_tx_buf->payload = NULL;
    }
    if (wifi_tx_buf) {
        k_free(wifi_tx_buf);
        wifi_tx_buf = NULL;
    }
    net_pkt_unref(pkt);
    return ret;
}

static void hosted_event_handler(hosted_event_t events, void *arg)
{
    const struct device *dev = arg;
    if (events & HOSTED_LINK_EVT_ACTIVE) {
        k_sem_give(&init_sem);
    } else if (events & HOSTED_WIFI_EVT_STA_DISCONNECTED) {
        wifi_send_event(dev, CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0, K_FOREVER);
    } else if (events & HOSTED_WIFI_EVT_AP_STACONNECTED) {
        wifi_send_event(dev, CSK_WIFI_EVT_AP_STACONNECTED, NULL, 0, K_FOREVER);
    } else if (events & HOSTED_WIFI_EVT_AP_STADISCONNECTED) {
        wifi_send_event(dev, CSK_WIFI_EVT_AP_STADISCONNECTED, NULL, 0, K_FOREVER);
    } else {
        abort();
    }
}

static int csk_eth_dev_init(const struct device *dev)
{
    const struct csk_wifi_config *config = dev->config;
    struct csk_wifi_data *data = dev->data;
    data->dev = dev;

    if (config->mode == WIFI_MODE_STA) {
        data->hosted_event_cb.events = HOSTED_WIFI_EVT_STA_DISCONNECTED;
    } else  if (config->mode == WIFI_MODE_AP) {
        data->hosted_event_cb.events = HOSTED_WIFI_EVT_AP_STACONNECTED |
                                       HOSTED_WIFI_EVT_AP_STADISCONNECTED;
    }
    data->hosted_event_cb.handler = &hosted_event_handler;
    if (!hosted_event_is_set(HOSTED_LINK_EVT_ACTIVE)) {
        data->hosted_event_cb.events |= HOSTED_LINK_EVT_ACTIVE; //Prevent this event be invoked twice
    }

    data->hosted_event_cb.arg = (void *)dev;
    hosted_add_callback(&data->hosted_event_cb);

    if (!hosted_is_inited()) {
        hosted_init();
    }
    return 0;
}

static const struct ethernet_api csk_eth_apis = {
	.iface_api.init	= csk_wifi_netif_init,
	.send = csk_wifi_send_pkt,
};

#ifdef CONFIG_CSK_WIFI_STATION
const struct csk_wifi_config csk_wifi_sta_config = {
    .mode = WIFI_MODE_STA,
};

NET_DEVICE_DT_INST_DEFINE(0,
                        csk_eth_dev_init,
                        NULL,
                        &csk_wifi_sta_data,
                        &csk_wifi_sta_config,
                        CONFIG_ETH_INIT_PRIORITY,
                        &csk_eth_apis,
                        ETHERNET_L2,
                        NET_L2_GET_CTX_TYPE(ETHERNET_L2),
                        NET_ETH_MTU);
#endif

#ifdef CONFIG_CSK_WIFI_SOFTAP
const struct csk_wifi_config csk_wifi_ap_config = {
    .mode = WIFI_MODE_AP
};

NET_DEVICE_DT_INST_DEFINE(1,
                        csk_eth_dev_init,
                        NULL,
                        &csk_wifi_ap_data,
                        &csk_wifi_ap_config,
                        CONFIG_ETH_INIT_PRIORITY,
                        &csk_eth_apis,
                        ETHERNET_L2,
                        NET_L2_GET_CTX_TYPE(ETHERNET_L2),
                        NET_ETH_MTU);
#endif
