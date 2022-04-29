#include <string.h>
#include <lisa_mem.h>
#include <lisa_time.h>
#include <lisa_log.h>

#include <lisa_queue.h>

#define TAG "lisa_queue"

lisa_queue_t *lisa_queue_create(uint32_t count, char *queue_name, uint32_t item_size)
{
	// int iret = 0;
	lisa_queue_t *lisa_queue = NULL;
	lisa_queue = (lisa_queue_t *)lisa_mem_calloc(1, sizeof(lisa_queue_t));
	if (lisa_queue == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_calloc FAILED!", __FUNCTION__, __LINE__);
		return NULL;
	}

	lisa_queue->pool = lisa_mem_calloc(count, item_size);
	if (NULL == lisa_queue->pool) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_calloc FAILED!", __FUNCTION__, __LINE__);
		lisa_mem_free(lisa_queue);
		return NULL;
	}

	k_msgq_init(&lisa_queue->handle, lisa_queue->pool, item_size, count);

	return lisa_queue;
}

lisa_err_t lisa_queue_push(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait)
{
	int iret;

	if ((queue == NULL) || (item == NULL)) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}

	if (true == k_is_in_isr()) {
		iret = k_msgq_put(&queue->handle, item, K_NO_WAIT);
	} else {
		if (wait == LISA_WAIT_FOREVER) {
			iret = k_msgq_put(&queue->handle, item, K_FOREVER);
		} else {
			iret = k_msgq_put(&queue->handle, item, K_MSEC(wait));
		}
	}
	if (iret != 0) {
		LISA_LOGE(TAG, "[%s %d]k_msgq_put FAILED errno=%d!", __FUNCTION__, __LINE__,
			  iret);
		return LISA_FAIL;
	}

	return LISA_OK;
}

lisa_err_t lisa_queue_receive(lisa_queue_t *queue, void *item, uint32_t item_size, int32_t wait)
{
	int iret = 0;

	if ((queue == NULL) || (item == NULL)) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}

	if (true == k_is_in_isr()) {
		iret = k_msgq_get(&queue->handle, item, K_NO_WAIT);

	} else {
		if (wait == LISA_WAIT_FOREVER) {
			iret = k_msgq_get(&queue->handle, item, K_FOREVER);
		} else {
			iret = k_msgq_get(&queue->handle, item, K_MSEC(wait));
		}
	}
	if (iret != 0) {
		return LISA_FAIL;
	}

	return LISA_OK;
}

lisa_err_t lisa_queue_delete(lisa_queue_t *queue)
{
	if (queue == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}

	k_msgq_cleanup(&queue->handle);
	lisa_mem_free(queue->pool);
	lisa_mem_free(queue);

	return LISA_OK;
}

uint32_t lisa_queue_available(lisa_queue_t *queue)
{
	if (queue == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return 0;
	}
	return k_msgq_num_free_get(&queue->handle);
}

uint32_t lisa_queue_waiting(lisa_queue_t *queue)
{
	if (queue == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return 0;
	}

	return k_msgq_num_used_get(&queue->handle);
}

lisa_err_t lisa_queue_clear(lisa_queue_t *queue)
{
	if (queue == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	k_msgq_purge(&queue->handle);

	return LISA_OK;
}

// testself

//#define LISA_QUEUE_TESTSELF

#ifdef LISA_QUEUE_TESTSELF

static lisa_queue_t *g_lisa_queue;

static void thread1_entry(void *parameter)
{
	char buf[10];
	uint8_t cnt = 0;
	int ret = LISA_OK;
	LISA_LOGI(TAG, "thread1_entry run");
	while (1) {
		ret = lisa_queue_receive(g_lisa_queue, &buf, sizeof(buf), LISA_WAIT_FOREVER);
		if (ret == LISA_OK) {
			for (uint8_t i = 0; i < 10; i++) {
				LISA_LOGE(TAG, "rev: %d", buf[i]);
			}
		} else {
			LISA_LOGE(TAG, "thread1 queue failed =%d", ret);
		}
	}
}

static void thread2_entry(void *parameter)
{
	char buf[10];
	uint8_t cnt = 0;
	LISA_LOGE(TAG, "thread2_entry run");

	while (1) {
		for (uint8_t i = 0; i < 10; i++) {
			buf[i] = i + cnt;
		}

		lisa_queue_push(g_lisa_queue, &buf, sizeof(buf), 0);

		vTaskDelay(10000);
		LISA_LOGE(TAG, "thread2_entry run");

		cnt += 1;
	}
}

static void lisa_queue(int argc, char *argv[])
{
	lisa_err_t ret;
	TaskHandle_t thread_t = NULL;

#if 0
	ret = lisa_queue_init(&g_lisa_queue, g_pool, 10, 100);
	if (ret != LISA_OK) {
		LISA_LOGE(TAG, "queue init err");
		return;
	}
#else
	g_lisa_queue = lisa_queue_create(1000, "text_qu1", 10);

#endif

	xTaskCreate(thread1_entry, "thread1", 1024, NULL, 25, &thread_t);
	if (thread_t != NULL) {
		LISA_LOGE(TAG, "thread1_entry create suc!");
	} else {
		LISA_LOGE(TAG, "thread1_entry create err!");
	}

	xTaskCreate(thread2_entry, "thread2", 1024, NULL, 25, &thread_t);
	if (thread_t != NULL) {
		LISA_LOGE(TAG, "thread2_entry create suc!");
	} else {
		LISA_LOGE(TAG, "thread2_entry create err!");
	}
}

CSK_SHELL_CMD(lisa_queue, lisa_queue_test);
#endif
