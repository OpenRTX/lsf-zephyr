#include "evs_auth_mgr.h"
#include "lisa_evs_auth.h"
#include <lisa_semaphore.h>
#include <lisa_log.h>
#include <lisa_err.h>
#include <lisa_typedef.h>
#include <lisa_thread.h>
#include <string.h>
#include <lisa_evs_pref.h>

#define TAG "ScanpenApp_auth"

#define EVS_KEY_REFRESH_TOKEN ("refresh_token")

// 鉴权相关
static lisa_evs_auth_t *s_auth_handle = NULL;
static lisa_evs_auth_cb_t s_auth_cb;
static char s_user_code[80] = {0};
static char s_device_code[80] = {0};
static char s_refresh_token[80] = {0};
static char s_access_token[80] = {0};

lisa_semaphore_t *s_auth_semaphore = NULL;

/// 获取devicecode、usercode回调
void get_devicecode_success_cb(const lisa_evs_get_devicecode_response_t *const code_response)
{
	strcpy(s_user_code, code_response->user_code);
	strcpy(s_device_code, code_response->device_code);
	LISA_LOGD(TAG, "get device code success, device code: %s, usr code: %s",
			code_response->device_code, code_response->user_code);
	lisa_semaphore_give(s_auth_semaphore);
}

/// 获取token回调
void get_token_success_cb(const lisa_evs_get_token_response_t *const token_response)
{
	strcpy(s_refresh_token, token_response->m_refresh_token);
    strcpy(s_access_token, token_response->m_access_token);
	LISA_LOGD(TAG, "get token success, access token: %s, refresh token: %s",
			token_response->m_access_token, token_response->m_refresh_token);
    //保存refresh_token
	lisa_pref_put_string(PREF_KEY_REFRESH_TOKEN, token_response->m_refresh_token);
    lisa_semaphore_give(s_auth_semaphore);
}

/// 获取devicecode, usercode失败
void get_devicecode_fail_cb(const char *const msg)
{
	LISA_LOGE(TAG, "get device code fail, msg: %s", msg);
	lisa_semaphore_give(s_auth_semaphore);
}

/// 获取token失败
void get_token_failed_cb(const char *const msg)
{
	LISA_LOGE(TAG, "get token fail, msg: %s", msg);
	lisa_semaphore_give(s_auth_semaphore);
}

int start_evs_auth(const lisa_evs_config_t *const config)
{
    if (s_auth_handle == NULL) {
    	s_auth_cb.get_devicecode_success = get_devicecode_success_cb;
    	s_auth_cb.get_token_success = get_token_success_cb;
    	s_auth_cb.get_devicecode_failed = get_devicecode_fail_cb;
    	s_auth_cb.get_token_failed = get_token_failed_cb;
    	s_auth_handle = lisa_evs_auth_create(&s_auth_cb, config);
    	s_auth_semaphore = lisa_semaphore_create(2);
    } else {
		memset(s_user_code, 0, sizeof(s_user_code));
		memset(s_device_code, 0, sizeof(s_device_code));
		memset(s_refresh_token, 0, sizeof(s_refresh_token));
		memset(s_access_token, 0, sizeof(s_access_token));
    }

    //先从flash里面读取refresh_token
    //char *refreshToken = ef_get_env(EVS_KEY_REFRESH_TOKEN);
	char *refreshToken = NULL;
	lisa_err_t ret = lisa_pref_get_string(PREF_KEY_REFRESH_TOKEN, &refreshToken);
	if (ret == LISA_OK) {
		LISA_LOGD(TAG, "ef_get_env refreshToken : %s\n", refreshToken);
	}
	if (refreshToken == NULL || refreshToken[0] == '\0') {
    	//refresh_token不存在则需要先调用接口code，再调用接口获取access_token
	    lisa_evs_auth_request_device_code(s_auth_handle);
	    lisa_semaphore_take(s_auth_semaphore, LISA_WAIT_FOREVER);
	    lisa_thread_delay(1);
	    lisa_evs_auth_request_refreshtoken(s_auth_handle, s_user_code, s_device_code);
	    lisa_semaphore_take(s_auth_semaphore, LISA_WAIT_FOREVER);
	} else {
    	//refresh_token存在直接调用接口获取access_token
		strcpy(s_refresh_token, refreshToken);
		lisa_mem_free(refreshToken);
        //刷新access token接口
	    lisa_evs_auth_request_accesstoken(s_auth_handle, s_refresh_token);
	    lisa_semaphore_take(s_auth_semaphore, LISA_WAIT_FOREVER);
    }
	return LISA_OK;
}

const char *get_evs_access_token()
{
    return s_access_token;
}
