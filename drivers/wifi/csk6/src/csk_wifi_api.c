#include "csk6/priv/csk_wifi_priv.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk_wifi, CONFIG_WIFI_LOG_LEVEL);

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

    sys_slist_init(&wifi_callback_list);
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    ret = wifi_send_command(NULL, WIFI_CMD_INIT, K_NO_WAIT, false);
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

int csk_wifi_deinit(void)
{
    return -ENOTSUP;
}

int csk_wifi_scan_ap(csk_wifi_scan_info_t **ap_info, csk_wifi_result_t *result, k_timeout_t timeout)
{
    CSK_WIFI_CHECK(result != NULL, -EINVAL);
    *result = CSK_WIFI_ERR_SCAN_FAILED;
    CSK_WIFI_CHECK_RETURN((K_TIMEOUT_EQ(timeout, K_FOREVER)) || (K_TIMEOUT_EQ(timeout, K_NO_WAIT)),
                            -ENOTSUP, "This timeout parameter is not available now, "
                            "only FOREVER or NO_WAIT is abailable");

    bool async_mode = K_TIMEOUT_EQ(timeout, K_NO_WAIT);
    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    ret = wifi_send_command(NULL, WIFI_CMD_SCAN, K_NO_WAIT, async_mode);
    if (ret != 0)   goto __exit;
    if (async_mode == false) {
        ret = wifi_take_result(&ret_msg, timeout);
        if (ret != 0)   goto __exit;
    } else {
        goto __exit;
    }
    *result = ret_msg.result;
    if (*result != CSK_WIFI_SUCCESS) {
        ret = -EIO;
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

    *result = CSK_WIFI_ERR_STA_FAILED;
    CSK_WIFI_CHECK_RETURN((K_TIMEOUT_EQ(timeout, K_FOREVER)) || (K_TIMEOUT_EQ(timeout, K_NO_WAIT)),
                            -ENOTSUP, "This timeout parameter is not available now, "
                            "only FOREVER or NO_WAIT is abailable");
    
    bool async_mode = K_TIMEOUT_EQ(timeout, K_NO_WAIT);
    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
    struct csk_wifi_data *data = dev->data;

    memcpy(&data->sta_config, sta_config, sizeof(csk_wifi_sta_config_t));

    ret = wifi_send_command(dev, WIFI_CMD_STA_CONNECT, K_NO_WAIT, async_mode);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, timeout);
    if (ret != 0)   goto __exit;
    *result = ret_msg.result;
    if (*result != CSK_WIFI_SUCCESS) {
        ret = -EIO;
        goto __exit;
    }

    memcpy(sta_config, &data->sta_config, sizeof(csk_wifi_sta_config_t));
    k_mutex_unlock(&wifi_api_mutex);
    return 0;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

int csk_wifi_sta_disconnect(csk_wifi_result_t *result, k_timeout_t timeout)
{
    CSK_WIFI_CHECK(result != NULL, -EINVAL);

    *result = CSK_WIFI_ERR_STA_FAILED;
    CSK_WIFI_CHECK_RETURN((K_TIMEOUT_EQ(timeout, K_FOREVER)) || (K_TIMEOUT_EQ(timeout, K_NO_WAIT)),
                            -ENOTSUP, "This timeout parameter is not available now, "
                            "only FOREVER or NO_WAIT is abailable");

    bool async_mode = K_TIMEOUT_EQ(timeout, K_NO_WAIT);
    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
    ret = wifi_send_command(dev, WIFI_CMD_STA_DISCONNECT, K_NO_WAIT, async_mode);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, timeout);
    if (ret != 0)   goto __exit;
    *result = ret_msg.result;
    if (*result != CSK_WIFI_SUCCESS) {
        ret = -EIO;
        goto __exit;
    }
    k_mutex_unlock(&wifi_api_mutex);
    return 0;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

int csk_wifi_sta_get_status(void)
{
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
    int ret_status = CSK_WIFI_STATUS_STA_UNKNOWN;
    struct csk_wifi_data *data = dev->data;

    if (data->sta_is_connected == true)
    {
        ret_status = CSK_WIFI_STATUS_STA_CONNECTED;
    }else{
        ret_status = CSK_WIFI_STATUS_STA_DISCONNECTED;
    }
    k_mutex_unlock(&wifi_api_mutex);

    return ret_status;
}

#endif

int csk_wifi_get_mac(csk_wifi_mode_t mode, uint8_t *mac_addr)
{
    wifi_sta_state_machine_t current_sm = wifi_obj_get_sm();
    if (current_sm == WIFI_SM_ON_STARTUP) {
        return -EAGAIN;
    }
    CSK_WIFI_CHECK(mac_addr != NULL, -EINVAL);
    CSK_WIFI_CHECK((mode >= CSK_WIFI_MODE_STA) && (mode < CSK_WIFI_MODE_MAX), -EINVAL);
    const struct device *dev = wifi_obj_get_dev(mode);
    struct csk_wifi_data *data = dev->data;
    memcpy(mac_addr, data->mac_addr, sizeof(data->mac_addr));
    return 0;
}

#ifdef CONFIG_CSK_WIFI_SOFTAP

int csk_wifi_ap_start(const csk_wifi_softap_config_t *ap_config, csk_wifi_result_t *result, k_timeout_t timeout)
{
    CSK_WIFI_CHECK(ap_config != NULL && result != NULL, -EINVAL);
    CSK_WIFI_CHECK(ap_config->encryption_mode >= 0 &&
                   ap_config->encryption_mode < CSK_WIFI_AUTH_MAX,
                   -EINVAL);
    CSK_WIFI_CHECK(ap_config->bandwidth >= 0 &&
                   ap_config->bandwidth <= CSK_WIFI_BW_HT40,
                   -EINVAL);

    bool async_mode = K_TIMEOUT_EQ(timeout, K_NO_WAIT);
    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_AP);
    struct csk_wifi_data *data = dev->data;

    memcpy(&data->ap_config, ap_config, sizeof(csk_wifi_softap_config_t));

    ret = wifi_send_command(dev, WIFI_CMD_AP_START, K_NO_WAIT, async_mode);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, timeout);
    if (ret != 0)   goto __exit;
    *result = ret_msg.result;
    if (*result != CSK_WIFI_SUCCESS) {
        ret = -EIO;
        goto __exit;
    }
    k_mutex_unlock(&wifi_api_mutex);
    return 0;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

int csk_wifi_ap_stop(csk_wifi_result_t *result, k_timeout_t timeout)
{
    CSK_WIFI_CHECK(result != NULL, -EINVAL);

    bool async_mode = K_TIMEOUT_EQ(timeout, K_NO_WAIT);
    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_AP);
    ret = wifi_send_command(dev, WIFI_CMD_AP_STOP, K_NO_WAIT, async_mode);
    if (ret != 0)   goto __exit;
    ret = wifi_take_result(&ret_msg, timeout);
    if (ret != 0)   goto __exit;
    *result = ret_msg.result;
    if (*result != CSK_WIFI_SUCCESS) {
        ret = -EIO;
        goto __exit;
    }
    return 0;

__exit:
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

#endif
