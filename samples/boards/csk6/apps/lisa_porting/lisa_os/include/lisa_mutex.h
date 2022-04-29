#ifndef __LISA_OS_MUTEX__
#define __LISA_OS_MUTEX__

#include <zephyr.h>

typedef struct k_mutex lisa_mutexhandle_t;

typedef struct {
	lisa_mutexhandle_t handle;
} lisa_mutex_t;

lisa_mutex_t *lisa_mutex_create(void);
lisa_err_t lisa_mutex_lock(lisa_mutex_t *mutex, int32_t block_time);
lisa_err_t lisa_mutex_unlock(lisa_mutex_t *mutex);
lisa_err_t lisa_mutex_delete(lisa_mutex_t *mutex);

#endif //__LISA_OS_MUTEX__