#include "unit_test.h"

#include <lisa_thread.h>
#include <lisa_semaphore.h>
#include <lisa_log.h>
#include <lisa_typedef.h>

#define TAG "seam_test"
#define SEAM_TEST_COUNT (10)
#define SEAM_TEST_SLEEP_TIME (100)

static lisa_semaphore_t *g_seam = NULL;
static lisa_thread_t *g_thread = NULL;
static unit_test_result_t g_test_flag = LISA_TEST_FAILED;

static void seam_test_thread(void* arg) 
{
    lisa_semaphore_take(g_seam, LISA_WAIT_FOREVER);
    g_test_flag = LISA_TEST_PASS;
    lisa_thread_delete(g_thread);
    g_thread = NULL;
}

unit_test_result_t sema_test()
{
    g_test_flag = LISA_TEST_FAILED;
    g_seam = lisa_semaphore_create(2);
    if (!g_seam) {
        LISA_LOGE(TAG, "seamphore create failed");
    }

    lisa_thread_attr_t thread_attr;
	thread_attr.name = "sema_test1";
	thread_attr.stack_size = 2 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;
    g_thread = lisa_thread_create(&thread_attr, seam_test_thread, NULL);

    for (int i = 0; i < SEAM_TEST_COUNT; i++) {
        if (g_test_flag == LISA_TEST_PASS) {
            lisa_semaphore_give(g_seam);
            LISA_LOGE(TAG, "seamphore not work");
        }
        lisa_thread_mdelay(SEAM_TEST_SLEEP_TIME);
    }
    lisa_semaphore_give(g_seam);
    lisa_thread_mdelay(100);

    lisa_semaphore_delete(g_seam);
    g_seam = NULL;
    return g_test_flag;
}
