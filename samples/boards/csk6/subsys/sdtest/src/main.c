/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/device.h>
#include <logging/log.h>
#include <csk_malloc.h>
#include <cache.h>
#include "res_storage.h"
LOG_MODULE_REGISTER(sample, LOG_LEVEL_DBG);

#define VALIDATION_DATA		(0)
#define STORAGE_TESTSELF	(0)

//使用限定sram内存下，测试读写不同长度的速率
uint32_t bytes_per_tests[4] = {1024 * 1024 * 2, 1024 * 1024 * 3, 1024 * 1024 * 4, 1024 * 1024 * 5};
uint32_t sram_size[2] = {1024 * 25, 1024 * 50};

void *psram_buf = NULL;

void test_csk_sdcard_setup(void)
{
	int rc;

	rc = res_storage_init();
	if(rc != 0){
		LOG_ERR(" res storage initialization failed\n");
	}

#if STORAGE_TESTSELF
	res_storage_testself();
#endif
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
			if(ret != 0){
				LOG_ERR(" res_storage_config failed 0x%x\n", cfg.inner_ram_size);
				return;
			}

			LOG_DBG(" data size:%d MB  sram buf size:%d KB \n", bytes_per_tests[i] / (1024 * 1024), sram_size[j] / 1024);
#if VALIDATION_DATA
			for(uint32_t j = 0; j < bytes_per_tests[i]; j++){
				*((uint8_t *)psram_buf + j)= (uint8_t)j % (i + 13);
			}
			res_storage_write(psram_buf, 0, bytes_per_tests[i]);
			memset(psram_buf, 0, bytes_per_tests[i]);
#endif
			ret = res_storage_copy(0, psram_buf, bytes_per_tests[i]);
			if(ret != 0){
				LOG_ERR("disk_sdmmc_tests failed, %d", ret);
				return;
			}

#if VALIDATION_DATA
			sys_cache_data_range(psram_buf, bytes_per_tests[i], K_CACHE_INVD);
			for(uint32_t j = 0; j < bytes_per_tests[i]; j++){
				if(*((uint8_t *)psram_buf + j) != (uint8_t)j % (i + 13)){
					LOG_ERR("test_csk_sdcard_wr err psram_buf[%d] = %d", j, *((uint8_t *)psram_buf + j));
					break;
				}
			}
#endif			
		}

		csk_free(psram_buf);
	}
}

extern int load_cp(void);
extern void spi_mock(void);
void main(void)
{
	load_cp();
	spi_mock();
	k_sleep(K_USEC(1000));	//等待CP和SPI MOCK执行起来
	test_csk_sdcard_setup();
	test_csk_sdcard_wr();
}