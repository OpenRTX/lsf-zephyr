#include "unit_test.h"
// #include "lisa_os/queue.h"
// #include "lisa_os/thread.h"
// #include "lisa_log/log.h"
// #include "lisa_common/typedef.h"
// #include "lisa_os/time.h"

#include <lisa_queue.h>
#include <lisa_thread.h>
#include <lisa_log.h>
#include <lisa_typedef.h>
#include <lisa_time.h>

#define TAG "queue_test"
#define QUEUE_TEST_TIME (10)
#define QUEUE_RECV_TIME (20)

static lisa_queue_t *g_queue = NULL;
static lisa_thread_t *g_thread = NULL;
static unit_test_result_t g_test_result = LISA_TEST_FAILED;
static int g_test_step = 0;

typedef struct 
{
    int index;
} queue_test_t;

static void queue_test_thread(void* arg)
{
    g_test_step = 1;
    // test step 1
    int temp_index = 1;
    while (temp_index <= QUEUE_TEST_TIME) {
        queue_test_t item;
        lisa_err_t ret = lisa_queue_receive(g_queue, &item, sizeof(queue_test_t), LISA_WAIT_FOREVER);
        if (ret != LISA_OK || item.index != temp_index) {
            LISA_LOGE(TAG, "queue recv failed %d %d", item.index, temp_index);
            g_test_result = LISA_TEST_FAILED;
            break;
        } else {
            temp_index++;
        }
    }
    g_test_step = 2;
    // test step 2
    temp_index = 0;
    while (temp_index < QUEUE_TEST_TIME) {
        queue_test_t item;
        lisa_err_t ret = lisa_queue_receive(g_queue, &item, sizeof(queue_test_t), LISA_WAIT_FOREVER);
        if (ret != LISA_OK) {
            LISA_LOGE(TAG, "queue recv failed");
            g_test_result = LISA_TEST_FAILED;
            break;
        }
        temp_index++;
        lisa_thread_mdelay(10);
    }

    g_test_step = 3;
    // test step 3
    temp_index = 0;
    while (temp_index < QUEUE_TEST_TIME) {
        queue_test_t item;
        uint64_t ms_begin = lisa_os_get_ticks();
        lisa_err_t ret = lisa_queue_receive(g_queue, &item, sizeof(queue_test_t), QUEUE_RECV_TIME);
        uint64_t ms_end = lisa_os_get_ticks();
        if (ret != LISA_OK) {
            if ((ms_end - ms_begin < QUEUE_RECV_TIME) || (ms_end - ms_begin > QUEUE_RECV_TIME * 2)) {
                LISA_LOGE(TAG, "receive item timeout failed");
                g_test_result = LISA_TEST_FAILED;
            }
        } else {
            temp_index++;
        }
    }

    g_test_step = 4;
    lisa_thread_delete(g_thread);
    g_thread = NULL;
}

unit_test_result_t queue_test()
{
    g_test_step = 0;
    g_queue = lisa_queue_create(10, "test_queue", sizeof(queue_test_t));
    if (!g_queue) {
        LISA_LOGE(TAG, "queue create failed\n");
        return LISA_TEST_FAILED;
    }

    lisa_thread_attr_t thread_attr;
	thread_attr.name = "unit_thread_test1";
	thread_attr.stack_size = 5 * 1024;
	thread_attr.priority = OS_PRIORITY_NORMAL;
    g_thread = lisa_thread_create(&thread_attr, queue_test_thread, NULL);

    // test step 1
    for (int i = 1; i <= QUEUE_TEST_TIME; i++) {
        queue_test_t item;
        memset(&item, 0, sizeof(queue_test_t));
        item.index = i;
        lisa_thread_mdelay(item.index * 50);
        lisa_queue_push(g_queue, (void*)&item, sizeof(item), 0);
    }

    while (true) {
        lisa_thread_mdelay(500);
        if (g_test_step == 2) break;
    }

    // test step 2
    for (int i = 0; i < QUEUE_TEST_TIME; i++) {
        queue_test_t item;
        memset(&item, 0, sizeof(queue_test_t));
        item.index = i;
        lisa_queue_push(g_queue, (void*)&item, sizeof(item), 0);
    }

    while (true) {
        lisa_thread_mdelay(500);
        if (g_test_step == 3) break;
    }
    // test step 3
    for (int i = 0; i < QUEUE_TEST_TIME; i++) {
        queue_test_t item;
        memset(&item, 0, sizeof(queue_test_t));
        item.index = i;
        lisa_thread_mdelay(100);
        lisa_queue_push(g_queue, (void*)&item, sizeof(item), 0);
    }
    
    // test step4
    lisa_queue_clear(g_queue);
    for (int i = 0; i < QUEUE_TEST_TIME; i++) {
        queue_test_t item;
        memset(&item, 0, sizeof(queue_test_t));
        item.index = i;
        lisa_queue_push(g_queue, (void*)&item, sizeof(item), 0);
    }
    uint32_t count =  lisa_queue_waiting(g_queue);
    if (count != QUEUE_TEST_TIME) {
        LISA_LOGE(TAG, "push item or get queue count failed");
        g_test_result = LISA_TEST_FAILED;
    }

    while (true) {
        lisa_thread_mdelay(500); 
        if (g_test_step == 4) break;
    }
    
    lisa_queue_clear(g_queue);
    lisa_queue_delete(g_queue);
    g_queue = NULL;
    return LISA_TEST_PASS;
}
