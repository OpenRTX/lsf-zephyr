#include "csk6/priv/csk_wifi_priv.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk_wifi, CONFIG_WIFI_LOG_LEVEL);

K_MUTEX_DEFINE(wifi_api_mutex);

static sys_slist_t wifi_callback_list;

void wifi_dispatch_callbacks(csk_wifi_event_t events, void *event_data, uint32_t event_data_len)
{
    sys_slist_t *list = &wifi_callback_list;
    csk_wifi_event_cb_t *cb, *tmp;
    /* Fixme: should add mutex prevent the race condition */
    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node) {
        if (cb->events & events) {
            cb->handler(cb->events & events, event_data, event_data_len, cb->arg);
        }
    }
}

static int wifi_remove_callback(csk_wifi_event_cb_t *wifi_event_cb)
{
    int ret = -ENXIO;
    sys_slist_t *list = &wifi_callback_list;
    csk_wifi_event_cb_t *cb, *tmp;
    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node) {
        if (cb == wifi_event_cb) {
            sys_slist_find_and_remove(list, &cb->node);
            ret = 0;
            break;
        }
    }
    return ret;
}

static void wifi_clean_callbacks(void)
{
    sys_slist_t *list = &wifi_callback_list;
    csk_wifi_event_cb_t *cb, *tmp;
    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node) {
        sys_slist_find_and_remove(list, &cb->node);
    }
}

int csk_wifi_add_callback(csk_wifi_event_cb_t *wifi_event_cb)
{
    CSK_WIFI_CHECK(wifi_event_cb != NULL, -EINVAL);
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    sys_slist_prepend(&wifi_callback_list, &wifi_event_cb->node);
    k_mutex_unlock(&wifi_api_mutex);
    return 0;
}

int csk_wifi_remove_callback(csk_wifi_event_cb_t *wifi_event_cb)
{
    CSK_WIFI_CHECK(wifi_event_cb != NULL, -EINVAL);
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    int ret = wifi_remove_callback(wifi_event_cb);
    k_mutex_unlock(&wifi_api_mutex);
    return ret;
}

int csk_wifi_init(void)
{
    int ret = 0;

    sys_slist_init(&wifi_callback_list);
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);

    ret = wifi_send_command(NULL, WIFI_CMD_INIT, K_NO_WAIT);
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
    int ret = 0;

    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
    wifi_clean_callbacks();
    ret = wifi_send_command(NULL, WIFI_CMD_DEINIT, K_NO_WAIT);
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
    *result = CSK_WIFI_ERR_SCAN_FAILED;
    CSK_WIFI_CHECK(result != NULL, -EINVAL);

    wifi_sta_state_machine_t current_sm = wifi_obj_get_sm();
    const char* wifi_sm_str = WIFI_STA_SM_STRINGS[current_sm];
    CSK_WIFI_CHECK_RETURN(current_sm != WIFI_SM_ON_STARTUP, -EAGAIN,
                            "Invalid State, current_sm: %s", wifi_sm_str);
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
    } else {
        *result = ret_msg.result;
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
    *result = CSK_WIFI_ERR_STA_FAILED;
    CSK_WIFI_CHECK(sta_config != NULL && result != NULL, -EINVAL);
    CSK_WIFI_CHECK(sta_config->encryption_mode >= 0 &&
                   sta_config->encryption_mode < CSK_WIFI_AUTH_MAX,
                   -EINVAL);

    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t current_sm = wifi_obj_get_sm();
    const char* wifi_sm_str = WIFI_STA_SM_STRINGS[current_sm];
    CSK_WIFI_CHECK_RETURN(current_sm == WIFI_SM_STA_DISCONNECTED ||
                            current_sm == WIFI_SM_STA_SCAN_DONE ||
                            current_sm == WIFI_SM_STARTUP_DONE,  -EAGAIN,
                            "Invalid State, current_sm: %s", wifi_sm_str);
    CSK_WIFI_CHECK_RETURN(data->sta_is_connected == false, -EAGAIN,
                            "Invalid State, Staion has connected");

    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);

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
    *result = CSK_WIFI_ERR_STA_FAILED;
    CSK_WIFI_CHECK(result != NULL, -EINVAL);

    const struct device *dev = wifi_obj_get_dev(CSK_WIFI_MODE_STA);
    struct csk_wifi_data *data = dev->data;
    wifi_sta_state_machine_t current_sm = wifi_obj_get_sm();
    const char* wifi_sm_str = WIFI_STA_SM_STRINGS[current_sm];
    CSK_WIFI_CHECK_RETURN(current_sm == WIFI_SM_STA_CONNECTED ||
                            current_sm == WIFI_SM_STA_SCAN_DONE,-EAGAIN,
                            "Invalid State, current_sm: %s", wifi_sm_str);
    CSK_WIFI_CHECK_RETURN(data->sta_is_connected == true, -EAGAIN,
                            "Invalid State, Staion has disconnected");

    int ret = 0;
    wifi_ret_msg_t ret_msg;
    k_mutex_lock(&wifi_api_mutex, K_FOREVER);
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
