/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_wifi

#include "csk6/priv/csk_wifi_priv.h"

#include <logging/log.h>
LOG_MODULE_DECLARE(csk_wifi, CONFIG_WIFI_LOG_LEVEL);

#define WIFI_MAX_SCAN_NUM       CONFIG_MAX_SCAN_APS_NUM
#define WIFI_MAX_TIMEOUT_SEC    CONFIG_WLAN_API_TIMEOUT

#ifndef MACTOSTR
#define MACTOSTR(mac) (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#define EVENT_SUBTYPE(event)    ((uint16_t)((event) & 0xFFFF))

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
static struct csk_wifi_data csk_wifi_ap_data;
#endif

const char *WIFI_STA_SM_STRINGS[] = {
    FOREACH_WIFI_STA_SM(GENERATE_STRING)
};

const char *WLAN_EVENT_STRINGS[] = {
    FOREACH_WLAN_EVENT(GENERATE_STRING)
};

const struct device * wifi_obj_get_dev(csk_wifi_mode_t mode)
{
    return s_csk_wifi_obj.dev[mode];
}

wifi_sta_state_machine_t wifi_obj_get_sm(void)
{
    wifi_sta_state_machine_t sm = WIFI_STA_SM_PRIMARY_GET(s_csk_wifi_obj.state);
    return sm;
}

static inline void wifi_obj_set_sm(wifi_sta_state_machine_t sm)
{
    WIFI_STA_SM_PRIMARY_SET(s_csk_wifi_obj.state, sm);
}

static inline void wifi_reset_result(void)
{
    k_msgq_purge(&wifi_result_msgq);
}

