/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_wifi

#include <fs/fs.h>

#include "csk6/priv/csk_wifi_priv.h"

#include <logging/log.h>
LOG_MODULE_DECLARE(csk_wifi, CONFIG_WIFI_LOG_LEVEL);

#define PARTITION_NODE DT_NODELABEL(lfs1)
FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);

#define WIFI_MAX_SCAN_NUM       CONFIG_MAX_SCAN_APS_NUM
#define WIFI_MAX_TIMEOUT_SEC    CONFIG_WLAN_API_TIMEOUT

#ifndef MACTOSTR
#define MACTOSTR(mac) (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

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

K_MSGQ_DEFINE(xrwlan_msgq, sizeof(uint32_t), 1, 4);
K_MSGQ_DEFINE(wifi_result_msgq, sizeof(wifi_ret_msg_t), 1, 4);
K_MSGQ_DEFINE(wifi_interactive_msgq, sizeof(csk_wifi_ctrl_msg_t), 4, 4);

static void wifi_control_thread(void *, void *, void *);

K_SEM_DEFINE(init_sem, 0, 1);
K_THREAD_DEFINE(wifi_ctrl_thread_id, CONFIG_WIFI_CTRL_STACK_SIZE,
                wifi_control_thread, NULL, NULL, NULL,
                CONFIG_WIFI_CTRL_PRIORITY, 0, 0);

static void csk_wifi_receive_pkt(struct mbuf *mb, uint16_t len, void *arg);

struct csk_wifi_config {
    int mode;
};

/* TODO: Remove this macro */
#ifdef CONFIG_CSK_WIFI_STATION
static struct csk_wifi_data csk_wifi_sta_data = {0};
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

int wlan_event_give(wlan_event_t *event)
{
    if (!k_msgq_num_free_get(&xrwlan_msgq)) {
        k_msgq_purge(&xrwlan_msgq); /* clean the message queue */
    }
    return k_msgq_put(&xrwlan_msgq, event, K_NO_WAIT);
}

