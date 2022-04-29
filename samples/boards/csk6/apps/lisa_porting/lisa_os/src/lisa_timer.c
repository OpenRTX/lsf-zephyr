#include <lisa_mem.h>
#include <lisa_log.h>
#include <lisa_timer.h>

#define TAG "lisa_timer"

lisa_timer_t *lisa_timer_create(lisa_timertype type, lisa_timercb_t cb, void *arg,
				uint32_t period_ms)
{
	lisa_timer_t *lisa_timer = NULL;

	if (type == OS_TIMER_ONCE) {
		LISA_LOGE(TAG, "[%s %d]Timer OS_TIMER_ONCE is not supported!!!!!", __FUNCTION__,
			  __LINE__);
		return NULL;
	}

	lisa_timer = (lisa_timer_t *)lisa_mem_calloc(1, sizeof(lisa_timer_t));
	if (lisa_timer == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_calloc FAILED!", __FUNCTION__, __LINE__);
		return NULL;
	}

	k_timer_init(&lisa_timer->handle, cb, NULL);
	k_timer_user_data_set(&lisa_timer->handle, arg);

	lisa_timer->period_ms = period_ms;
	return lisa_timer;
}

lisa_err_t lisa_timer_delete(lisa_timer_t *timer)
{
	if (timer == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}

	k_timer_stop(&timer->handle);
	lisa_mem_free(timer);

	return LISA_OK;
}

lisa_err_t lisa_timer_stop(lisa_timer_t *timer)
{
	if (timer == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	if (timer->status == OS_TIMER_STOP) {
		LISA_LOGD(TAG, "lisa_timer already is stop");
		LISA_LOGD(TAG, "lisa_timer %p\n", timer);
		return LISA_OK;
	}
	k_timer_stop(&timer->handle);
	timer->status = OS_TIMER_STOP;
	return LISA_OK;
}

lisa_err_t lisa_timer_start(lisa_timer_t *timer)
{
	if (timer == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	if (timer->status == OS_TIMER_START) {
		lisa_timer_stop(timer);
		LISA_LOGE(TAG, "lisa_timer already is start,so stop & start");
	}
	k_timer_start(&timer->handle, K_MSEC(timer->period_ms), K_MSEC(timer->period_ms));
	timer->status = OS_TIMER_START;

	return LISA_OK;
}

lisa_err_t lisa_timer_change_period(lisa_timer_t *timer, uint32_t period_ms)
{
	if (timer == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	timer->period_ms = period_ms;

	if (timer->status == OS_TIMER_START) {
		lisa_timer_stop(timer);
		LISA_LOGD(TAG, "lisa_timer already is start,so stop & start");
		lisa_timer_start(timer);
	}
	return LISA_OK;
}