static inline int wifi_give_result(wifi_ret_msg_t *ret_msg, bool async_mode)
{
    int ret = 0;
    if (async_mode == false) {
        if (!k_msgq_num_free_get(&wifi_result_msgq)) {
            wifi_reset_result(); /* clean the message queue */
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

int wlan_event_clear(void)
{
    k_msgq_purge(&xrwlan_msgq); /* clean the message queue */
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
    wifi_reset_result();
    return k_msgq_put(&wifi_interactive_msgq, &msg, timeouts);
}

csk_wifi_encryption_mode_t wifi_get_encryption_mode(csk_wifi_encryption_mode_t mode)
{
    return mode;
}

enum wlan_mode wifi_get_wlan_mode(csk_wifi_mode_t mode)
{
    if (mode == CSK_WIFI_MODE_STA) {
        return WLAN_MODE_STA;
    } else {
        return WLAN_MODE_MAX;
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

int wifi_sm_on_connect_scan_handler(const struct device *dev, wlan_event_t current_event)
{
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_ON_CONNECT_SCAN) {
        return -ENXIO;
    }

    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_SCAN_SUCCESS) {
        LOG_INF("STA_SM[%s]: scan success", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_ON_CONNECT);
    } else if (current_event == WLAN_EVENT_CONNECTED) {
        /* Second time connect request maybe will get connected event without "scan event" */
        wifi_obj_set_sm(WIFI_SM_STA_CONNECTED);
        wlan_event_give(&current_event);
    } else if (current_event == WLAN_EVENT_SCAN_FAILED) {
        LOG_ERR("STA_SM[%s]: scan failed", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
    } else if (current_event == WLAN_EVENT_AUTH_FAILED) {
        LOG_ERR("STA_SM[%s]: auth failed", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_ON_AUTH_EXCEPTION);
    } else if (current_event == WLAN_EVENT_SSID_NOT_FOUND) {
        /* Maybe startup scan and update wifi aps list will get a better performance */
        LOG_ERR("STA_SM[%s]: ssid not found", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        data->err = CSK_WIFI_ERR_AP_NOT_FOUND;
        wlan_event_give(&current_event);
    } else if (current_event == WLAN_EVENT_4WAY_HANDSHAKE_FAILED) {
        /* Not sure whether will go into here */
        LOG_ERR("STA_SM[%s]: handshake failed", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_ON_HANDSHAKE_EXCEPTION);
    } else {
        LOG_ERR("STA_SM[%s]: unknown failed", wifi_sta_sm_str);
    }

    return 0;
}

int wifi_sm_on_connect_handler(const struct device *dev, wlan_event_t current_event)
{
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_ON_CONNECT) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_CONNECTED) {
        LOG_INF("STA_SM[%s]: connected", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_CONNECTED);
        wlan_event_give(&current_event);
    } else if (current_event == WLAN_EVENT_SSID_NOT_FOUND) {
        LOG_ERR("STA_SM[%s]: ssid not found {%s}",wifi_sta_sm_str, wlan_event_str);
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        data->err = CSK_WIFI_ERR_AP_NOT_FOUND;
        wlan_event_give(&current_event);
    } else if (current_event == WLAN_EVENT_AUTH_TIMEOUT || current_event == WLAN_EVENT_AUTH_FAILED) {
        LOG_ERR("STA_SM[%s]: auth timeout or auth timeout failed", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_ON_AUTH_EXCEPTION);
    } else if (current_event == WLAN_EVENT_ASSOC_TIMEOUT || current_event == WLAN_EVENT_ASSOC_FAILED) {
        LOG_ERR("STA_SM[%s]: assoc timeout or assoc timeout failed", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_ON_ASSOC_EXCEPTION);
    } else if (current_event == WLAN_EVENT_4WAY_HANDSHAKE_FAILED) {
        LOG_ERR("STA_SM[%s]: handshake failed", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_ON_HANDSHAKE_EXCEPTION);
    } else if (current_event == WLAN_EVENT_DISCONNECTED) {
        LOG_ERR("STA_SM[%s]: disconnected wait for handshake err event,", wifi_sta_sm_str);
    } else {
        LOG_ERR("STA_SM[%s]: connect failed {%s}", wifi_sta_sm_str, wlan_event_str);
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
    }
    return 0;
}

int wifi_sm_on_auth_exception_handler(const struct device *dev, wlan_event_t current_event)
{
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_ON_AUTH_EXCEPTION) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_DISCONNECTED) {
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        data->err = CSK_WIFI_ERR_STA_FAILED;
        wlan_event_give(&current_event);
    } else if (current_event == WLAN_EVENT_4WAY_HANDSHAKE_FAILED) {
        LOG_ERR("STA_SM[%s]: handshake failed", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_ON_HANDSHAKE_EXCEPTION);
    } else {
        LOG_WRN("STA_SM[%s]: auth timeout or auth timeout failed, evt: %s",
                wifi_sta_sm_str, wlan_event_str);
    }
    return 0;
}

int wifi_sm_on_assoc_exception_handler(const struct device *dev, wlan_event_t current_event)
{
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_ON_ASSOC_EXCEPTION) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_DISCONNECTED) {
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        data->err = CSK_WIFI_ERR_STA_FAILED;
        wlan_event_give(&current_event);
    } else {
        LOG_WRN("STA_SM[%s]: assoc timeout or auth timeout failed, evt: %s",
                wifi_sta_sm_str, wlan_event_str);
    }
    return 0;
}

int wifi_sm_on_handshake_exception_handler(const struct device *dev, wlan_event_t current_event)
{
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_ON_HANDSHAKE_EXCEPTION) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_DISCONNECTED) {
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        data->err = CSK_WIFI_ERR_INVALID_PASSWORD;
        wlan_event_give(&current_event);
    } else {
        LOG_ERR("STA_SM[%s]:  failed, evt: %s",
                wifi_sta_sm_str, wlan_event_str);
    }
    return 0;
}

int wifi_sm_on_disconnect_handler(const struct device *dev, wlan_event_t current_event)
{
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_ON_DISCONNECT) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_DISCONNECTED) {
        LOG_INF("STA_SM[%s]: disconnected", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        wlan_event_give(&current_event);
    } else {
        LOG_ERR("STA_SM[%s]: disconnect failed {%s}", wifi_sta_sm_str, wlan_event_str);
        wifi_obj_set_sm(WIFI_SM_STA_CONNECTED);
    }
    return 0;
}

