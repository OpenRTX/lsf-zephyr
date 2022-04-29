#include <zephyr.h>
#include <zephyr/types.h>
#include <init.h>
#include <lisa_typedef.h>
#include <lisa_mem.h>
#include <lisa_time.h>
#include <lisa_log.h>
#include <lisa_mutex.h>
#include <lisa_thread.h>
#define THREAD_TIMESLICE 5

#define TAG "lisa_thread"
typedef void (*lisa_thread_entry_t)(void *, void *, void *);

/* Delayed work for timeout handling */
static struct k_work_delayable		s_thread_delete_work;
static sys_slist_t pre_delete_thread_list;
static lisa_mutex_t *pre_thread_mutex = NULL;
#define LISA_THREAD_DELAY_DELETE_TIMEOUT	K_MSEC(100)
typedef struct{
	sys_snode_t node;
	lisa_thread_t *thread;
}pre_delete_thread_t;

static void _thread_delete_work_handler(struct k_work *work){
    sys_slist_t *list = &pre_delete_thread_list;
    pre_delete_thread_t *pre_thread, *tmp;
	lisa_mutex_lock(pre_thread_mutex,LISA_WAIT_FOREVER);
    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(list, pre_thread, tmp, node) {
		sys_slist_find_and_remove(list,&pre_thread->node);
		lisa_mem_free(pre_thread->thread);
		lisa_mem_free(pre_thread);
		
    }
	lisa_mutex_unlock(pre_thread_mutex);
}


static int lisa_thread_prepare(const struct device *unused){

	ARG_UNUSED(unused);
	pre_thread_mutex = lisa_mutex_create();
	sys_slist_init(&pre_delete_thread_list);
	k_work_init_delayable(&s_thread_delete_work,_thread_delete_work_handler);

	return 0;
}

static void _thread_wrapper(void *arg1, void *arg2, void *arg3)
{
	void (*fun_ptr)(void *) = arg3;
	fun_ptr(arg1);
}

lisa_thread_t *lisa_thread_create(const lisa_thread_attr_t *attr, void (*entry)(void *), void *arg)
{
	lisa_thread_t *lisa_thread = NULL;

	lisa_thread = (lisa_thread_t *)lisa_mem_calloc(1, sizeof(lisa_thread_t));
	if (lisa_thread == NULL) {
		LISA_LOGE(TAG, "[%s %d]lisa_mem_calloc FAILED!", __FUNCTION__, __LINE__);
		return NULL;
	}

	lisa_thread->thread_id =
		k_thread_create(&lisa_thread->handle, (k_thread_stack_t *)attr->stack,
				attr->stack_size, (k_thread_entry_t)_thread_wrapper, arg, NULL, entry,
				attr->priority, 0, K_NO_WAIT);
	k_thread_system_pool_assign(&lisa_thread->handle);
	return lisa_thread;
}

lisa_err_t lisa_thread_delete(lisa_thread_t *thread)
{
	int iret;
	if (thread == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}

	if(k_current_get() == thread->thread_id){
		pre_delete_thread_t* pre_thread;

		pre_thread = lisa_mem_alloc(sizeof(pre_delete_thread_t));
		if(pre_thread != NULL){
			pre_thread->thread = thread;
			lisa_mutex_lock(pre_thread_mutex,LISA_WAIT_FOREVER);
			sys_slist_prepend(&pre_delete_thread_list, &pre_thread->node);
			lisa_mutex_unlock(pre_thread_mutex);
			k_work_reschedule(&s_thread_delete_work, LISA_THREAD_DELAY_DELETE_TIMEOUT);
		}
		else{
			LISA_LOGE(TAG, "[%s %d]lisa_mem_alloc FAILED!", __FUNCTION__, __LINE__);
		}
		k_thread_abort(thread->thread_id);
		return LISA_OK;
	}

	k_thread_abort(thread->thread_id);
	if (true == k_is_in_isr()) {
		iret = k_thread_join(&thread->handle, K_NO_WAIT);
	} else {
		iret = k_thread_join(&thread->handle, K_FOREVER);
	}
	if (0 != iret) {
		LISA_LOGE(TAG, "[%s %d]k_thread_join failed,errno=%d!", __FUNCTION__, __LINE__,
			  iret);
		return LISA_FAIL;
	}

	lisa_mem_free(thread);

	return LISA_OK;
}

lisa_err_t lisa_thread_yield(void)
{
	k_yield();
	return LISA_OK;
}

lisa_err_t lisa_thread_suspend(lisa_thread_t *thread)
{
	if (thread == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	k_thread_suspend(thread->thread_id);
	return LISA_OK;
}

lisa_err_t lisa_thread_resume(lisa_thread_t *thread)
{
	if (thread == NULL) {
		LISA_LOGE(TAG, "[%s %d]Input paramter error!", __FUNCTION__, __LINE__);
		return LISA_FAIL;
	}
	k_thread_resume(thread->thread_id);
	return LISA_OK;
}

lisa_err_t lisa_thread_delay(uint32_t secs)
{
	k_sleep(K_SECONDS(secs));
	return LISA_OK;
}

lisa_err_t lisa_thread_mdelay(uint32_t ms)
{
	k_msleep(ms);
	return LISA_OK;
}

lisa_thread_t *lisa_thread_getcurrent(void)
{
	// TODO
	__ASSERT(0, "%s is unsupport", __FUNCTION__);
	return NULL;
}

SYS_INIT(lisa_thread_prepare, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
