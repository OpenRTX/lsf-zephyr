#ifndef __LISA_OS_TIMER__
#define __LISA_OS_TIMER__

#include <zephyr.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_time.h>

typedef struct k_timer lisa_timerhandle_t;
typedef k_timer_expiry_t lisa_timercb_t;

typedef enum {
	OS_TIMER_STOP = 0,
	OS_TIMER_START = 1,
} lisa_timerstatus_e;

typedef enum {
	OS_TIMER_ONCE = 0,
	OS_TIMER_PERIODIC = 1,
} lisa_timertype;

typedef struct {
	lisa_timerhandle_t handle;
	lisa_timerstatus_e status;
	uint32_t period_ms;
} lisa_timer_t;

lisa_timer_t *lisa_timer_create(lisa_timertype type, lisa_timercb_t cb, void *arg,
				uint32_t period_ms);
lisa_err_t lisa_timer_delete(lisa_timer_t *timer);
lisa_err_t lisa_timer_start(lisa_timer_t *timer);
lisa_err_t lisa_timer_stop(lisa_timer_t *timer);
lisa_err_t lisa_timer_change_period(lisa_timer_t *timer, uint32_t period_ms);

#endif //__LISA_OS_TIMER__