int wifi_sm_connected_handler(const struct device *dev, wlan_event_t current_event)
{
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_CONNECTED) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_DISCONNECTED) {
        LOG_INF("STA_SM[%s]: async disconnected", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        wifi_send_event(dev, CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0, K_FOREVER);
    } else if (current_event == WLAN_EVENT_CONNECTED ||
                current_event == WLAN_EVENT_REKEY_COMPLETE) {
        LOG_INF("STA_SM[%s]: async connected, rekey event trigger, {%s}",
                    wifi_sta_sm_str, wlan_event_str);
    } else {
        LOG_ERR("STA_SM[%s]: err event [%s]", wifi_sta_sm_str, wlan_event_str);
    }

    return 0;
}

int wifi_sm_disconnected_handler(const struct device *dev, wlan_event_t current_event)
{
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_DISCONNECTED) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    LOG_ERR("STA_SM[%s]: err event [%s]", wifi_sta_sm_str, wlan_event_str);

    return 0;
}

int wifi_sm_on_scan_handler(const struct device *dev, wlan_event_t current_event)
{
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_ON_SCAN) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_SCAN_SUCCESS) {
        LOG_INF("STA_SM[%s]: scan successfully", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_SCAN_DONE);
        wlan_event_give(&current_event);
    } else {
        LOG_ERR("STA_SM[%s]: scan failed {%s}", wifi_sta_sm_str, wlan_event_str);
        wifi_obj_set_sm(WIFI_SM_STA_SCAN_DONE);
        wlan_event_give(&current_event);
    }
    return 0;
}

int wifi_sm_scandone_handler(const struct device *dev, wlan_event_t current_event)
{
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_STA_SCAN_DONE) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];

    if (current_event == WLAN_EVENT_CONNECTED ||
        current_event == WLAN_EVENT_REKEY_COMPLETE) {
        LOG_INF("STA_SM[%s]: Rekey event trigger, {%s}", wifi_sta_sm_str, wlan_event_str);
        if (current_event == WLAN_EVENT_CONNECTED) {
            wifi_obj_set_sm(WIFI_SM_STA_CONNECTED);
        }
    } else if (current_event == WLAN_EVENT_DISCONNECTED) {
        LOG_INF("STA_SM[%s]: async disconnected", wifi_sta_sm_str);
        wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
        wifi_send_event(dev, CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0, K_FOREVER);
    } else {
        LOG_ERR("STA_SM[%s]: err event [%s]", wifi_sta_sm_str, wlan_event_str);
    }

    return 0;
}

int wifi_sm_on_cmdtimeout_handler(const struct device *dev, wlan_event_t current_event)
{
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_FW_CMD_TIMEOUT) {
        wifi_obj_set_sm(WIFI_SM_ON_CMDTIMEOUT_EXCEPTION);
        LOG_ERR("STA_SM[%s]: WiFi module Firmware exception ocurred: %s !!!", wifi_sta_sm_str, wlan_event_str);
        wifi_reset_result(); /* unblock all thread which blocked in csk_wifi_api */
        return 0;
    }

    if (primary_state != WIFI_SM_ON_CMDTIMEOUT_EXCEPTION) {
        return -ENXIO;
    }

    if (current_event == WLAN_EVENT_DISCONNECTED) {
        wifi_obj_set_sm(WIFI_SM_ON_STARTUP);
        LOG_WRN("Waiting for wifi module reset done");
    } else {
        LOG_WRN("SM[%s]: unknown event %s", wifi_sta_sm_str, wlan_event_str);
    }

    return 0;
}

