/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_wifi

#include "csk6/priv/csk_wifi_priv.h"

#include <logging/log.h>
LOG_MODULE_DECLARE(csk_wifi, CONFIG_WIFI_LOG_LEVEL);


#define CTRL_CMD_DEFAULT_REQ() {                                                \
    .msg_type = CTRL_REQ,                                                       \
    .ctrl_resp_cb = NULL,                                                       \
    .cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT /*30 sec*/                     \
}

#define CLEANUP_APP_MSG(app_msg) do {                                           \
    if (app_msg) {                                                              \
        if (app_msg->free_buffer_handle) {                                      \
        if (app_msg->free_buffer_func) {                                        \
            app_msg->free_buffer_func(app_msg->free_buffer_handle);             \
            app_msg->free_buffer_handle = NULL;                                 \
        }                                                                       \
        }                                                                       \
        hosted_free(app_msg);                                                   \
    }                                                                           \
} while(0);

typedef enum {
    WIFI_COMMAND,
    WIFI_EVENT
} wifi_ctrl_msg_type_t;

typedef struct {
    const struct device *dev;
    wifi_ctrl_msg_type_t type;
    union {
        wifi_req_t req;
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
    csk_wifi_mode_t mode;
};

typedef struct {
    wifi_fsm_t state;
    const struct device *dev[CSK_WIFI_MODE_MAX];
} csk_wifi_object_t;
static csk_wifi_object_t s_csk_wifi_obj = {0};

/* TODO: Remove this macro */
#ifdef CONFIG_CSK_WIFI_STATION
static struct csk_wifi_data csk_wifi_sta_data = {0};
#endif

#ifdef CONFIG_CSK_WIFI_SOFTAP
static struct csk_wifi_data csk_wifi_ap_data = {0};
#endif

static hosted_event_callback_t s_driver_event_cb;

const char *WIFI_STA_SM_STRINGS[] = {
    FOREACH_WIFI_STA_SM(GENERATE_STRING)
};

const char *WLAN_EVENT_STRINGS[] = {
    FOREACH_WLAN_EVENT(GENERATE_STRING)
};

wifi_sta_state_machine_t wifi_obj_get_sm(void)
{
    wifi_sta_state_machine_t sm = WIFI_STA_SM_PRIMARY_GET(s_csk_wifi_obj.state);
    return sm;
}

static inline void wifi_obj_set_sm(wifi_sta_state_machine_t sm)
{
    WIFI_STA_SM_PRIMARY_SET(s_csk_wifi_obj.state, sm);
}

static inline int wifi_give_result(wifi_ret_msg_t *ret_msg, bool async_mode)
{
    int ret = 0;
    if (async_mode == false) {
    if (!k_msgq_num_free_get(&wifi_result_msgq)) {
            k_msgq_purge(&wifi_result_msgq); /* clean the message queue */
        }
        ret = k_msgq_put(&wifi_result_msgq, ret_msg, K_NO_WAIT);
    }
    return ret;
}

int wifi_take_result(wifi_ret_msg_t *ret_msg, k_timeout_t timeout)
{
    int ret = 0;
    bool async_mode = (K_TIMEOUT_EQ(timeout, K_NO_WAIT) ? true : false);
    if (async_mode == false) {
        ret = k_msgq_get(&wifi_result_msgq, ret_msg, timeout);
    }
    return ret;
}
static inline void wifi_clear_message(void)
{
    k_msgq_purge(&wifi_interactive_msgq);
}

int wifi_send_command(const struct device *dev, wifi_cmd_t command, k_timeout_t timeouts, bool async_mode)
{
    wifi_req_t req = {
        .cmd = command,
        .async_mode = async_mode,
    };

    csk_wifi_ctrl_msg_t msg = {
        .dev = dev,
        .type = WIFI_COMMAND,
        .req = req,
    };

    wifi_clear_message();
    return k_msgq_put(&wifi_interactive_msgq, &msg, timeouts);
}

static wifi_auth_mode_e wifi_get_drv_encryption_mode(csk_wifi_encryption_mode_t mode)
{
    wifi_auth_mode_e ret;
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

static csk_wifi_encryption_mode_t wifi_get_csk_encryption_mode(wifi_auth_mode_e mode)
{
    csk_wifi_encryption_mode_t ret;
    switch (mode) {
    case WIFI_AUTH_OPEN:
        ret = CSK_WIFI_AUTH_OPEN;
        break;
    case WIFI_AUTH_WEP:
        ret = CSK_WIFI_AUTH_WEP;
        break;
    case WIFI_AUTH_WPA_PSK:
        ret = CSK_WIFI_AUTH_WPA_PSK;
        break;
    case WIFI_AUTH_WPA2_PSK:
        ret = CSK_WIFI_AUTH_WPA2_PSK;
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ret = CSK_WIFI_AUTH_WPA_WPA2_PSK;
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ret = CSK_WIFI_AUTH_WPA2_ENTERPRISE;
        break;

    case WIFI_AUTH_WPA3_PSK:
        ret = CSK_WIFI_AUTH_WPA3_PSK;
        break;

    case WIFI_AUTH_WPA2_WPA3_PSK:
        ret = CSK_WIFI_AUTH_WPA2_WPA3_PSK;
        break;

    default:
        ret = CSK_WIFI_AUTH_MAX;  //Not support
        break;
    }
    return ret;
}

static wifi_mode_e wifi_get_drv_wifi_mode(csk_wifi_mode_t mode)
{
    if (mode == CSK_WIFI_MODE_STA) {
        return WIFI_MODE_STA;
    } else if (mode == CSK_WIFI_MODE_AP) {
        return WIFI_MODE_AP;
    } else {
        return WIFI_MODE_MAX;
    }
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

const struct device * wifi_obj_get_dev(csk_wifi_mode_t mode)
{
    if (mode == CSK_WIFI_MODE_STA) {
    #ifdef CONFIG_CSK_WIFI_STATION
        return csk_wifi_sta_data.dev;
    #else
        return NULL;
    #endif
    } else if (mode == CSK_WIFI_MODE_AP) {
    #ifdef CONFIG_CSK_WIFI_SOFTAP
        return csk_wifi_ap_data.dev;
    #else 
        return NULL;
    #endif
    } else {
        return NULL;
    }
}



static void hosted_event_handler(hosted_event_t events, void *arg)
{
    if (events & HOSTED_LINK_EVT_ACTIVE) {
        k_sem_give(&init_sem);
    } else if (events & HOSTED_WIFI_EVT_STA_DISCONNECTED) {
        wifi_send_event(wifi_obj_get_dev(CSK_WIFI_MODE_STA), CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0, K_FOREVER);
        wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();
        if (primary_state == WIFI_SM_STA_CONNECTED || 
            primary_state == WIFI_SM_STA_SCAN_DONE) {
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0);
        } else {
            LOG_WRN("Got disconnected, drop");
        }
    } else if (events & HOSTED_WIFI_EVT_AP_STACONNECTED) {
        wifi_send_event(wifi_obj_get_dev(CSK_WIFI_MODE_AP), CSK_WIFI_EVT_AP_STACONNECTED, NULL, 0, K_FOREVER);
    } else if (events & HOSTED_WIFI_EVT_AP_STADISCONNECTED) {
        wifi_send_event(wifi_obj_get_dev(CSK_WIFI_MODE_AP), CSK_WIFI_EVT_AP_STADISCONNECTED, NULL, 0, K_FOREVER);
    } else {
        abort();
    }
}

static int wifi_driver_init(void)
{
    wifi_obj_set_sm(WIFI_SM_ON_STARTUP);
    s_driver_event_cb.events = HOSTED_WIFI_EVT_STA_DISCONNECTED |
                                HOSTED_WIFI_EVT_AP_STACONNECTED |
                                HOSTED_WIFI_EVT_AP_STADISCONNECTED |
                                HOSTED_LINK_EVT_ACTIVE;
    s_driver_event_cb.handler = &hosted_event_handler;
    hosted_add_callback(&s_driver_event_cb);
    hosted_init();
    k_sem_take(&init_sem, K_FOREVER);
    wifi_obj_set_sm(WIFI_SM_STARTUP_DONE);
    return 0;
}

static void wifi_virtual_netif_init(const struct device *dev)
{
    ctrl_cmd_t *resp = NULL;
    ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
    const struct csk_wifi_config *config = dev->config;
    struct csk_wifi_data *data = dev->data;
    wifi_mode_e esp_wifi_mode = wifi_get_drv_wifi_mode(config->mode);
    if ((esp_wifi_mode != WIFI_MODE_STA) && (esp_wifi_mode != WIFI_MODE_AP)) {
        LOG_ERR("wifi mode err, %d", config->mode);
        return;
    }
	req.u.wifi_mac.mode = esp_wifi_mode;
	resp = wifi_get_mac(req);
    if (resp == NULL) {
        return;
    }
    convert_str_to_bytes(data->mac_addr, resp->u.wifi_mac.mac);
    CLEANUP_APP_MSG(resp);
    if (config->mode == CSK_WIFI_MODE_STA) {
        data->virtual_netif = network_open(STA_INTERFACE, csk_wifi_receive_pkt, data);
    } else if (config->mode == CSK_WIFI_MODE_AP) {
        data->virtual_netif = network_open(SOFTAP_INTERFACE, csk_wifi_receive_pkt, data);
    } else {
        abort();
    }
    assert(data->virtual_netif);
}

static csk_wifi_result_t wifi_sta_connect(const struct device *dev)
{
    wifi_obj_set_sm(WIFI_SM_STA_ON_CONNECT);
	ctrl_cmd_t *resp = NULL;
    ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
    struct csk_wifi_data *data = dev->data;
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;

    strcpy((char *)&req.u.wifi_ap_config.ssid, data->sta_config.ssid);
	strcpy((char *)&req.u.wifi_ap_config.pwd, data->sta_config.pwd);
	strcpy((char *)&req.u.wifi_ap_config.bssid, data->sta_config.bssid);
	req.u.wifi_ap_config.is_wpa3_supported = false;
	req.u.wifi_ap_config.listen_interval = 1;

	resp = wifi_connect_ap(req);
    if (resp == NULL) {
        return CSK_WIFI_ERR_STA_FAILED;
    }
    if (resp->resp_event_status == 0) {
        wifi_obj_set_sm(WIFI_SM_STA_CONNECTED);
        data->sta_is_connected = true;
    } else if (resp->resp_event_status == CTRL_ERR_INVALID_PASSWORD) {
        ret = CSK_WIFI_ERR_INVALID_PASSWORD;
    } else if (resp->resp_event_status == CTRL_ERR_NO_AP_FOUND) {
        ret = CSK_WIFI_ERR_AP_NOT_FOUND;
    } else {
        ret = CSK_WIFI_ERR_STA_FAILED;
    }
    CLEANUP_APP_MSG(resp);
    return ret;
}

static csk_wifi_result_t wifi_sta_get_connected_info(const struct device *dev)
{
    ctrl_cmd_t *resp = NULL;
    ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
    struct csk_wifi_data *data = dev->data;
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;

    resp = wifi_get_ap_config(req);
    if (resp == NULL || resp->resp_event_status != 0) {
        CLEANUP_APP_MSG(resp);
        return CSK_WIFI_ERR_STA_FAILED;
    }

    wifi_ap_config_t *connected_ap = &resp->u.wifi_ap_config;
    strcpy(data->sta_config.bssid, connected_ap->bssid);
    data->sta_config.rssi = connected_ap->rssi;
    data->sta_config.channel = connected_ap->channel;
    CLEANUP_APP_MSG(resp);
    return ret;
}

static csk_wifi_result_t wifi_sta_disconnect(const struct device *dev)
{
    wifi_obj_set_sm(WIFI_SM_STA_ON_DISCONNECT);
	ctrl_cmd_t *resp = NULL;
    ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
    struct csk_wifi_data *data = dev->data;

    resp = wifi_disconnect_ap(req);
    if (resp == NULL || resp->resp_event_status != 0) {
        ret = CSK_WIFI_ERR_STA_FAILED;
    }

    wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
    data->sta_is_connected = false;
    CLEANUP_APP_MSG(resp);
    return ret;
}

static csk_wifi_result_t wifi_sotfap_start(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;
	ctrl_cmd_t *resp = NULL;
    ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();

    strncpy((char *)&req.u.wifi_softap_config.ssid, data->ap_config.ssid, SSID_LENGTH);
    strncpy((char *)&req.u.wifi_softap_config.pwd, data->ap_config.pwd, PASSWORD_LENGTH);
    req.u.wifi_softap_config.channel = data->ap_config.channel;
    req.u.wifi_softap_config.bandwidth = data->ap_config.bandwidth;
    req.u.wifi_softap_config.ssid_hidden = data->ap_config.ssid_hidden;
    req.u.wifi_softap_config.max_connections = data->ap_config.max_connections;
    req.u.wifi_softap_config.encryption_mode = wifi_get_drv_encryption_mode(data->ap_config.encryption_mode);
    resp = wifi_start_softap(req);
    if (resp == NULL || resp->resp_event_status != 0) {
        ret = CSK_WIFI_ERR_AP_FAILED;
    }
    CLEANUP_APP_MSG(resp);
    return ret;
}

static csk_wifi_result_t wifi_sotfap_stop(const struct device *dev)
{
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;

	ctrl_cmd_t *resp = NULL;
    ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
	resp = wifi_stop_softap(req);
    if (resp == NULL || resp->resp_event_status != 0) {
        ret = CSK_WIFI_ERR_AP_FAILED;
    }
    CLEANUP_APP_MSG(resp);
    return ret;
}

static csk_wifi_result_t wifi_scan_list(int *ap_count, wifi_scan_list_t *aps)
{
    wifi_obj_set_sm(WIFI_SM_STA_ON_SCAN);
	ctrl_cmd_t *resp = NULL;
    ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
    csk_wifi_result_t ret = CSK_WIFI_SUCCESS;

    resp = wifi_ap_scan_list(req);
    if (resp == NULL || resp->resp_event_status != 0) {
        return CSK_WIFI_ERR_STA_FAILED;
    }

    wifi_ap_scan_list_t *scan_list = &resp->u.wifi_ap_scan;
    aps->count = scan_list->count;
    aps->ap_list = k_calloc(aps->count, sizeof(csk_wifi_scan_info_t));
    if (aps->ap_list == NULL) {
        printk("No memory\n");
        return CSK_WIFI_ERR_STA_FAILED;
    }
    for (int idx = 0; idx < scan_list->count; idx++) {
        strncpy(aps->ap_list[idx].ssid, scan_list->out_list[idx].ssid, CSK_WIFI_SSID_LEN);
        strncpy(aps->ap_list[idx].bssid, scan_list->out_list[idx].bssid, CSK_WIFI_BSSID_LEN);
        aps->ap_list[idx].channel = scan_list->out_list[idx].channel;
        aps->ap_list[idx].rssi = scan_list->out_list[idx].rssi;
        aps->ap_list[idx].encryption_mode = wifi_get_csk_encryption_mode(
                                                scan_list->out_list[idx].encryption_mode);
    }
    wifi_obj_set_sm(WIFI_SM_STA_SCAN_DONE);
    CLEANUP_APP_MSG(resp);
    return ret;
}

static void wifi_command_handler(const struct device *dev,  wifi_req_t req)
{
    wifi_cmd_t command = req.cmd;
    wifi_ret_msg_t ret_msg = {
        .arg = NULL,
        .result = CSK_WIFI_SUCCESS,
    };

    switch (command) {
        case WIFI_CMD_INIT: {
            wifi_driver_init();
#ifdef CONFIG_CSK_WIFI_STATION
            wifi_virtual_netif_init(wifi_obj_get_dev(CSK_WIFI_MODE_STA));
#endif
#ifdef CONFIG_CSK_WIFI_SOFTAP
            wifi_virtual_netif_init(wifi_obj_get_dev(CSK_WIFI_MODE_AP));
#endif
            wifi_give_result(&ret_msg, req.async_mode);
        } break;
        case WIFI_CMD_STA_CONNECT: {
            //Add event 'connecting'.
            struct csk_wifi_data *data = dev->data;
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTING,
                                    &data->sta_config, sizeof(data->sta_config));
            ret_msg.result = wifi_sta_connect(dev);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                csk_wifi_result_t get_ret = wifi_sta_get_connected_info(dev);
                if (get_ret == CSK_WIFI_SUCCESS) {
                    wifi_send_event(dev, CSK_WIFI_EVT_STA_CONNECTED, NULL, 0, K_FOREVER);
                    wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTED, &data->sta_config, sizeof(csk_wifi_sta_config_t));
                }
            } else {
                wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTION_FAILED, &(ret_msg.result), sizeof(ret_msg.result));
            }
            wifi_give_result(&ret_msg, req.async_mode);
        } break;
        case WIFI_CMD_STA_DISCONNECT: {
            ret_msg.result = wifi_sta_disconnect(dev);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                wifi_send_event(dev, CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0, K_FOREVER);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0);
            }
            wifi_give_result(&ret_msg, req.async_mode);
        } break;

        case WIFI_CMD_AP_START: {
            ret_msg.result = wifi_sotfap_start(dev);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                wifi_send_event(dev, CSK_WIFI_EVT_AP_STARTED, NULL, 0, K_FOREVER);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_AP_STARTED, NULL, 0);
            }
            wifi_give_result(&ret_msg, req.async_mode);
        } break;

        case WIFI_CMD_AP_STOP: {
            ret_msg.result = wifi_sotfap_stop(dev);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                wifi_send_event(dev, CSK_WIFI_EVT_AP_STOPPED, NULL, 0, K_FOREVER);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_AP_STOPPED, NULL, 0);
            }
            wifi_give_result(&ret_msg, req.async_mode);
        } break;

        case WIFI_CMD_SCAN: {
            wifi_scan_list_t scan_list = {0};
            int count = 0;
            ret_msg.result = wifi_scan_list(&count, &scan_list);
            if (ret_msg.result == CSK_WIFI_SUCCESS) {
                ret_msg.arg = &scan_list;
                LOG_DBG("wifi_scan_list scan_list.count = %d", scan_list.count);
                wifi_dispatch_callbacks(CSK_WIFI_EVT_SCAN_DONE, scan_list.ap_list, 
                    sizeof(csk_wifi_scan_info_t) * scan_list.count);
            }
            wifi_give_result(&ret_msg, req.async_mode);
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
    csk_wifi_ctrl_msg_t msg;
    while (1) {
        int ret = wifi_get_message(&msg, K_FOREVER);
        if (ret != 0) {
            continue;
        }
        const struct device *dev = msg.dev;
        if (msg.type == WIFI_COMMAND) {
            /* This function maybe will here block thread for a long time */
            wifi_command_handler(dev, msg.req);
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
            goto __cleanup;
        }

        if (net_pkt_write(pkt, wifi_rx_buf->payload, wifi_rx_buf->len) < 0) {
            goto __cleanup;
        }

        if (net_recv_data(netif, pkt) < 0) {
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
    struct csk_wifi_data *data = dev->data;
    data->netif = netif;
    net_if_flag_set(data->netif, NET_IF_NO_AUTO_START);
    net_if_set_link_addr(data->netif, data->mac_addr, 6, NET_LINK_ETHERNET);
    ethernet_init(data->netif);  //Must be init in here!!!!!!!!!!!
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

static int csk_eth_dev_init(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    data->dev = dev;
    return 0;
}

static const struct ethernet_api csk_eth_apis = {
	.iface_api.init	= csk_wifi_netif_init,
	.send = csk_wifi_send_pkt,
};

#ifdef CONFIG_CSK_WIFI_STATION
const struct csk_wifi_config csk_wifi_sta_config = {
    .mode = CSK_WIFI_MODE_STA,
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
    .mode = CSK_WIFI_MODE_AP
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