int wlan_event_take(wlan_event_t *event, k_timeout_t timeout)
{
    return k_msgq_get(&xrwlan_msgq, event, timeout);
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

csk_wifi_encryption_mode_t wifi_get_encryption_mode(csk_wifi_encryption_mode_t mode)
{
    return mode;
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

const struct device * wifi_get_dev(int mode)
{
    if (mode == WLAN_MODE_STA) {
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

static void wlan_event_handler(uint32_t param0, uint32_t param1)
{
    wlan_event_t wifi_event = EVENT_SUBTYPE(param0);
    wlan_event_give(&wifi_event);
}

static int wifi_driver_init(const struct device *dev)
{
    int ret = 0;
    struct fs_mount_t *mp =&FS_FSTAB_ENTRY(PARTITION_NODE);
#if !DT_NODE_EXISTS(PARTITION_NODE) || !(FSTAB_ENTRY_DT_MOUNT_FLAGS(PARTITION_NODE) & FS_MOUNT_FLAG_AUTOMOUNT)
    ret = fs_mount(mp);
	if (ret < 0) {
        LOG_ERR("%s mount failed", mp->mnt_point);
		return ret;
	}
	LOG_INF("%s mounted\n", mp->mnt_point);
#else
    LOG_INF("%s automounted\n", mp->mnt_point);
#endif
    wlan_sys_init();
    wlan_attach(wlan_event_handler);

    return 0;
}

static void wifi_virtual_netif_init(const struct device *dev)
{
    const struct csk_wifi_config *config = dev->config;
    struct csk_wifi_data *data = dev->data;
    data->virtual_netif = wlan_netif_create(config->mode, csk_wifi_receive_pkt, data);
    wlan_start(data->virtual_netif);
    wlan_get_mac_addr(data->virtual_netif, data->mac_addr, sizeof(data->mac_addr));
}

static int wifi_sta_connect(const struct device *dev)
{
    int ret = 0;
    wlan_event_t event;
    struct csk_wifi_data *data = dev->data;

    wlan_sta_set((uint8_t *)data->sta_config.ssid, strlen(data->sta_config.ssid), (uint8_t *)data->sta_config.pwd);
    wlan_sta_enable();
    do {
        int wlan_ret = wlan_event_take(&event, K_SECONDS(WIFI_MAX_TIMEOUT_SEC));
        if (wlan_ret == -EAGAIN) {
            ret = wlan_ret;
            break;
        }
        if (event == WLAN_EVENT_CONNECTED) {
            LOG_INF("WiFi station connect successfully");
            return 0;
        } else if (event == WLAN_EVENT_SCAN_SUCCESS) {
            LOG_INF("WiFi Scan successfully");
            continue;
        }  else if (event == WLAN_EVENT_DISCONNECTED) {
            LOG_WRN("WiFi station unknown exception, event_code: %d", event);
            continue;
        } else if (event == WLAN_EVENT_SCAN_FAILED) {
            ret = CSK_WIFI_ERR_STA_FAILED;
            LOG_ERR("WiFi Scan failed, event_code: %d", event);
            break;
        } else if (event == WLAN_EVENT_SSID_NOT_FOUND) {
            ret = CSK_WIFI_ERR_AP_NOT_FOUND;
            LOG_ERR("WiFi AP ssid not found, event_code: %d", event);
            break;
        } else if (event == WLAN_EVENT_AUTH_TIMEOUT) {
            ret = CSK_WIFI_ERR_STA_FAILED;
            LOG_ERR("WiFi station authentication timeout, event_code: %d", event);
            break;
        } else if (event == WLAN_EVENT_ASSOC_TIMEOUT) {
            ret = CSK_WIFI_ERR_STA_FAILED;
            LOG_ERR("WiFi station association timeout, event_code: %d", event);
            break;
        } else if (event == WLAN_EVENT_ASSOC_FAILED) {
            ret = CSK_WIFI_ERR_STA_FAILED;
            LOG_ERR("WiFi station association failed, event_code: %d", event);
            break;
        } else if (event == WLAN_EVENT_4WAY_HANDSHAKE_FAILED) {
            ret = CSK_WIFI_ERR_INVALID_PASSWORD;
            LOG_ERR("WiFi station 4-way handshake failed, event_code: %d", event);
            break;
        } else {
            ret = CSK_WIFI_ERR_STA_FAILED;
            LOG_ERR("WiFi station unknown exception, event_code: %d", event);
            break;
        }
    } while (1); /* station connect request will trigger scan operation */

    return ret;
}

static int wifi_sta_get_connected_info(const struct device *dev)
{
    int ret = 0;
    wlan_sta_ap_t ap_info;
    wlan_sta_config_t config = {0};
    struct csk_wifi_data *data = dev->data;

    ret = wlan_sta_ap_info(&ap_info);
    if (ret != 0) {
        goto __failed;
    }
    memcpy(data->sta_config.ssid, ap_info.ssid.ssid, ap_info.ssid.ssid_len);
    data->sta_config.ssid[ap_info.ssid.ssid_len] = '\0';
    config.field = WLAN_STA_FIELD_PSK;
    ret = wlan_sta_get_config(&config);
    if (ret != 0) {
        goto __failed;
    }
    memcpy(data->sta_config.pwd, config.u.psk, CSK_WIFI_PWD_LEN);
    data->sta_config.pwd[CSK_WIFI_PWD_LEN - 1] = '\0';
    sprintf(data->sta_config.bssid, MACSTR, MACTOSTR(ap_info.bssid));
    data->sta_config.rssi = -(ap_info.rssi * 0.5);
    data->sta_config.channel = ap_info.channel;
    return 0;

__failed:
    LOG_ERR("WiFi station get connected info failed, ret %d\n", ret);
    return -EIO;
}

static csk_wifi_result_t wifi_sta_disconnect(const struct device *dev)
{
    wlan_event_t event;
    int ret = wlan_sta_disconnect();
    if (ret != 0) {
        goto __failed;
    }
    ret = wlan_event_take(&event, K_SECONDS(WIFI_MAX_TIMEOUT_SEC));
    if ((ret != 0) || (event != WLAN_EVENT_DISCONNECTED)) {
        goto __failed;
    }
    wlan_sta_disable();
    return ret;

__failed:
    LOG_ERR("WiFi station disconnecte from AP failed, ret %d\n", ret);
    return -EIO;
}

static int wifi_scan_list(int *ap_count, csk_wifi_scan_info_t **ap_list)
{
    int ret = 0;
    wlan_event_t event;
    wlan_sta_scan_results_t scan_results;

    int wlan_ret = wlan_sta_scan_once();
    if (wlan_ret != 0) {
        return CSK_WIFI_ERR_SCAN_FAILED;
    }
    ret = wlan_event_take(&event, K_SECONDS(WIFI_MAX_TIMEOUT_SEC));
    if (ret != 0) {
        return -EAGAIN;
    }
    if (event != WLAN_EVENT_SCAN_SUCCESS) {
        return CSK_WIFI_ERR_SCAN_FAILED;
    }

    scan_results.size = WIFI_MAX_SCAN_NUM;
    scan_results.ap = k_calloc(scan_results.size, sizeof(wlan_sta_ap_t));
    if (scan_results.ap == NULL) {
        LOG_ERR("Scan list no mem");
        return -ENOMEM;
    }
    ret = wlan_sta_scan_result(&scan_results);
    if (ret != 0) {
        ret = CSK_WIFI_ERR_SCAN_FAILED;
        goto __out;
    }
    *ap_count = scan_results.num;
    *ap_list = k_calloc(*ap_count, sizeof(csk_wifi_scan_info_t));
    if (*ap_list == NULL) {
        ret = CSK_WIFI_ERR_SCAN_FAILED;
        goto __out;
    }
    csk_wifi_scan_info_t *ap_list_ptr = *ap_list;
    for (int idx = 0; idx < *ap_count; idx++) {
        memcpy(ap_list_ptr[idx].ssid, scan_results.ap[idx].ssid.ssid, scan_results.ap[idx].ssid.ssid_len);
        ap_list_ptr[idx].ssid[scan_results.ap[idx].ssid.ssid_len] = '\0';
        sprintf(ap_list_ptr[idx].bssid, MACSTR, MACTOSTR(scan_results.ap[idx].bssid));
        ap_list_ptr[idx].channel = scan_results.ap[idx].channel;
        ap_list_ptr[idx].rssi = -(scan_results.ap[idx].rssi * 0.5);
    }

__out:
    if (scan_results.ap) {
        k_free(scan_results.ap);
        scan_results.ap = NULL;
    }
    return ret;
}

static void wifi_command_handler(const struct device *dev, wifi_cmd_t command)
{
    int ret = 0;
    wifi_ret_msg_t ret_msg = {
        .arg = NULL,
        .result = 0,
    };
    struct csk_wifi_data *data = dev->data;

    switch (command) {
        case WIFI_CMD_AP_INIT:
        case WIFI_CMD_STA_INIT: {
            ret = wifi_driver_init(dev);
            if (ret != 0)   break;
            wifi_virtual_netif_init(dev);
        } break;

        case WIFI_CMD_STA_CONNECT: {
            ret = wifi_sta_connect(dev);
            if (ret != 0)   break;
            ret = wifi_sta_get_connected_info(dev);
            if (ret != 0)   break;
            wifi_send_event(dev, CSK_WIFI_EVT_STA_CONNECTED, NULL, 0, K_FOREVER);
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTED,
                                    &data->sta_config, sizeof(data->sta_config));
        } break;

        case WIFI_CMD_STA_DISCONNECT: {
            ret = wifi_sta_disconnect(dev);
            if (ret != 0)   break;
            wifi_send_event(dev, CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0, K_FOREVER);
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0);
        } break;

        case WIFI_CMD_SCAN: {
            int aps_num = 0;
            csk_wifi_scan_info_t *ap_list = NULL;
            ret = wifi_scan_list(&aps_num, &ap_list);
            if (ret != 0)   break;
            wifi_scan_list_t scan_list = {0};
            scan_list.ap_list = ap_list;
            scan_list.count = aps_num;
            ret_msg.arg = &scan_list;
            csk_wifi_scan_info_t csk_ap_list_copy[aps_num];
            memset(csk_ap_list_copy, 0, sizeof(csk_wifi_scan_info_t) * aps_num);
            memcpy(csk_ap_list_copy, ap_list, sizeof(csk_wifi_scan_info_t) * aps_num);
            wifi_dispatch_callbacks(CSK_WIFI_EVT_SCAN_DONE,
                                    csk_ap_list_copy, sizeof(csk_wifi_scan_info_t) * aps_num);
        } break;

        default:
            break;
    }
    ret_msg.result = ret;
    wifi_give_result(&ret_msg);
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

static void csk_wifi_receive_pkt(struct mbuf *mb, uint16_t len, void *arg)
{
    struct csk_wifi_data *data = arg;
    struct net_if* netif = data->netif;
    
    struct net_pkt* pkt = NULL;
    if (mb) {
        pkt = net_pkt_rx_alloc_with_buffer(netif, len, AF_UNSPEC, 0, K_NO_WAIT);
        if (pkt == NULL) {
            goto __cleanup;
        }
        uint8_t *m_data = mtod(mb, uint8_t *);
        if (net_pkt_write(pkt, m_data, mb->m_len) < 0) {
            goto __cleanup;
        }
        int ret = net_recv_data(netif, pkt);
        if (ret < 0) {
            goto __cleanup;
        }
    }
    return;

__cleanup:
    if (pkt) {
        net_pkt_unref(pkt);
    }
    return;
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
    uint8_t *payload = k_malloc(pkt_len);
    if (payload == NULL) {
        return -ENOMEM;
    }

    if (net_pkt_read(pkt, payload, pkt_len) < 0) {
        ret = -EIO;
        goto __cleanup;
    }

    struct mbuf *m = mb_get(pkt_len, 1 | MBUF_GET_FLAG_LIMIT_TX);
    if (m == NULL) {
        ret = -ENOMEM;
        goto __cleanup;
    }
    uint8_t *m_data = mtod(m, uint8_t *);
    memcpy(m_data, payload, pkt_len);
    ret = wlan_linkoutput(data->virtual_netif, m);
    if (ret != 0) {
        goto __cleanup;
    }
    if (payload) {
        k_free(payload);
        payload = NULL;
    }
    return 0;

__cleanup:
    if (payload) {
        k_free(payload);
        payload = NULL;
    }
    net_pkt_unref(pkt);
    return ret;
}

static int csk_eth_dev_init(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    data->dev = dev;
    data->wifi_status = WLAN_EVENT_MAX;
    k_sem_give(&init_sem);
    return 0;
}

static const struct ethernet_api csk_eth_apis = {
	.iface_api.init	= csk_wifi_netif_init,
	.send = csk_wifi_send_pkt,
};

#ifdef CONFIG_CSK_WIFI_STATION
const struct csk_wifi_config csk_wifi_sta_config = {
    .mode = WLAN_MODE_STA,
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
#error "xr819s WiFi softAP is not available now!"
#endif
