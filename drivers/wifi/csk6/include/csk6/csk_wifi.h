/*
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define CSK_WIFI_SSID_LEN       32
#define CSK_WIFI_BSSID_LEN      17
#define CSK_WIFI_PWD_LEN        64

/* TODO: Add WIFI state machine transition diagram */

/**
 * @brief CSK WiFi encryption mode
 */
typedef enum {
    CSK_WIFI_AUTH_OPEN = 0,         /* Authenticate mode : open */
    CSK_WIFI_AUTH_WEP,              /* Authenticate mode : WEP */
    CSK_WIFI_AUTH_WPA_PSK,          /* Authenticate mode : WPA_PSK */
    CSK_WIFI_AUTH_WPA2_PSK,         /* Authenticate mode : WPA2_PSK */
    CSK_WIFI_AUTH_WPA_WPA2_PSK,     /* Authenticate mode : WPA_WPA2_PSK */
    CSK_WIFI_AUTH_WPA2_ENTERPRISE,  /* Authenticate mode : WPA2_ENTERPRISE */
    CSK_WIFI_AUTH_WPA3_PSK,         /* Authenticate mode : WPA3_PSK */
    CSK_WIFI_AUTH_WPA2_WPA3_PSK,    /* Authenticate mode : WPA2_WPA3_PSK */
    CSK_WIFI_AUTH_MAX ,             /* Authenticate mode : WAPI_PSK */
} csk_wifi_encryption_mode_t;

/**
 * @brief CSK WiFi bandwidth mode
 */
typedef enum {
	CSK_WIFI_BW_HT20 = 0,           /* Bandwidth is HT20(20M) */
	CSK_WIFI_BW_HT40,               /* Bandwidth is HT40(40M) */
} csk_wifi_bandwidth_t;

/**
 * @brief CSK WiFi station configuration struct
 */
typedef struct {
    char ssid[CSK_WIFI_SSID_LEN+1];                 /* Target AP SSID */
    char pwd[CSK_WIFI_PWD_LEN+1];                   /* Target AP Password */
    char bssid[CSK_WIFI_BSSID_LEN+1];               /* Target AP bssid (MAC address) */
    uint8_t channel;                                /* Target AP channel */
    int rssi;                                       /* Target AP rssi */
    csk_wifi_encryption_mode_t encryption_mode;     /* Target AP encryption mode */
} csk_wifi_sta_config_t;

/**
 * @brief CSK WiFi softap configuration struct
 */
typedef struct {
    char ssid[CSK_WIFI_SSID_LEN+1];                 /* WiFi SoftAP SSID */
    char pwd[CSK_WIFI_PWD_LEN+1];                   /* WiFi SoftAP Password */
    csk_wifi_encryption_mode_t encryption_mode;     /* WiFi SoftAP encryption mode */
    csk_wifi_bandwidth_t bandwidth;                 /* WiFi SoftAP bandwidth */
    uint8_t channel;                                /* WiFi SoftAP channel */
    uint8_t max_connections;                        /* Maximum number of stations allowed to connect in */
    bool ssid_hidden;                               /* SSID hidden */
} csk_wifi_softap_config_t;

/**
 * @brief CSK WiFi scan info type
 */
typedef struct {
    char ssid[CSK_WIFI_SSID_LEN+1];                 /* Neighboring AP SSID */
    char bssid[CSK_WIFI_BSSID_LEN+1];               /* Neighboring AP BSSID */
    int rssi;                                       /* Neighboring AP rssi */
    uint8_t channel;                                /* Neighboring AP channel */
    csk_wifi_encryption_mode_t encryption_mode;     /* Neighboring AP encryption mode */
} csk_wifi_scan_info_t;

/**
 * @brief CSK WiFi operation(API) results structure
 */
typedef enum {
    CSK_WIFI_SUCCESS,                   /* WIFI operation successful */
    CSK_WIFI_ERR_STA_FAILED,            /* Unknown Station exception, will be raised in Station mode. TODO: remove it */
    CSK_WIFI_ERR_AP_FAILED,             /* Unknown SoftAP exception, will be raised in SoftAP mode. TODO: remove it */
    CSK_WIFI_ERR_AP_NOT_FOUND,          /* AP ssid not found, will be raised in Station mode */
    CSK_WIFI_ERR_INVALID_PASSWORD,      /* Invalid WIFI password, will be raised in Station mode */
    CSK_WIFI_ERR_SCAN_FAILED,           /* WiFi scan neighboring AP devices failed */
} csk_wifi_result_t;                    /* WIFI API result type */

/**
 * @brief CSK WiFi events structure
 */
typedef enum {
    CSK_WIFI_EVT_STA_CONNECTED =        BIT(0),     /* WIFI-Station connected event bit */
    CSK_WIFI_EVT_STA_DISCONNECTED =     BIT(1),     /* WIFI-Station disconnected event bit */
    CSK_WIFI_EVT_AP_STARTED =           BIT(2),     /* WIFI-SoftAP stared event bit */
    CSK_WIFI_EVT_AP_STOPPED =           BIT(3),     /* WIFI-SoftAP stopped event bit */
    CSK_WIFI_EVT_AP_STACONNECTED =      BIT(4),     /* WIFI-SoftAP station connected event bit */
    CSK_WIFI_EVT_AP_STADISCONNECTED =   BIT(5),     /* WIFI-SoftAP station disconnected event bit */
    CSK_WIFI_EVT_SCAN_DONE =            BIT(6),     /* WIFI-SoftAP scan done event bit */
} csk_wifi_event_t;

