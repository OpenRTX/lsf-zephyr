#include <string.h>
#include <stdio.h>
#include "unit_test.h"
// #include "lisa_os/mem.h"
// #include "lisa_log/log.h"
#include <lisa_mem.h>
#include <lisa_log.h>

#define TAG "mem_test"

#define TEST_CONTENT_LEN (10)
#define MEM_TEST_COUNT (100)

static uint8_t s_mem_test_content[TEST_CONTENT_LEN] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};

unit_test_result_t mem_test()
{
    // test step 1
    uint8_t* mem_test_step1 = lisa_mem_alloc(TEST_CONTENT_LEN);
    if (mem_test_step1 == NULL) {
        LISA_LOGE(TAG, "memory test failed because malloc failed");
        return LISA_TEST_FAILED;
    }
    memcpy(mem_test_step1, s_mem_test_content, TEST_CONTENT_LEN);
    unit_test_result_t mem_result = LISA_TEST_PASS;
    for (int i = 0; i < TEST_CONTENT_LEN; i++) {
        if (mem_test_step1[i] != s_mem_test_content[i]) {
            mem_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "memory test failed because malloc failed");
            break;
        }
    }
    lisa_mem_free(mem_test_step1);
    if (mem_result != LISA_TEST_PASS) return LISA_TEST_FAILED;
    
    // test step2
    uint8_t* mem_test_step2 = lisa_mem_calloc(1, TEST_CONTENT_LEN);
    if (mem_test_step2 == NULL) {
        LISA_LOGE(TAG, "memory test failed because calloc failed");
        return LISA_TEST_FAILED;
    }
    memcpy(mem_test_step2, s_mem_test_content, TEST_CONTENT_LEN);
    for (int i = 0; i < TEST_CONTENT_LEN; i++) {
        if (mem_test_step2[i] != s_mem_test_content[i]) {
            mem_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "memory test failed because calloc failed");
            break;
        }
    }
    lisa_mem_free(mem_test_step2);
    if (mem_result != LISA_TEST_PASS) return LISA_TEST_FAILED;

    // test step3
    uint8_t* mem_test_step3 = lisa_mem_alloc(1);
    if (mem_test_step3 == NULL) {
        LISA_LOGE(TAG, "memory test failed because malloc failed");
        return LISA_TEST_FAILED;
    }
    mem_test_step3 = lisa_mem_realloc(mem_test_step3, TEST_CONTENT_LEN);
    if (mem_test_step3 == NULL) {
        LISA_LOGE(TAG, "memory test failed because realloc failed");
        return LISA_TEST_FAILED;
    }
    memcpy(mem_test_step3, s_mem_test_content, TEST_CONTENT_LEN);
    for (int i = 0; i < TEST_CONTENT_LEN; i++) {
        if (mem_test_step3[i] != s_mem_test_content[i]) {
            mem_result = LISA_TEST_FAILED;
            LISA_LOGE(TAG, "memory test failed because relloc failed");
            break;
        }
    }
    lisa_mem_free(mem_test_step3);
    if (mem_result != LISA_TEST_PASS) return LISA_TEST_FAILED;

    // test step4
    for (int i = 0; i < MEM_TEST_COUNT; i++) {
        void* temp_alloc = lisa_mem_alloc(TEST_CONTENT_LEN);
        lisa_mem_free(temp_alloc);

        void* temp_calloc = lisa_mem_calloc(1, TEST_CONTENT_LEN);
        lisa_mem_free(temp_calloc);
    }
    return LISA_TEST_PASS;

}
