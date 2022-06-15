#include <lisa_mem.h>
#include <lisa_time.h>
#include <lisa_log.h>
#include <lisa_mutex.h>

#define TAG "lisa_mutex"
lisa_mutex_t *lisa_mutex_create(void)
{
	lisa_mutex_t *lisa_mutex = NULL;
	lisa_mutex = (lisa_mutex_t *)lisa_mem_calloc(1, sizeof(lisa_mutex_t));
	if (lisa_mutex == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_calloc FAILED!", __FUNCTION__, __LINE__);
		return NULL;
	}

	if (0 != k_mutex_init(&lisa_mutex->handle)) {
		LISA_LOGE(TAG, "[%s %d]k_mutex_init FAILED!", __FUNCTION__, __LINE__);
		lisa_mem_free(lisa_mutex);
		return NULL;
	}

	LISA_LOGD(TAG, "[%s %d]k_mutex_init SUCCESS!", __FUNCTION__, __LINE__);

	return lisa_mutex;
}

lisa_err_t lisa_mutex_lock(lisa_mutex_t *mutex, int32_t block_time)
{
	int iret;

	if (mutex == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	if (true == k_is_in_isr()) {
		iret = k_mutex_lock(&mutex->handle, K_NO_WAIT);
	} else {
		if (block_time == LISA_WAIT_FOREVER) {
			iret = k_mutex_lock(&mutex->handle, K_FOREVER);
		} else {
			iret = k_mutex_lock(&mutex->handle, K_MSEC(block_time));
		}
	}
	if (0 != iret) {
		LISA_LOGE(TAG, "lisa_mutex_lock failed ,errno=%d!!!", iret);
		return LISA_FAIL;
	}

	return LISA_OK;
}

lisa_err_t lisa_mutex_unlock(lisa_mutex_t *mutex)
{
	int iret;
	if (mutex == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	iret = k_mutex_unlock(&mutex->handle);
	if (0 != iret) {
		LISA_LOGE(TAG, "k_mutex_unlock failed,err=%d!!!", iret);
		return LISA_FAIL;
	}
	return LISA_OK;
}

lisa_err_t lisa_mutex_delete(lisa_mutex_t *mutex)
{
	lisa_err_t result = LISA_FAIL;
	if (mutex != NULL) {
		lisa_mem_free(mutex);
		result = LISA_OK;
	}
	return result;
}