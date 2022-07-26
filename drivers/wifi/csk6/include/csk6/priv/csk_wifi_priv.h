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

#define CSK_WIFI_CHECK(cond, ret_val) ({                                      \
    if (!(cond)) {                                                            \
        LOG_ERR("%s:%d Check failed", __FUNCTION__, __LINE__);                \
        return (ret_val);                                                     \
    }                                                                         \
})


#define CSK_WIFI_CHECK_RETURN(cond, ret_val, format, ...) ({                  \
    if (!(cond)) {                                                            \
        LOG_ERR("%s:%d Check failed", __FUNCTION__, __LINE__);                \
        LOG_ERR(format, ##__VA_ARGS__);                                       \
        return (ret_val);                                                     \
    }                                                                         \
})


#define CSK_WIFI_CHECK_GOTO(cond, label, format, ...) ({                      \
    if (!(cond)) {                                                            \
        LOG_ERR("%s:%d Check failed", __FUNCTION__, __LINE__);                \
        LOG_ERR(format, ##__VA_ARGS__);                                       \
        goto label;                                                           \
    }                                                                         \
})


#define FOREACH_WIFI_STA_SM(WIFI_SM)                                          \
        WIFI_SM(WIFI_SM_ON_STARTUP)                                           \
        WIFI_SM(WIFI_SM_STARTUP_DONE)                                         \
        WIFI_SM(WIFI_SM_STA_CONNECTED)                                        \
        WIFI_SM(WIFI_SM_STA_DISCONNECTED)                                     \
        WIFI_SM(WIFI_SM_STA_ON_CONNECT)                                       \
        WIFI_SM(WIFI_SM_STA_ON_CONNECT_SCAN)                                  \
        WIFI_SM(WIFI_SM_STA_ON_AUTH_EXCEPTION)                                \
        WIFI_SM(WIFI_SM_STA_ON_ASSOC_EXCEPTION)                               \
        WIFI_SM(WIFI_SM_STA_ON_HANDSHAKE_EXCEPTION)                           \
        WIFI_SM(WIFI_SM_ON_CMDTIMEOUT_EXCEPTION)                              \
        WIFI_SM(WIFI_SM_STA_ON_DISCONNECT)                                    \
        WIFI_SM(WIFI_SM_STA_ON_SCAN)                                          \
        WIFI_SM(WIFI_SM_STA_SCAN_DONE)                                        \

#define FOREACH_WLAN_EVENT(WLAN_EVENT)                                        \
        WLAN_EVENT(WLAN_EVENT_CONNECTED)                                      \
        WLAN_EVENT(WLAN_EVENT_DISCONNECTED)                                   \
        WLAN_EVENT(WLAN_EVENT_SCAN_SUCCESS)                                   \
        WLAN_EVENT(WLAN_EVENT_SCAN_FAILED)                                    \
        WLAN_EVENT(WLAN_EVENT_4WAY_HANDSHAKE_FAILED)                          \
        WLAN_EVENT(WLAN_EVENT_SSID_NOT_FOUND)                                 \
        WLAN_EVENT(WLAN_EVENT_AUTH_TIMEOUT)                                   \
        WLAN_EVENT(WLAN_EVENT_ASSOC_TIMEOUT)                                  \
        WLAN_EVENT(WLAN_EVENT_CONNECT_FAILED)                                 \
        WLAN_EVENT(WLAN_EVENT_CONNECTION_LOSS)                                \
        WLAN_EVENT(WLAN_EVENT_AUTH_FAILED)                                    \
        WLAN_EVENT(WLAN_EVENT_ASSOC_FAILED)                                   \
        WLAN_EVENT(WLAN_EVENT_DEV_HANG)                                       \
        WLAN_EVENT(WLAN_EVENT_AP_STA_CONNECTED)                               \
        WLAN_EVENT(WLAN_EVENT_AP_STA_DISCONNECTED)                            \
        WLAN_EVENT(WLAN_EVENT_REKEY_COMPLETE)                                 \
        WLAN_EVENT(WLAN_EVENT_FW_CMD_TIMEOUT)                                 \
        WLAN_EVENT(WLAN_EVENT_FW_STARTUP_DONE)                                \
        WLAN_EVENT(WLAN_EVENT_MAX)                                            \


