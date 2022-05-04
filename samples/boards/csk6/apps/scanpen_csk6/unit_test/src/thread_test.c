#include <stdio.h>
#include <stdint.h>
#include "unit_test.h"
// #include "lisa_os/thread.h"
// #include "lisa_os/mem.h"
// #include "lisa_common/typedef.h"
// #include "lisa_log/log.h"

#include <lisa_thread.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>
#include <lisa_log.h>

#define TAG "thread_test"

static K_THREAD_STACK_DEFINE(thread_stack_area, 1024);
static K_THREAD_STACK_DEFINE(thread1_stack_area, 5 * 1024);
static K_THREAD_STACK_DEFINE(thread2_stack_area, 5 * 1024);
static K_THREAD_STACK_DEFINE(thread3_stack_area, 5 * 1024);

static lisa_thread_t *g_unit_test_thread = NULL;
static lisa_thread_t *g_unit_test_thread1 = NULL;
static lisa_thread_t *g_unit_test_thread2 = NULL;
static lisa_thread_t *g_unit_test_thread3 = NULL;
static char* g_test_handle = NULL;
static int g_test_num = 0;

static void test_thread(void* arg) {
    if (g_test_handle == arg) {
        lisa_mem_free(g_test_handle);
        for (int i = 0; i < 10; i++) {
            g_test_num++;
            lisa_thread_delay(1);
        }
    }
    
    lisa_thread_delete(g_unit_test_thread);
    g_unit_test_thread = NULL;
}

static void test_thread1(void* arg) {

    lisa_thread_delay(1); 
    g_test_num++;
    lisa_thread_delete(g_unit_test_thread1);
    g_unit_test_thread1 = NULL;
}

static void test_thread2(void* arg) {
    lisa_thread_delay(2); 
    g_test_num++;
    lisa_thread_delete(g_unit_test_thread2);
    g_unit_test_thread2 = NULL;
}

static void test_thread3(void* arg) {
    lisa_thread_delay(3);
    g_test_num++;
    lisa_thread_delete(g_unit_test_thread3);
    g_unit_test_thread3 = NULL;
}

unit_test_result_t thread_test()
{
    g_test_num = 0;
    // 测试是否多线程
    g_test_handle = lisa_mem_alloc(10);
    int temp_num = g_test_num;
	lisa_thread_attr_t thread_attr;
	thread_attr.name = "unit_thread_test";
	thread_attr.stack_size = 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;
    g_unit_test_thread = lisa_thread_create(&thread_attr, test_thread, g_test_handle);
    if (g_unit_test_thread == NULL) {
        LISA_LOGE(TAG, "thread create failed");
        return LISA_FAIL;
    }
    int num_change_time = 0;
    for (int i = 0; i < 20; i++) {
        if (temp_num != g_test_num) {
            temp_num = g_test_num;
            num_change_time++;
        }
        lisa_thread_mdelay(500);
    }
    if (num_change_time <= 1) return LISA_TEST_FAILED;


    // 测试是否能同时创建多条线程
    g_test_num = 0;
    lisa_thread_attr_t thread_attr1;
	thread_attr1.name = "unit_thread_test1";
	thread_attr1.stack_size = 1024;
	thread_attr1.priority = OS_PRIORITY_NORMAL;
    g_unit_test_thread1 = lisa_thread_create(&thread_attr1, test_thread1, g_test_handle);

    lisa_thread_attr_t thread_attr2;
	thread_attr2.name = "unit_thread_test2";
	thread_attr2.stack_size = 1024;
	thread_attr2.priority = OS_PRIORITY_NORMAL;
    g_unit_test_thread2 = lisa_thread_create(&thread_attr2, test_thread2, g_test_handle);
    if (g_unit_test_thread2 == NULL) {
        LISA_LOGE(TAG, "thread create failed");
        return LISA_FAIL;
    }

    lisa_thread_attr_t thread_attr3;
	thread_attr3.name = "unit_thread_test3";
    thread_attr3.stack_size = 1024;
	thread_attr3.priority = OS_PRIORITY_NORMAL;
    g_unit_test_thread3 = lisa_thread_create(&thread_attr3, test_thread3, g_test_handle);
    if (g_unit_test_thread3 == NULL) {
        LISA_LOGE(TAG, "thread create failed");
        return LISA_FAIL;
    }

    lisa_thread_delay(5);
    if (g_test_num != 3) return LISA_TEST_FAILED;

    return LISA_TEST_PASS;
}
