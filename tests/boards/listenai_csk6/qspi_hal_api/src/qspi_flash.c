/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sys/printk.h"
#include <zephyr.h>
#include <kernel.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <device.h>
#include <ztest.h>
#include <sys/reboot.h>
#include <debug/thread_analyzer.h>
#include "spiflash.h"
#include "platform.h"

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_NODE)
#define FLASH_TEST_REGION_OFFSET FLASH_AREA_OFFSET(storage)


#define SOC_NV_FLASH_NODE DT_INST(0, soc_nv_flash)
#define FLASH_BASE_ADDRESS DT_REG_ADDR_BY_IDX(SOC_NV_FLASH_NODE, 0)

#define NUM_OF_SECTORS_TO_TEST 100
#define FLASH_SECTOR_SIZE 4096
#define TEST_DATA_LEN 4

int storage_offset;

extern _EXT_RAM int sflash_read(FLASH_DEV *dev, off_t offset, void *data, size_t len);
extern _EXT_RAM int sflash_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len);
extern _EXT_RAM int sflash_erase(FLASH_DEV *dev, off_t offset, size_t size);

void test_qspi_flash(void)
{
	const struct device *flash_dev;
	unsigned int key;
	int ret = 0;
	int rc = 0;
	uint32_t i, offset, rd_val, wr_val;
	uint8_t wr_buf[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
	uint8_t rd_buf[2];
	struct flash_pages_info info;

	flash_dev = DEVICE_DT_GET(FLASH_NODE);
	zassert_equal(!flash_dev, TC_PASS, "Flash device not found!");

	storage_offset = FLASH_TEST_REGION_OFFSET;
	rc = flash_get_page_info_by_offs(flash_dev, storage_offset, &info);
	if (rc) {
		printk("Unable to get page info\n");
		return;
	}
	TC_PRINT("##### sector size: %d ######\n", info.size);

	for (i = 0U; i < NUM_OF_SECTORS_TO_TEST; i++) {
		TC_PRINT("\nTesting: Flash Sector-%d\n", i);
		offset = FLASH_SECTOR_SIZE * i;
		offset += storage_offset;

		/* Flash Erase Test */
		TC_PRINT("	Flash Erase Test...");
		key = arch_irq_lock();
		zassert_equal(sflash_erase(flash_dev->config, FLASH_BASE_ADDRESS + offset, FLASH_SECTOR_SIZE), TC_PASS,
			      "Flash erase call failed!");
		arch_irq_unlock(key);
		zassert_equal(sflash_read(flash_dev->config, FLASH_BASE_ADDRESS + offset, &rd_val, TEST_DATA_LEN), TC_PASS,
			      "Flash read call failed!");
		/* In case of erase all bits will be set to 1 */
		wr_val = 0xffffffff;
		zassert_equal(rd_val != wr_val, TC_PASS, "Flash Erase Test failed!!");
		TC_PRINT("PASS\n");

		/* Flash Write & Read Test */
		TC_PRINT("	Flash Write & Read Test...\n");
		wr_val = 0xAABBCCDD;

		key = arch_irq_lock();
		printk("dev: %p, dev->w_protect: %d, dev->run_mod: %d \n",
		       flash_dev->config, ((FLASH_DEV *)flash_dev->config)->w_protect, ((FLASH_DEV *)flash_dev->config)->run_mod);
		ret = sflash_write(flash_dev->config, FLASH_BASE_ADDRESS + offset, &wr_val, TEST_DATA_LEN);
		arch_irq_unlock(key);
		zassert_equal(ret, TC_PASS, "Flash write call failed!, ret: %d", ret);
		zassert_equal(sflash_read(flash_dev->config, FLASH_BASE_ADDRESS + offset, &rd_val, TEST_DATA_LEN), TC_PASS,
			      "Flash read call failed!");
		zassert_equal(rd_val != wr_val, TC_PASS, "Flash Write & Read Test failed!!");
		TC_PRINT("PASS\n");

		/* Flash Unaligned Read Test */
		TC_PRINT("	Flash Unaligned Read Test...");

		key = arch_irq_lock();
		zassert_equal(sflash_write(flash_dev->config, FLASH_BASE_ADDRESS + offset + sizeof(wr_val), &wr_buf,
					   sizeof(wr_buf)),
			      TC_PASS, "Flash write call failed!");
		arch_irq_unlock(key);
		zassert_equal(sflash_read(flash_dev->config, FLASH_BASE_ADDRESS + offset + sizeof(wr_val) + 1, &rd_buf,
					  sizeof(rd_buf)),
			      TC_PASS, "Flash read call failed!");
		zassert_equal(memcmp(wr_buf + 1, rd_buf, sizeof(rd_buf)), TC_PASS,
			      "Flash Write & Read Test failed!!");
		TC_PRINT("PASS\n");
	}
}

void test_main(void)
{
	// thread_analyzer_print();
	ztest_test_suite(nios2_qspi_test_suite, ztest_unit_test(test_qspi_flash));
	ztest_run_test_suite(nios2_qspi_test_suite);
}