int wifi_sm_on_startup_handler(const struct device *dev, wlan_event_t current_event)
{
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (primary_state != WIFI_SM_ON_STARTUP) {
        return -ENXIO;
    }

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (current_event == WLAN_EVENT_FW_STARTUP_DONE) {
        wifi_obj_set_sm(WIFI_SM_STARTUP_DONE);
        LOG_INF("WiFi module startup done");
    } else if (current_event == WLAN_EVENT_DISCONNECTED) {
        LOG_WRN("SM[%s]: Redundant event %s", wifi_sta_sm_str, wlan_event_str);
    } else {
        LOG_WRN("SM[%s]: unknown event %s", wifi_sta_sm_str, wlan_event_str);
    }
    return 0;
}

void wifi_sta_sm_handler(wlan_event_t current_event)
{
    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    if (current_event == WLAN_EVENT_DISCONNECTED) {
        data->sta_is_connected = false;
    } else if (current_event == WLAN_EVENT_CONNECTED) {
        data->sta_is_connected = true;
    }

    int ret = 0;
    ret = wifi_sm_on_cmdtimeout_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_startup_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_connect_scan_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_connect_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_auth_exception_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_assoc_exception_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_handshake_exception_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_disconnect_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_on_scan_handler(dev, current_event);
    if (ret == 0) return;

    ret = wifi_sm_connected_handler(dev, current_event);
    if (ret == 0) return;
    ret = wifi_sm_disconnected_handler(dev, current_event);
    if (ret == 0) return;

    ret = wifi_sm_scandone_handler(dev, current_event);
    if (ret == 0) return;

    const char *wlan_event_str = WLAN_EVENT_STRINGS[current_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    if (ret < 0) {
        LOG_ERR("STA_SM[%s]: sm handler don't hit any events {%s}", wifi_sta_sm_str, wlan_event_str);
    }
}

static void wlan_event_handler(uint32_t param0, uint32_t param1)
{
    wlan_event_t wlan_event = EVENT_SUBTYPE(param0);
    wifi_sta_state_machine_t primary_state = wifi_obj_get_sm();

    const char *wlan_event_str = WLAN_EVENT_STRINGS[wlan_event];
    const char *wifi_sta_sm_str = WIFI_STA_SM_STRINGS[primary_state];
    LOG_DBG("Got wlan event [%s], current state[%s]", wlan_event_str, wifi_sta_sm_str);
    wifi_sta_sm_handler(wlan_event);
}

static int wifi_driver_init(void)
{
    wlan_sys_init();
    wlan_attach(wlan_event_handler);
    return 0;
}

static int wifi_driver_deinit(void)
{
	wlan_detach();
    return 0;
}

static void wifi_virtual_netif_init(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    const struct csk_wifi_config *config = dev->config;
    enum wlan_mode wlan_mode = wifi_get_wlan_mode(config->mode);
    data->virtual_netif = wlan_netif_create(wlan_mode, csk_wifi_receive_pkt, data);
    wlan_start(data->virtual_netif);
    if (config->mode == CSK_WIFI_MODE_STA) {
        data->sta_is_connected = false;
        wlan_sta_set_autoconnect(0);
    }
    wlan_get_mac_addr(data->virtual_netif, data->mac_addr, sizeof(data->mac_addr));
}

static void wifi_virtual_netif_deinit(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    wlan_stop(data->virtual_netif);
	wlan_netif_delete(data->virtual_netif);
}

static int wifi_sta_connect(const struct device *dev)
{
    int ret = 0;
    wlan_event_t event;
    struct csk_wifi_data *data = dev->data;

    data->err = CSK_WIFI_SUCCESS;

    ret = wlan_sta_set((uint8_t *)data->sta_config.ssid,
                        strlen(data->sta_config.ssid), (uint8_t *)data->sta_config.pwd);
    CSK_WIFI_CHECK_RETURN(ret == 0, -EIO, "Driver unknown exception, ret: %d", ret);

    ret = wlan_sta_connect();
    CSK_WIFI_CHECK_RETURN(ret == 0, -EIO, "Driver unknown exception, ret: %d", ret);
    ret = wlan_event_take(&event, K_SECONDS(WIFI_MAX_TIMEOUT_SEC));
    CSK_WIFI_CHECK_RETURN(ret == 0, ret, "Fatal error, WiFi lib does not report any event, ret: %d", ret);

    const char *wlan_event_str = WLAN_EVENT_STRINGS[event];
    CSK_WIFI_CHECK_RETURN((event != WLAN_EVENT_CONNECTED) || (event != WLAN_EVENT_SSID_NOT_FOUND),
                            data->err, "WiFi connect failed, event: %s", wlan_event_str);

    if (event == WLAN_EVENT_CONNECTED) {
        return 0;
    } else if (event == WLAN_EVENT_SSID_NOT_FOUND) {
        ret = wlan_sta_disable();
        CSK_WIFI_CHECK_RETURN(ret == 0, -EIO, "Driver unknown exception, ret: %d", ret);
        LOG_ERR("WiFi AP ssid not found, take event: %s", wlan_event_str);
        return CSK_WIFI_ERR_AP_NOT_FOUND;
    } else if (event == WLAN_EVENT_DISCONNECTED) {
        LOG_ERR("WiFi connect failed, result: %d", data->err);
        return data->err;
    } else {
        LOG_ERR("Unknown event, %s", wlan_event_str);
        return -EIO;
    }
}

static int wifi_sta_get_connected_info(const struct device *dev)
{
    int ret = 0;
    wlan_sta_ap_t ap_info;
    wlan_sta_config_t config = {0};
    struct csk_wifi_data *data = dev->data;

    ret = wlan_sta_ap_info(&ap_info);
    CSK_WIFI_CHECK_RETURN(ret == 0, -EIO, "Driver unknown exception, ret: %d", ret);

    memcpy(data->sta_config.ssid, ap_info.ssid.ssid, ap_info.ssid.ssid_len);
    data->sta_config.ssid[ap_info.ssid.ssid_len] = '\0';
    config.field = WLAN_STA_FIELD_PSK;

    if (strlen(data->sta_config.pwd)) {
        /* When PSK mode is OPEN, this function will failed and return -1 */
        ret = wlan_sta_get_config(&config);
        CSK_WIFI_CHECK_RETURN(ret == 0, -EIO, "Driver unknown exception, ret: %d", ret);
        memcpy(data->sta_config.pwd, config.u.psk, CSK_WIFI_PWD_LEN);
    } else {
        memset(data->sta_config.pwd, '\0', CSK_WIFI_PWD_LEN);
    }

    data->sta_config.pwd[CSK_WIFI_PWD_LEN - 1] = '\0';
    sprintf(data->sta_config.bssid, MACSTR, MACTOSTR(ap_info.bssid));
    data->sta_config.rssi = -(ap_info.rssi * 0.5);
    data->sta_config.channel = ap_info.channel;

    return 0;
}

static int wifi_sta_disconnect(const struct device *dev)
{
    wlan_event_t event;

    int ret = wlan_sta_disconnect();
    CSK_WIFI_CHECK_RETURN(ret == 0, -EIO, "Driver unknown exception, ret: %d", ret);

    ret = wlan_event_take(&event, K_SECONDS(WIFI_MAX_TIMEOUT_SEC));
    CSK_WIFI_CHECK_RETURN(ret == 0, ret, "Fatal error, WiFi lib does not report any event, ret: %d", ret);
    const char *wlan_event_str = WLAN_EVENT_STRINGS[event];
    CSK_WIFI_CHECK_RETURN(event == WLAN_EVENT_DISCONNECTED, CSK_WIFI_ERR_STA_FAILED,
                            "Unknown event occured: %s", wlan_event_str);

    return 0;
}

static int wifi_scan_list(const struct device *dev, int *ap_count, csk_wifi_scan_info_t **ap_list)
{
    int ret = 0;
    wlan_event_t event;
    wlan_sta_scan_results_t scan_results;

    /* clear old events */
    wlan_event_clear();

    /**
     * -2 pending scan schedule -reject new request
     * -1 interface is disabled -reject new request
     */
    ret = wlan_sta_scan_once();
    CSK_WIFI_CHECK_RETURN(ret == 0, -EIO, "Driver unknown exception, ret: %d", ret);

    ret = wlan_event_take(&event, K_SECONDS(WIFI_MAX_TIMEOUT_SEC));
    CSK_WIFI_CHECK_RETURN(ret == 0, ret, "Fatal error, WiFi lib does not report any event, ret: %d", ret);
    const char *wlan_event_str = WLAN_EVENT_STRINGS[event];
    CSK_WIFI_CHECK_RETURN(event == WLAN_EVENT_SCAN_SUCCESS, CSK_WIFI_ERR_SCAN_FAILED,
                            "WiFi Scan failed: %s", wlan_event_str);

    scan_results.size = WIFI_MAX_SCAN_NUM;
    scan_results.ap = k_calloc(scan_results.size, sizeof(wlan_sta_ap_t));
    CSK_WIFI_CHECK_RETURN(scan_results.ap != NULL, -ENOMEM, "Insufficient memory");

    ret = wlan_sta_scan_result(&scan_results);
    CSK_WIFI_CHECK_GOTO(ret == 0, __driver_err, "Driver get scan result failed, %d", ret);

    *ap_count = scan_results.num;
    *ap_list = k_calloc(*ap_count, sizeof(csk_wifi_scan_info_t));
    CSK_WIFI_CHECK_GOTO(*ap_list != NULL, __nomem, "Insufficient memory");

    csk_wifi_scan_info_t *ap_list_ptr = *ap_list;
    for (int idx = 0; idx < *ap_count; idx++) {
        memcpy(ap_list_ptr[idx].ssid, scan_results.ap[idx].ssid.ssid, scan_results.ap[idx].ssid.ssid_len);
        ap_list_ptr[idx].ssid[scan_results.ap[idx].ssid.ssid_len] = '\0';
        sprintf(ap_list_ptr[idx].bssid, MACSTR, MACTOSTR(scan_results.ap[idx].bssid));
        ap_list_ptr[idx].channel = scan_results.ap[idx].channel;
        ap_list_ptr[idx].rssi = -(scan_results.ap[idx].rssi * 0.5);
    }
    k_free(scan_results.ap);
    scan_results.ap = NULL;
    return 0;

__nomem:
    if (scan_results.ap) {
        k_free(scan_results.ap);
        scan_results.ap = NULL;
    }
    return -ENOMEM;

__driver_err:
    if (scan_results.ap) {
        k_free(scan_results.ap);
        scan_results.ap = NULL;
    }
    return -EIO;
}

static void wifi_command_handler(const struct device *dev, wifi_req_t req)
{
    int ret = 0;
    wifi_cmd_t command = req.cmd;

    wifi_ret_msg_t ret_msg = {
        .arg = NULL,
        .result = 0,
    };
    struct csk_wifi_data *data = dev->data;

    switch (command) {
        case WIFI_CMD_INIT: {
            wifi_obj_set_sm(WIFI_SM_ON_STARTUP); /* Fixme: remove the sta dependentce */
            ret = wifi_driver_init();
            if (ret != 0)   break;
            const struct device *sta_dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
            wifi_virtual_netif_init(sta_dev);
        } break;

        case WIFI_CMD_DEINIT: {
            wifi_obj_set_sm(WIFI_SM_ON_STARTUP); /* Fixme: remove the sta dependentce */
            const struct device *sta_dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
            wifi_virtual_netif_deinit(sta_dev);
            ret = wifi_driver_deinit();
            if (ret != 0)   break;
        } break;

        case WIFI_CMD_STA_CONNECT: {
            wifi_obj_set_sm(WIFI_SM_STA_ON_CONNECT_SCAN);
            ret = wifi_sta_connect(dev);
            if (ret == -EAGAIN) {
                wifi_obj_set_sm(WIFI_SM_STA_DISCONNECTED);
                break;
            } else if (ret != 0) {
                break;
            } else {
                wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTING,
                                        &data->sta_config, sizeof(data->sta_config));
            }
            ret = wifi_sta_get_connected_info(dev);
            if (ret != 0)   break;
            net_if_up(data->netif);
            net_eth_carrier_on(data->netif);
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTED,
                                    &data->sta_config, sizeof(data->sta_config));
        } break;

        case WIFI_CMD_STA_DISCONNECT: {
            wifi_obj_set_sm(WIFI_SM_STA_ON_DISCONNECT);
            ret = wifi_sta_disconnect(dev);
            if (ret == -EAGAIN) {
                wifi_obj_set_sm(WIFI_SM_STA_CONNECTED);
                break;
            } else if (ret != 0) {
                break;
            } else {
            }
            net_eth_carrier_off(data->netif);
            net_if_down(data->netif);
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0);
        } break;

        case WIFI_CMD_SCAN: {
            int aps_num = 0;
            csk_wifi_scan_info_t *ap_list = NULL;
            wifi_obj_set_sm(WIFI_SM_STA_ON_SCAN);
            ret = wifi_scan_list(NULL, &aps_num, &ap_list);
            if (ret == -EAGAIN) {
                wifi_obj_set_sm(WIFI_SM_STA_SCAN_DONE);
                break;
            }
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
    wifi_give_result(&ret_msg, req.async_mode);
}