#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef uint32_t wifi_fsm_t;

typedef enum {
    /** WIFI_SM_STA_ON_AUTH_EXCEPTION
     * 1. auth timeout --> auth failed --> disconnected
     * 2. auth failed --> disconnected
     * 3. auth failed --> handshake failed --> disconnected
     */

    /** WIFI_SM_STA_ON_ASSOC_EXCEPTION
     * 1. assoc failed --> auth failed --> disconnected
     */

    /**  WIFI_SM_STA_ON_HANDSHAKE_EXCEPTION
     * 1. handshake err --> disconnected
     */

    /**  WIFI_SM_ON_CMDTIMEOUT_EXCEPTION
     * 1. cmd timeout --> disconnected
     */
    FOREACH_WIFI_STA_SM(GENERATE_ENUM)
} wifi_sta_state_machine_t;

#define WIFI_STA_SM_PRIMARY_GET(a)      ((uint16_t)((a)))
#define WIFI_STA_SM_PRIMARY_SET(a, b)   do {                                        \
    const char *a_sm_str = WIFI_STA_SM_STRINGS[(a)];                                \
    const char *b_sm_str = WIFI_STA_SM_STRINGS[(b)];                                \
    LOG_DBG("(%d)STA_SM: current[%s], next[%s]", __LINE__, a_sm_str, b_sm_str);     \
    (a) = (b);                                                                      \
} while(0)

struct csk_wifi_data {
    const struct device *dev;
    void *virtual_netif;
    struct net_if *netif;
    uint8_t mac_addr[6];
    union {
        csk_wifi_sta_config_t sta_config;
        csk_wifi_softap_config_t ap_config;
    };
#ifdef CONFIG_CSK_WIFI_XR819S
    bool sta_is_connected;
    csk_wifi_result_t err;
#endif
};

typedef enum {
    CSK_WIFI_MODE_STA,
    CSK_WIFI_MODE_AP,
    CSK_WIFI_MODE_MAX,
} csk_wifi_mode_t;

typedef enum {
    WIFI_CMD_INIT,
    WIFI_CMD_DEINIT,
    WIFI_CMD_STA_INIT,
    WIFI_CMD_STA_CONNECT,
    WIFI_CMD_STA_DISCONNECT,
    WIFI_CMD_AP_INIT,
    WIFI_CMD_AP_START,
    WIFI_CMD_AP_STOP,
    WIFI_CMD_SCAN,
} wifi_cmd_t;

typedef struct {
    wifi_cmd_t cmd;
    bool async_mode;
} wifi_req_t;

typedef struct {
    int result;
    void *arg;
} wifi_ret_msg_t;

typedef struct {
    csk_wifi_scan_info_t *ap_list;
    int count;
} wifi_scan_list_t;

extern const char *WIFI_STA_SM_STRINGS[];

const struct device * wifi_obj_get_dev(csk_wifi_mode_t mode);
#ifdef CONFIG_XR819S
int wifi_send_command(const struct device *dev, wifi_cmd_t command, k_timeout_t timeouts, bool async_mode);
#else 
int wifi_send_command(const struct device *dev, wifi_cmd_t command, k_timeout_t timeouts);
#endif
int wifi_take_result(wifi_ret_msg_t *ret_msg, k_timeout_t timeout);

void wifi_dispatch_callbacks(csk_wifi_event_t events, void *event_data, uint32_t event_data_len);
#ifdef CONFIG_CSK_WIFI_XR819S
wifi_sta_state_machine_t wifi_obj_get_sm(void);
csk_wifi_encryption_mode_t wifi_get_encryption_mode(csk_wifi_encryption_mode_t mode);
const char *wlan_sm_str(wifi_sta_state_machine_t state);
#endif

#ifdef __cplusplus
}
#endif
