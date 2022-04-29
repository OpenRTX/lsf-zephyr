#ifndef __LISA_OS_QUEUE__
#define __LISA_OS_QUEUE__

#include <zephyr.h>
#include <lisa_mem.h>
#include <lisa_time.h>

typedef struct k_msgq lisa_queuehandle_t;

typedef struct {
	lisa_queuehandle_t handle;
	void *pool;
} lisa_queue_t;

lisa_queue_t *lisa_queue_create(uint32_t count, char *queue_name, uint32_t item_size);
lisa_err_t lisa_queue_push(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait);
lisa_err_t lisa_queue_receive(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait);
lisa_err_t lisa_queue_delete(lisa_queue_t *queue);
uint32_t lisa_queue_available(lisa_queue_t *queue);
uint32_t lisa_queue_waiting(lisa_queue_t *queue);
lisa_err_t lisa_queue_clear(lisa_queue_t *queue);

#endif //__LISA_OS_QUEUE__