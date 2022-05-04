#include <lisa_mem.h>
#include <lisa_time.h>
#include <lisa_log.h>
#include <lisa_semaphore.h>

#define TAG "lisa_semaphore"

lisa_semaphore_t *lisa_semaphore_create(uint32_t count)
{
	lisa_semaphore_t *lisa_sem = NULL;
	lisa_sem = (lisa_semaphore_t *)lisa_mem_calloc(1, sizeof(lisa_semaphore_t));
	if (lisa_sem == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_calloc FAILED!", __FUNCTION__, __LINE__);
		return NULL;
	}
	if (0 != k_sem_init(&lisa_sem->handle, 0, count)) {
		LISA_LOGE(TAG, "[%s %d]k_sem_init FAILED!", __FUNCTION__, __LINE__);
		lisa_mem_free(lisa_sem);
		return NULL;
	}
	LISA_LOGD(TAG, "[%s %d]k_mutex_init SUCCESS!", __FUNCTION__, __LINE__);
	return lisa_sem;
}

lisa_err_t lisa_semaphore_take(lisa_semaphore_t *sem, int32_t block_time)
{
	int iret;

	if (sem == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}

	if (true == k_is_in_isr()) {
		iret = k_sem_take(&sem->handle, K_NO_WAIT);
	} else {
		if (block_time == LISA_WAIT_FOREVER) {
			iret = k_sem_take(&sem->handle, K_FOREVER);
		} else {
			iret = k_sem_take(&sem->handle, K_MSEC(block_time));
		}
	}

	if (iret != 0) {
		LISA_LOGE(TAG, "[%s %d]k_sem_take FAILED errno=%d!", __FUNCTION__, __LINE__,
			  iret);
		return LISA_FAIL;
	}
	return LISA_OK;
}

lisa_err_t lisa_semaphore_give(lisa_semaphore_t *sem)
{
	if (sem == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	k_sem_give(&sem->handle);

	return LISA_OK;
}
lisa_err_t lisa_semaphore_delete(lisa_semaphore_t *sem)
{
	lisa_err_t result = LISA_FAIL;
	if (sem != NULL) {
		lisa_mem_free(sem);
		result = LISA_OK;
	}
	return result;
}

uint32_t lisa_semaphore_getcount(lisa_semaphore_t *sem)
{
	if (sem == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return 0;
	}
	return k_sem_count_get(&sem->handle);
}