static void wifi_async_event_handler(const struct device *dev, csk_wifi_event_t event)
{
    struct csk_wifi_data *data = dev->data;
    switch (event) {
        case CSK_WIFI_EVT_STA_CONNECTED: {
            net_if_up(data->netif);
            net_eth_carrier_on(data->netif);
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_CONNECTED,
                                    &data->sta_config, sizeof(data->sta_config));
        } break;

        case CSK_WIFI_EVT_STA_DISCONNECTED: {
            net_eth_carrier_off(data->netif);
            net_if_down(data->netif);
            wifi_dispatch_callbacks(CSK_WIFI_EVT_STA_DISCONNECTED, NULL, 0);
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
            wifi_command_handler(dev, msg.req);
        } else if (msg.type == WIFI_EVENT) {
            wifi_async_event_handler(dev, msg.event);
        }
    }
}

static void csk_wifi_receive_pkt(struct mbuf *mb, uint16_t len, void *arg)
{
    struct csk_wifi_data *data = arg;
    struct net_if* netif = data->netif;
    
    struct net_pkt* pkt = NULL;
    if (mb) {
        pkt = net_pkt_rx_alloc_with_buffer(netif, len, AF_UNSPEC, 0, K_MSEC(CONFIG_NET_RX_ALLOC_TIMEOUT));
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

    struct mbuf *m = mb_get(pkt_len, 1 | MBUF_GET_FLAG_LIMIT_TX);
    if (m == NULL) {
        ret = -ENOMEM;
        goto __cleanup;
    }
    uint8_t *m_data = mtod(m, uint8_t *);
    if (net_pkt_read(pkt, m_data, pkt_len) < 0) {
        ret = -EIO;
        goto __cleanup;
    }

    ret = wlan_linkoutput(data->virtual_netif, m);
    if (ret != 0) {
        goto __cleanup;
    }
    return 0;

__cleanup:
    net_pkt_unref(pkt);
    return ret;
}

static int csk_eth_dev_init(const struct device *dev)
{
    struct csk_wifi_data *data = dev->data;
    const struct csk_wifi_config *config = dev->config;
    data->dev = dev;
    s_csk_wifi_obj.dev[config->mode] = dev;
    // data->status = 0;
    k_sem_give(&init_sem);
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
#error "xr819s WiFi softAP is not available now!"
#endif
