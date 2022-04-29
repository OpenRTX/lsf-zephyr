/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <lisa_thread.h>
#include <lisa_semaphore.h>
#include <lisa_mutex.h>
#include <lisa_queue.h>
#include <lisa_time.h>
#include <lisa_timer.h>

#define TAG "tester_lisaos"
#define  _IN_ABS(a) ((a)>0?(a):-(a))
typedef struct {
	lisa_thread_t *threadA;
	lisa_thread_t *threadB;
	lisa_thread_t *threadC;
	lisa_timer_t *timer;

	lisa_semaphore_t *syncSem;
	lisa_semaphore_t *ackSem;
	lisa_semaphore_t *forMutex;
	lisa_semaphore_t *forQueue;

	lisa_mutex_t *mutex;
	lisa_queue_t *queue;
	char queuemsg[1024];
	char mutexString[1024];
} lisaOS_tester_data_t;
static lisaOS_tester_data_t lisaOS_tester_data;
#define LISAOS_QUEUE_MSG1 "Tester queue message1"
#define LISAOS_TIMER_ARG "lisaOS tester Timer"

void resourcesMutex(void *ptr)
{
	char str[128];
	LISA_LOGD(TAG, "[%s %d]%s wait get resources!", __FUNCTION__, __LINE__, (char *)ptr);
	if (LISA_OK != lisa_mutex_lock(lisaOS_tester_data.mutex, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_mutex_lock FAILED!");
	}
	LISA_LOGD(TAG, "[%s %d]%s get resources!", __FUNCTION__, __LINE__, (char *)ptr);
	k_sleep(K_MSEC(10));
	snprintf(str, sizeof(str), "This is %s\n", (char *)ptr);
	strcat(lisaOS_tester_data.mutexString, str);
	LISA_LOGD(TAG, "[%s %d]%s release resources!", __FUNCTION__, __LINE__, (char *)ptr);
	lisa_mutex_unlock(lisaOS_tester_data.mutex);
}

void threadA(void *arg)
{
	LISA_LOGD(TAG, "threadA with arg<%s>runing...", (char *)arg);

	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.forMutex, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take forMutex FAILED!");
	}
	resourcesMutex((char *)__FUNCTION__);

	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.forQueue, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take forQueue FAILED!");
	}
	snprintf(lisaOS_tester_data.queuemsg, sizeof(lisaOS_tester_data.queuemsg),
		 LISAOS_QUEUE_MSG1);
	if (LISA_OK != lisa_queue_push(lisaOS_tester_data.queue, lisaOS_tester_data.queuemsg,
				       sizeof(lisaOS_tester_data.queuemsg), LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_queue_push FAILED!");
	}

	while (1) {
		LISA_LOGD(TAG, "[%s %d]loop...", __FUNCTION__, __LINE__);
		k_sleep(K_MSEC(1000));
	}
}

void threadB(void *arg)
{
	char tempbuf[1024];
	LISA_LOGD(TAG, "threadB with arg<%s>runing...", (char *)arg);
	k_sleep(K_MSEC(10));
	lisa_semaphore_give(lisaOS_tester_data.ackSem);

	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.syncSem, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take syscSem FAILED!");
	}
	lisa_semaphore_give(lisaOS_tester_data.ackSem);

	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.forMutex, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take forMutex FAILED!");
	}
	resourcesMutex((char *)__FUNCTION__);

	lisa_semaphore_give(lisaOS_tester_data.ackSem);

	memset(tempbuf, 0, sizeof(tempbuf));
	if (LISA_OK != lisa_queue_receive(lisaOS_tester_data.queue, tempbuf, sizeof(tempbuf),
					  LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_queue_receive FAILED!");
	}
	if (0 != strcmp(tempbuf, LISAOS_QUEUE_MSG1)) {
		__ASSERT(0, "lisaOS queue message is error!str=%s", tempbuf);
	}
	LISA_LOGD(TAG, "queue message recive:%s", tempbuf);
	lisa_semaphore_give(lisaOS_tester_data.ackSem);

	while (1) {
		LISA_LOGD(TAG, "[%s %d]loop...", __FUNCTION__, __LINE__);
		k_sleep(K_MSEC(1000));
	}
}

