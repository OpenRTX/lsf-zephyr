/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <ztest.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/device.h>
#include <logging/log.h>
#include <csk_malloc.h>
#include "res_storage.h"
LOG_MODULE_REGISTER(sample, LOG_LEVEL_DBG);

//使用限定sram内存下，测试读写不同长度的速率
uint32_t bytes_per_tests[4] = {1024 * 1024 * 2, 1024 * 1024 * 3, 1024 * 1024 * 4, 1024 * 1024 * 5};
uint32_t sram_size[2] = {1024 * 25, 1024 * 50};

void *psram_buf = NULL;

void test_csk_sdcard_setup(void)
{
	int rc;

	rc = res_storage_init();
	zassert_equal(rc, 0, "res storage initialization failed");
}

void test_csk_sdcard_wr(void)
{
	int ret = 0;

	for(uint8_t i = 0; i < sizeof(bytes_per_tests)/sizeof(bytes_per_tests[0]); i++){
		psram_buf = csk_malloc(bytes_per_tests[i]);
		if(!psram_buf){
			LOG_ERR(" psram malloc err(0x%x) \n", bytes_per_tests[i]);
		}
		for(uint8_t j = 0; j < sizeof(sram_size)/sizeof(sram_size[0]); j++){
			res_storage_cgf_t cfg;
			cfg.inner_ram_size = sram_size[j];
			ret = res_storage_config(&cfg);
			zassert_equal(ret, 0, "res_storage_config failed 0x%x", cfg.inner_ram_size);

			LOG_INF(" data size:%d MB  sram buf size:%d KB \n", bytes_per_tests[i] / (1024 * 1024), sram_size[j] / 1024);
			ret = res_storage_copy(0, psram_buf, bytes_per_tests[i]);
			zassert_equal(ret, 0, "disk_sdmmc_tests failed, %d", ret);

		}

		csk_free(psram_buf);
	}
}

void test_main(void)
{
	ztest_test_suite(csk_sdcard_test,
			 ztest_unit_test(test_csk_sdcard_setup),
			 ztest_unit_test(test_csk_sdcard_wr)
			 );
	ztest_run_test_suite(csk_sdcard_test);
}