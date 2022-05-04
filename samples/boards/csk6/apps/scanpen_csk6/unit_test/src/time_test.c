#include <stdio.h>
#include "unit_test.h"
// #include "lisa_os/time.h"
// #include "lisa_os/thread.h"

#include <lisa_time.h>
#include <lisa_thread.h>

unit_test_result_t time_test()
{

    uint32_t time_start_s = lisa_os_get_time();
    lisa_thread_delay(10);
    uint32_t time_end_s = lisa_os_get_time();
    uint32_t used_time = time_end_s - time_start_s;
    if (used_time != 10 && used_time != 11) return LISA_TEST_FAILED;

    uint64_t time_start_ms = lisa_os_get_ticks();
    lisa_thread_mdelay(500);
    uint64_t time_end_ms =  lisa_os_get_ticks();
    uint64_t used_time_ms = time_end_ms - time_start_ms;
    if (used_time_ms < 500 || used_time_ms > 510) return LISA_TEST_FAILED;

    return LISA_TEST_PASS;
}