void threadC_delete_self(void *arg)
{
	lisa_thread_t* thread;

	
	k_msleep(10);/*must wait for arg pointer update*/
	thread = *(lisa_thread_t**)arg;
	lisa_thread_delete(thread);

	__ASSERT(0, "lisa_thread_delete %s failed.\n",__FUNCTION__);
}
static void timerCB(lisa_timerhandle_t *timer)
{
	lisa_semaphore_give(lisaOS_tester_data.ackSem);
	LISA_LOGD(TAG, "[%s %d]user data=%s,time sec=%d,tick=%d", __FUNCTION__, __LINE__,
		  (char *)timer->user_data, lisa_os_get_time(), lisa_os_get_ticks());
}
void lisa_os_test(void)
{
	char *ptr = NULL;
	char *pos = NULL;
	int i;
	static K_THREAD_STACK_DEFINE(threadA_stack_area, 1024);
	static K_THREAD_STACK_DEFINE(threadB_stack_area, 2048);
	static K_THREAD_STACK_DEFINE(threadC_stack_area, 1024);
	lisa_thread_attr_t attr;
	LISA_LOGD(TAG, "lisaOS test start\n\n\n\n");
    lisa_heap_info();
	memset(&lisaOS_tester_data,0,sizeof(lisaOS_tester_data_t));

	lisaOS_tester_data.syncSem = lisa_semaphore_create(1);
	lisaOS_tester_data.ackSem = lisa_semaphore_create(1);
	lisaOS_tester_data.forMutex = lisa_semaphore_create(2);
	lisaOS_tester_data.forQueue = lisa_semaphore_create(1);
	if ((lisaOS_tester_data.syncSem == NULL) || (lisaOS_tester_data.ackSem == NULL) ||
	    (lisaOS_tester_data.forMutex == NULL) || (lisaOS_tester_data.forQueue == NULL)) {
		__ASSERT(0, "Create semaphore failed!");
	}

	lisaOS_tester_data.mutex = lisa_mutex_create();
	if (lisaOS_tester_data.mutex == NULL) {
		__ASSERT(0, "Create lisa_mutex_create failed!");
	}

	lisaOS_tester_data.queue =
		lisa_queue_create(1, "lisaOS_tester_msgqueue", sizeof(lisaOS_tester_data.queuemsg));
	if (lisaOS_tester_data.queue == NULL) {
		__ASSERT(0, "Create lisa_queue_create failed!");
	}

	LISA_LOGD(TAG, "++++++++++++++++Test 1: Thread test++++++++++++++++");
	LISA_LOGD(TAG, "Create threadA and threadB");
	attr.name = "threadA";
	attr.stack = threadA_stack_area;
	attr.stack_size = K_THREAD_STACK_SIZEOF(threadA_stack_area);
	attr.priority = 3;
	lisaOS_tester_data.threadA =
		lisa_thread_create(&attr, threadA, "I'm threadA from lisa_os_test!");
	k_sleep(K_MSEC(10));
	attr.name = "threadB";
	attr.stack = threadB_stack_area;
	attr.stack_size = K_THREAD_STACK_SIZEOF(threadB_stack_area);
	attr.priority = 4;
	lisaOS_tester_data.threadB =
		lisa_thread_create(&attr, threadB, "I'm threadB from lisa_os_test!");

	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.ackSem, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take take failed!");
	}

	attr.name = "threadC";
	attr.stack = threadC_stack_area;
	attr.stack_size = K_THREAD_STACK_SIZEOF(threadC_stack_area);
	attr.priority = 4;
	lisaOS_tester_data.threadC =
		lisa_thread_create(&attr, threadC_delete_self, &lisaOS_tester_data.threadC);

	LISA_LOGD(TAG, "----------------Test 1: Thread test SUCCESS!!!----------------\n\n\n");

	LISA_LOGD(TAG, "++++++++++++++++Test 2: mem test++++++++++++++++");
	lisa_heap_info();
	ptr = (char *)lisa_mem_alloc(1024);
	if (ptr == NULL) {
		__ASSERT(0, "mem malloc failed!");

	} else {
		pos = ptr;
		for (i = 0; i < 1024; i++) {
			*pos++ = i;
		}
		LISA_LOGD(TAG, "mem malloc success,ptr=%p!", ptr);
		lisa_mem_free(ptr);
	}

	ptr = (char *)lisa_mem_calloc(1024, 1);
	if (ptr == NULL) {
		__ASSERT(0, "mem malloc failed!");
	} else {
		pos = ptr;
		for (i = 0; i < 1024; i++) {
			if (*pos != 0) {
				__ASSERT(0, "mem clean failed addr=%d!", i);
			}
			*pos++ = i;
		}
		if (i == 1024) {
			LISA_LOGD(TAG, "mem calloc success,ptr=%p!", ptr);
		}
		lisa_mem_free(ptr);
	}

	ptr = (char *)lisa_strdup("strdup tester...");
	if (ptr == NULL) {
		__ASSERT(0, "mem lisa_strdup failed!");
	} else {
		LISA_LOGD(TAG, "lisa_strdup success! ptr=%p,string=%s", ptr, ptr);
		lisa_mem_free(ptr);
	}
	lisa_heap_info();
	LISA_LOGD(TAG, "----------------Test 2: mem test SUCCESS!!!----------------\n\n\n");

	LISA_LOGD(TAG, "++++++++++++++++Test 3: semaphore test+++++++++++++++");
	lisa_semaphore_give(lisaOS_tester_data.syncSem);
	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.ackSem, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take ackSem failed!");
	}
	if (LISA_OK == lisa_semaphore_take(lisaOS_tester_data.ackSem, LISA_NO_WAIT)) {
		__ASSERT(0, "lisa_semaphore_take check block failed!");
	}
	LISA_LOGD(TAG, "----------------Test 3: semaphore test SUCCESS!!!----------------\n\n\n");

	LISA_LOGD(TAG, "++++++++++++++++Test 4: mutex test++++++++++++++++");
	lisa_semaphore_give(lisaOS_tester_data.forMutex);
	lisa_semaphore_give(lisaOS_tester_data.forMutex);
	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.ackSem, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take ackSem failed!");
	}
	if (0 != memcmp(lisaOS_tester_data.mutexString, "This is threadA\nThis is threadB\n",
			strlen("This is threadA\nThis is threadB\n"))) {
		__ASSERT(0, "check mutexString failed,str<%s>!", lisaOS_tester_data.mutexString);
	}
	LISA_LOGD(TAG, "----------------Test 4: mutex test SUCCESS!!!----------------\n\n\n");

	LISA_LOGD(TAG, "++++++++++++++++Test 5: queue test++++++++++++++++");
	lisa_semaphore_give(lisaOS_tester_data.forQueue);
	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.ackSem, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take ackSem failed!");
	}
	LISA_LOGD(TAG, "----------------Test 5: queue test SUCCESS!!!----------------\n\n\n");

	LISA_LOGD(TAG, "++++++++++++++++Test 6: Thread delete test++++++++++++++++");
	if (LISA_OK != lisa_thread_delete(lisaOS_tester_data.threadA)) {
		__ASSERT(0, "lisa_thread_delete failed!");
	}
	if (LISA_OK != lisa_thread_delete(lisaOS_tester_data.threadB)) {
		__ASSERT(0, "lisa_thread_delete failed!");
	}

	LISA_LOGD(TAG, "----------------Test 6: Thread delete test SUCCESS!!!----------------\n\n\n");

	uint32_t ticks1 = 0;
	uint32_t ticks2 = 0;
	int32_t biasTicks = 0;
	LISA_LOGD(TAG, "++++++++++++++++Test 7: time test++++++++++++++++");
	ticks1 = lisa_os_get_ticks();
	k_msleep(100);
	ticks2 = lisa_os_get_ticks();
	biasTicks = (ticks2 - ticks1) - 100;
	;
	if (_IN_ABS(biasTicks) > 3) {
		__ASSERT(0, "lisa_os_get_ticks failed,ticks[%d,%d,%d]", ticks1, ticks2, biasTicks);
	}
	LISA_LOGD(TAG, "lisa_os_get_ticks bias ticks=%d", biasTicks);

	uint32_t timeSec1 = 0;
	uint32_t timeSec2 = 0;
	int32_t biasSec = 0;
	timeSec1 = lisa_os_get_time();
	k_sleep(K_SECONDS(1));
	timeSec2 = lisa_os_get_time();
	biasSec = (timeSec2 - timeSec1) - 1;
	if (biasSec != 0) {
		__ASSERT(0, "lisa_os_get_time failed,time[%d,%d,%d]", timeSec1, timeSec2, biasSec);
	}
	LISA_LOGD(TAG, "lisa_os_get_time bias time %d sec", biasSec);
	LISA_LOGD(TAG, "----------------Test 7: time test SUCCESS!!!----------------\n\n\n");

	LISA_LOGD(TAG, "++++++++++++++++Test 8: timer test++++++++++++++++");
	lisaOS_tester_data.timer =
		lisa_timer_create(OS_TIMER_PERIODIC, timerCB, LISAOS_TIMER_ARG, 1000);
	if (lisaOS_tester_data.timer == NULL) {
		__ASSERT(0, "lisa_timer_create failed");
	}

	lisa_timer_start(lisaOS_tester_data.timer);
	ticks1 = lisa_os_get_ticks();
	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.ackSem, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take ackSem failed!");
	}
	ticks2 = lisa_os_get_ticks();
	biasTicks = (ticks2 - ticks1) - K_MSEC(1000).ticks;
	if (_IN_ABS(biasTicks) > K_MSEC(10).ticks) {
		__ASSERT(0, "lisaOS timer period error!biasTicks=%d", biasTicks);
	}

	lisa_timer_change_period(lisaOS_tester_data.timer, 2000);
	ticks1 = lisa_os_get_ticks();

	if (LISA_OK != lisa_semaphore_take(lisaOS_tester_data.ackSem, LISA_WAIT_FOREVER)) {
		__ASSERT(0, "lisa_semaphore_take ackSem failed!");
	}
	ticks2 = lisa_os_get_ticks();
	biasTicks = (ticks2 - ticks1) - K_MSEC(2000).ticks;
	if (_IN_ABS(biasTicks) > K_MSEC(10).ticks) {
		__ASSERT(0, "lisaOS timer period error!diffTicks=%d", biasTicks);
	}
	lisa_timer_stop(lisaOS_tester_data.timer);

	LISA_LOGD(TAG, "----------------Test 8: timer test SUCCESS!!!----------------\n\n\n");

	lisa_semaphore_delete(lisaOS_tester_data.syncSem);
	lisa_semaphore_delete(lisaOS_tester_data.ackSem);
	lisa_semaphore_delete(lisaOS_tester_data.forMutex);
	lisa_semaphore_delete(lisaOS_tester_data.forQueue);
	lisa_mutex_delete(lisaOS_tester_data.mutex);
	lisa_queue_delete(lisaOS_tester_data.queue);
	lisa_timer_delete(lisaOS_tester_data.timer);

	k_msleep(100);/*wait for thread resource free*/
	lisa_heap_info();
	LISA_LOGD(TAG, "lisaOS test end\n\n\n\n");
}
