#include <stdio.h>
#include <stdint.h>
#include "unit_test.h"
// #include "lisa_common/typedef.h"
// #include "lisa_os/thread.h"
// #include "lisa_os/mutex.h"
// #include "lisa_os/mem.h"
// #include "lisa_log/log.h"
#include <lisa_typedef.h>
#include <lisa_thread.h>
#include <lisa_mutex.h>
#include <lisa_mem.h>
#include <lisa_log.h>

static K_THREAD_STACK_DEFINE(thread1_stack_area, 5 * 1024);
static K_THREAD_STACK_DEFINE(thread2_stack_area, 5 * 1024);

static lisa_thread_t *g_mutex_test_thread1 = NULL;
static lisa_thread_t *g_mutex_test_thread2 = NULL;
static lisa_mutex_t *g_text_mutex = NULL;
static unit_test_result_t g_result = LISA_TEST_FAILED;

#define MAIN_THREAD_LOCK_TIME (5000)
#define TEST_COUNT 10
#define TAG "mutex_test"

static void mutex_test1(void* arg) {
    lisa_thread_mdelay(200);
    lisa_mutex_lock(g_text_mutex, LISA_WAIT_FOREVER);
    g_result = LISA_TEST_PASS;
    lisa_mutex_unlock(g_text_mutex);

    lisa_thread_delete(g_mutex_test_thread1);
    g_mutex_test_thread1 = NULL;
}

static void mutex_test2(void* arg) {

    for (int i = 0; i < TEST_COUNT; i++) {
        lisa_thread_mdelay((MAIN_THREAD_LOCK_TIME / TEST_COUNT) - 10);
        if (g_result == LISA_TEST_PASS && i < TEST_COUNT - 1) {
            LISA_LOGE(TAG, "mutex not work");
            g_result = LISA_TEST_FAILED;
        }
    }
    
    lisa_thread_delete(g_mutex_test_thread2);
    g_mutex_test_thread2 = NULL;
}

unit_test_result_t mutex_test()
{
    g_result = LISA_TEST_FAILED;
    g_text_mutex = lisa_mutex_create();
    if (!g_text_mutex) {
        LISA_LOGE(TAG, "mutex create failed");
        return LISA_TEST_FAILED;
    }
    lisa_thread_attr_t thread_attr1;
	thread_attr1.name = "mutex_test1";
	thread_attr1.stack_size = 5 * 1024;
	thread_attr1.priority = OS_PRIORITY_NORMAL;
    g_mutex_test_thread1 = lisa_thread_create(&thread_attr1, mutex_test1, NULL);

    lisa_thread_attr_t thread_attr2;
	thread_attr2.name = "mutex_test2";
	thread_attr2.stack_size = 5 * 1024;
	thread_attr2.priority = OS_PRIORITY_NORMAL;
    g_mutex_test_thread2 = lisa_thread_create(&thread_attr2, mutex_test2, NULL);
    lisa_mutex_lock(g_text_mutex, LISA_WAIT_FOREVER);
    lisa_thread_mdelay(MAIN_THREAD_LOCK_TIME);
    lisa_mutex_unlock(g_text_mutex);
    lisa_thread_mdelay(MAIN_THREAD_LOCK_TIME / TEST_COUNT);

    lisa_mutex_delete(g_text_mutex);
    g_text_mutex = NULL;

    return g_result;
}
