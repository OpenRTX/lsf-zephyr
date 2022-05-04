#include "unit_test.h"
#include <assert.h>
unit_test_result_t unit_test_normal()
{
	lisa_thread_delay(2);
	printf("开始单元测试...\n");
    unit_test_result_t result = mem_test();
	printf("%s\n", result == LISA_TEST_PASS ? "mem test=>[PASS]" : "mem test=>[FAIL]");
    assert(result == LISA_TEST_PASS);

	result = thread_test();
	printf("%s\n", result == LISA_TEST_PASS ? "thread test=>[PASS]" : "thread test=>[FAIL]");
    assert(result == LISA_TEST_PASS);

	result = time_test();
	printf("%s\n", result == LISA_TEST_PASS ? "time test=>[PASS]" : "time test=>[FAIL]");
    assert(result == LISA_TEST_PASS);

	result = mutex_test();
	printf("%s\n", result == LISA_TEST_PASS ? "mutex test=>[PASS]" : "mutex test=>[FAIL]");
    assert(result == LISA_TEST_PASS);

	result = sema_test();
	printf("%s\n", result == LISA_TEST_PASS ? "seam test=>[PASS]" : "seam test=>[FAIL]");
    assert(result == LISA_TEST_PASS);
    
	result = queue_test();
	printf("%s\n", result == LISA_TEST_PASS ? "queue test=>[PASS]" : "queue test=>[FAIL]");
    assert(result == LISA_TEST_PASS);

	result = http_test();
	printf("%s\n", result == LISA_TEST_PASS ? "http test=>[PASS]" : "http test=>[FAIL]");
    assert(result == LISA_TEST_PASS);

	result = websocket_test();
	printf("%s\n", result == LISA_TEST_PASS ? "ws test=>[PASS]" : "ws test=>[FAIL]");
    assert(result == LISA_TEST_PASS);

    return LISA_TEST_PASS;
}