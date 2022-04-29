#ifndef __LISA_OS_THREAD__
#define __LISA_OS_THREAD__

#include <zephyr.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_time.h>

typedef struct k_thread lisa_threadhandle_t;
typedef k_tid_t lisa_threadid_t;

typedef enum {
	OS_PRIORITY_IDLE = K_PRIO_PREEMPT(CONFIG_NUM_PREEMPT_PRIORITIES - 1),
	OS_PRIORITY_LOW = K_PRIO_PREEMPT(CONFIG_NUM_PREEMPT_PRIORITIES - 2),
	OS_PRIORITY_BELOW_NORMAL = K_PRIO_PREEMPT(CONFIG_NUM_PREEMPT_PRIORITIES - 3),
	OS_PRIORITY_NORMAL = K_PRIO_PREEMPT(CONFIG_NUM_PREEMPT_PRIORITIES - 4),
	OS_PRIORITY_ABOVE_NORMAL = K_PRIO_PREEMPT(CONFIG_NUM_PREEMPT_PRIORITIES - 5),
	OS_PRIORITY_HIGH = K_PRIO_PREEMPT(2),
	OS_PRIORITY_HIGHER = K_PRIO_PREEMPT(1),
	OS_PRIORITY_REAL_TIME = K_PRIO_PREEMPT(0),
} OS_Priority;

typedef struct {
	lisa_threadhandle_t handle;
	lisa_threadid_t thread_id;
} lisa_thread_t;

typedef struct {
	char *name;
	void *stack;
	uint32_t stack_size;
	uint32_t priority;
} lisa_thread_attr_t;

lisa_thread_t *lisa_thread_create(const lisa_thread_attr_t *attr, void (*entry)(void *), void *arg);
lisa_err_t lisa_thread_delete(lisa_thread_t *thread);
lisa_err_t lisa_thread_yield(void);
lisa_err_t lisa_thread_suspend(lisa_thread_t *thread);
lisa_err_t lisa_thread_resume(lisa_thread_t *thread);
lisa_err_t lisa_thread_delay(uint32_t ticks);
lisa_err_t lisa_thread_mdelay(uint32_t ms);
lisa_thread_t *lisa_thread_getcurrent(void);

#endif //__LISA_OS_THREAD__