/**
 * WIFI event handler type (will be invoked in `csk_wifi_event_cb_t`)
 */
typedef void(*wifi_event_cb_t) (csk_wifi_event_t event, void *event_data, uint32_t event_data_len, void *arg);

/**
 * @brief CSK WIFI callback structure
 *
 * Used to register a callback in the csk_wifi driver instance callback list.
 * As many callbacks as needed can be added as long as each of them
 * are unique pointers of struct csk_wifi_event_cb.
 * Beware such structure should not be allocated on stack.
 *
 * TODO: add a callback initializer
 */
typedef struct csk_wifi_event_cb {
    /**
     * This is meant to be used in the driver core and the user should not
	 * touch it (see csk_wifi_api.c).
	 */
    sys_snode_t node;

    /**
     * This function will be called while WIFI event occurred
     */
    wifi_event_cb_t handler;

    /**
     * Wifi events mask, see `csk_wifi_event_t`
     */
    uint32_t events;

    /**
     * Wifi event data
     */
    void *event_data;

    /**
     * Wifi event data len
     */
    uint32_t event_data_len;

    /**
     * user argument
     */
    void *arg;
} csk_wifi_event_cb_t;

/**
 * @brief CSK WiFi driver initialize
 * 
 * @return 0 if successful, negative errno code on failure.
 */
int csk_wifi_init();

/**
 * @brief Add an wifi event callback
 * 
 * @param wifi_event_cb[in] Application's callback structure pointer.
 * 
 * @return 0 if successful, negative errno code on failure.
 */
int csk_wifi_add_callback(csk_wifi_event_cb_t *wifi_event_cb);

/**
 * @brief Scan the neighboring AP device
 * 
 * @param ap_info[in/out] Pointer to WiFi AP information structure
 * @param result[out] Pointer to WIFI operation result
 * @param timeout[in] How long to try to connect WIFI Access Point
 * 
 * @return The number of AP imformation, negative errno code on failure.
 */
int csk_wifi_scan_ap(csk_wifi_scan_info_t **ap_info, csk_wifi_result_t *result, k_timeout_t timeout);

#ifdef CONFIG_CSK_WIFI_STATION
/**
 * @brief Start a WIFI connection request as a WiFi station device
 *
 * This function will send a WiFi station connect request to the driver core
 * and the WiFi driver thread will start the real WiFi connection request. When
 * the WiFi station connected, CSK_WIFI_EVT_STA_CONNECTED event will be triggered
 * and WiFi event handler will be called (if have).
 *
 * @note This function could only call while WIFI is in Station disconnected status
 *
 * @param sta_config[in/out] Pointer to WIFI Station configuration structure
 * @param result[out] Pointer to WIFI operation result
 * @param timeout[in] How long to try to connect WIFI Access Point
 *
 * @return 0 if successful, negative errno code on failure.
 */
int csk_wifi_sta_connect(csk_wifi_sta_config_t *sta_config, csk_wifi_result_t *result, k_timeout_t timeout);

/**
 * @brief Start a WIFI disconnection request
 *
 * This function will send a WiFi station disconnect request to the driver core
 * and the WiFi driver thread will start the real WiFi disconnection request. When
 * the WiFi station disconnected, CSK_WIFI_EVT_STA_DISCONNECTED event will be triggered
 * and WiFi event handler will be called (if have).
 *
 * @note This function could only call while WIFI is in Station connected status
 *
 * @param result[out] Pointer to WIFI operation result
 * @param timeout[in] How long to try to disconnect from WIFI Access Point
 *
 * @return 0 if successful, negative errno code on failure.
 */
int csk_wifi_sta_disconnect(csk_wifi_result_t *result, k_timeout_t timeout);
#endif

#ifdef CONFIG_CSK_WIFI_SOFTAP
/**
 * @brief Set up WIFI Access Point
 *
 * This function will send a WIFI Access Point start request to the driver core
 * and the WiFi driver thread will call the real WIFI SoftAP startup function. When
 * the WIFI Access Point startup, CSK_WIFI_EVT_AP_STARTED event will be triggered
 * and WiFi event handler will be called (if have).
 *
 * @note This function could only call while WIFI is in SoftAP stopped status
 *
 * @param ap_config[in] Pointer to WIFI SoftAP configuration structure
 * @param result[out] Pointer to operation result
 * @param timeout[in] How long to try to set up WIFI Access Point
 *
 * @return 0 if successful, negative errno code on failure.
 */
int csk_wifi_ap_start(const csk_wifi_softap_config_t *ap_config, csk_wifi_result_t *result, k_timeout_t timeout);

/**
 * @brief Stop WIFI Access Point
 *
 * This function will send a WIFI Access Point stop request to the driver core
 * and the WiFi driver thread will call the real WIFI SoftAP stop function. When
 * the WIFI Access Point stopped, CSK_WIFI_EVT_AP_STOPPED event will be triggered
 * and WiFi event handler will be called (if have).
 *
 * @note This function could only call while WIFI is in SoftAP started status
 *
 * @param result[out] Pointer to operation result
 * @param timeout[in] How long to try to stop WIFI Access Point
 *
 * @return 0 if successful, negative errno code on failure.
 */
int csk_wifi_ap_stop(csk_wifi_result_t *result, k_timeout_t timeout);
#endif

#ifdef __cplusplus
}
#endif
