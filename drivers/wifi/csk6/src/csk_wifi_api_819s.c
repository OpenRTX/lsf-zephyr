#include "csk6/priv/csk_wifi_priv.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk_wifi, CONFIG_WIFI_LOG_LEVEL);

#define CSK_WIFI_CHECK(cond, ret_val) ({                                      \
    if (!(cond)) {                                                            \
        LOG_ERR("%s(%d): Check failed", __FUNCTION__, __LINE__);              \
        return (ret_val);                                                     \
    }                                                                         \
})


K_MUTEX_DEFINE(wifi_api_mutex);

static sys_slist_t wifi_callback_list;



void wifi_dispatch_callbacks(csk_wifi_event_t events, void *event_data, uint32_t event_data_len)
{
    sys_slist_t *list = &wifi_callback_list;
    csk_wifi_event_cb_t *cb, *tmp;
    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node) {
        if (cb->events & events) {
            cb->handler(cb->events & events, event_data, event_data_len, cb->arg);
        }
    }
}

int csk_wifi_add_callback(csk_wifi_event_cb_t *wifi_event_cb)
{
    CSK_WIFI_CHECK(wifi_event_cb != NULL, -EINVAL);
    sys_slist_prepend(&wifi_callback_list, &wifi_event_cb->node);
    return 0;
}

int csk_wifi_init()
{
    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_get_dev(WLAN_MODE_STA); /* TODO: add wifi softAP support */

    ret = wifi_send_command(dev, WIFI_CMD_STA_INIT, K_NO_WAIT);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, K_FOREVER);
    if (ret != 0)   goto __exit;
    ret = ret_msg.result;
    k_mutex_unlock(&wifi_api_mutex);
    return 0;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

int csk_wifi_scan_ap(csk_wifi_scan_info_t **ap_info, csk_wifi_result_t *result, k_timeout_t timeout)
{
    CSK_WIFI_CHECK(result != NULL, -EINVAL);

    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    ret = wifi_send_command(NULL, WIFI_CMD_SCAN, K_NO_WAIT);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, timeout);
    if (ret != 0)   goto __exit;
    if (ret_msg.result > 0) {
        /* Fixme: Refactor this API to return wifi failed reason */
        *result = ret_msg.result;
        ret = -EIO;
        goto __exit;
    } else if (ret_msg.result < 0) {
        ret = ret_msg.result;
        *result = CSK_WIFI_ERR_SCAN_FAILED;
        goto __exit;
    }
    wifi_scan_list_t *scan_list = ret_msg.arg;
    *ap_info = scan_list->ap_list;
    k_mutex_unlock(&wifi_api_mutex);
    return scan_list->count;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

#ifdef CONFIG_CSK_WIFI_STATION

int csk_wifi_sta_connect(csk_wifi_sta_config_t *sta_config, csk_wifi_result_t *result, k_timeout_t timeout)
{
    CSK_WIFI_CHECK(sta_config != NULL && result != NULL, -EINVAL);
    CSK_WIFI_CHECK(sta_config->encryption_mode >= 0 &&
                   sta_config->encryption_mode < CSK_WIFI_AUTH_MAX,
                   -EINVAL);

    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_get_dev(WLAN_MODE_STA);
    struct csk_wifi_data *data = dev->data;

    memcpy(data->sta_config.ssid, sta_config->ssid, sizeof(data->sta_config.ssid));
    memcpy(data->sta_config.pwd, sta_config->pwd, sizeof(data->sta_config.pwd));
    memcpy(data->sta_config.bssid, sta_config->bssid, sizeof(data->sta_config.bssid));
    data->sta_config.encryption_mode = wifi_get_encryption_mode(sta_config->encryption_mode);

    ret = wifi_send_command(dev, WIFI_CMD_STA_CONNECT, K_NO_WAIT);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, timeout);
    if (ret != 0)   goto __exit;
    if (ret_msg.result > 0) {
        /* Fixme: Refactor this API to return wifi failed reason */
        *result = ret_msg.result;
        ret = -EIO;
        goto __exit;
    } else if (ret_msg.result < 0) {
        ret = ret_msg.result;
        *result = CSK_WIFI_ERR_STA_FAILED;
        goto __exit;
    } else {
        *result = ret_msg.result;
    }
    memcpy(sta_config->ssid, data->sta_config.ssid, sizeof(sta_config->ssid));
    memcpy(sta_config->bssid, data->sta_config.bssid, sizeof(sta_config->bssid));
    sta_config->rssi = data->sta_config.rssi;
    sta_config->channel = data->sta_config.channel;
    sta_config->encryption_mode = wifi_get_encryption_mode(data->sta_config.encryption_mode);
    k_mutex_unlock(&wifi_api_mutex);
    return 0;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

int csk_wifi_sta_disconnect(csk_wifi_result_t *result, k_timeout_t timeout)
{
    CSK_WIFI_CHECK(result != NULL, -EINVAL);

    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_get_dev(WLAN_MODE_STA);
    ret = wifi_send_command(dev, WIFI_CMD_STA_DISCONNECT, K_NO_WAIT);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, timeout);
    if (ret != 0)   goto __exit;
    if (ret_msg.result > 0) {
        /* Fixme: Refactor this API to return wifi failed reason */
        *result = ret_msg.result;
        ret = -EIO;
        goto __exit;
    } else if (ret_msg.result < 0) {
        ret = ret_msg.result;
        *result = CSK_WIFI_ERR_STA_FAILED;
        goto __exit;
    } else {
        *result = ret_msg.result;
    }
    k_mutex_unlock(&wifi_api_mutex);
    return 0;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

#endif
