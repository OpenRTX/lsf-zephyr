#ifndef _TESTER_LISA_H_
#define _TESTER_LISA_H_


#include <stdint.h>

typedef int32_t unit_test_result_t;

#define LISA_TEST_PASS   (int32_t)(1)
#define LISA_TEST_FAILED (int32_t)(0)
// interl
extern void _wifi_init(void);


// test
extern unit_test_result_t lisa_os_test(void);
extern unit_test_result_t lisa_https_test(void);
extern unit_test_result_t lisa_websocket_test(void);

#endif

