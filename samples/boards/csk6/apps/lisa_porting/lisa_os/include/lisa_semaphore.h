#ifndef __LISA_OS_SEMAPHORE__
#define __LISA_OS_SEMAPHORE__

#include <zephyr.h>
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_time.h>

typedef struct k_sem lisa_semaphorehandle_t;

typedef struct {
	lisa_semaphorehandle_t handle;
} lisa_semaphore_t;

lisa_semaphore_t *lisa_semaphore_create(uint32_t count);
lisa_err_t lisa_semaphore_take(lisa_semaphore_t *sem, int32_t block_time);
lisa_err_t lisa_semaphore_give(lisa_semaphore_t *sem);
lisa_err_t lisa_semaphore_delete(lisa_semaphore_t *sem);
uint32_t lisa_semaphore_getcount(lisa_semaphore_t *sem);

#endif //__LISA_OS_SEMAPHORE__