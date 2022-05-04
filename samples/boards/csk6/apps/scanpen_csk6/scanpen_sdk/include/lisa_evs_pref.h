#ifndef __LISA_EVS_PREF_H__
#define __LISA_EVS_PREF_H__

#include <lisa_err.h>
#include <lisa_mutex.h>

#define PREF_KEY_CLIENT_ID		("client_id")
#define PREF_KEY_DEVICE_ID		("device_id")
#define PREF_KEY_OTA_SECRET 	("ota_secret")
#define PREF_KEY_REFRESH_TOKEN 	("token")

typedef struct lisa_evs_pref
{
	lisa_mutex_t *m_mutex;
} lisa_evs_pref_t;

/**
 * @brief  init
 * @return lisa_evs_pref_t* 
 */
lisa_evs_pref_t* lisa_pref_init();

/**
 * @brief  deinit
 * @param  handle			[in]handle
 */
void lisa_pref_deinit(lisa_evs_pref_t *handle);

/**
 * @brief  put string
 * @param  k				[in]key
 * @param  v				[in]value
 * @return lisa_err_t
 */
lisa_err_t lisa_pref_put_string(const char *k, const char *v);

/**
 * @brief  get string need free out value
 * @param  k                [in]key
 * @param  out_v            [out]value
 * @return lisa_err_t 
 */
lisa_err_t lisa_pref_get_string(const char *k, char **out_v);

/**
 * @brief  put int
 * @param  k                [in]key
 * @param  v                [in]value
 * @return lisa_err_t 
 */
lisa_err_t lisa_pref_put_int(const char *k, const int v);

/**
 * @brief  get int
 * @param  k                [in]key
 * @param  out_v            [out]value
 * @return lisa_err_t 
 */
lisa_err_t lisa_pref_get_int(const char *k, int **out_v);

/**
 * @brief  put bool
 * @param  k                [in]key
 * @param  v                [in]value
 * @return lisa_err_t 
 */
lisa_err_t lisa_pref_put_bool(const char *k, bool v);

/**
 * @brief  get bool
 * @param  k                [in]key
 * @param  out_v            [out]value
 * @return lisa_err_t 
 */
lisa_err_t lisa_pref_get_bool(const char *k,  bool *out_v);

/**
 * @brief  delete config
 * @param  k                [in]key
 * @return lisa_err_t 
 */
lisa_err_t lisa_pref_delete(const char *k);

/**
 * @brief clear config
 */
void lisa_pref_clear();

/**
 * @brief print config msg
 */
void lisa_pref_dump();

#endif //__LISA_EVS_PREF_H__
