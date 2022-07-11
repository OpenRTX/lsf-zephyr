/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sys/printk.h"
#include <stdint.h>
#include <zephyr.h>
#include <kernel.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <device.h>
#include <ztest.h>
#include <sys/reboot.h>
#include <debug/thread_analyzer.h>

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_NODE)
#define FLASH_TEST_REGION_OFFSET FLASH_AREA_OFFSET(storage)

#define NUM_OF_SECTORS_TO_TEST 10
#define FLASH_SECTOR_SIZE 4096
#define TEST_DATA_LEN 4



static void flash_erase_target_sectors(const struct device *flash_dev, uint32_t storage_offset, int erase_start_sector_index, int erase_sector_num)
{
	int ret = 0;
	uint32_t offset = 0;

	for (int i = erase_start_sector_index; i < erase_sector_num; i++) {
		offset = FLASH_SECTOR_SIZE * i;
		offset += storage_offset;
		ret = flash_erase(flash_dev, offset, FLASH_SECTOR_SIZE);
		zassert_equal(ret, 0, "flash erase failed");
	}
}

void test_flash_erase(void)
{
	int storage_offset;
	const struct device *flash_dev;
	struct flash_pages_info info;
	int rc = 0;
	int ret = 0;
	uint32_t offset = 0;
	uint8_t wr_buf[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
	uint8_t read_buf[4] = { 0 };
	uint8_t def_buf[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
	int erase_start_sector_index = 0;
	int erase_sector_num = 3;

	flash_dev = DEVICE_DT_GET(FLASH_NODE);
	zassert_equal(!flash_dev, TC_PASS, "Flash device not found!");

	storage_offset = FLASH_TEST_REGION_OFFSET;
	rc = flash_get_page_info_by_offs(flash_dev, storage_offset, &info);
	if (rc) {
		printk("Unable to get page info\n");
		return;
	}
	TC_PRINT("##### sector size: %d ######\n", info.size);

	/* erase 3 sectors */
	flash_erase_target_sectors(flash_dev, storage_offset, erase_start_sector_index, erase_sector_num);

	/* write some test data in sector 1*/
	offset = 0;
	ret = flash_write(flash_dev, storage_offset + offset * FLASH_SECTOR_SIZE, &wr_buf, sizeof(wr_buf));
	zassert_equal(ret, 0, "flash write failed");

	/* write some test data in sector 2*/
	offset++;
	ret = flash_write(flash_dev, storage_offset + offset * FLASH_SECTOR_SIZE, &wr_buf, sizeof(wr_buf));
	zassert_equal(ret, 0, "flash write failed");

	/* only erase the 1st sector */
	erase_sector_num = 1;
	flash_erase_target_sectors(flash_dev, storage_offset, erase_start_sector_index, erase_sector_num);

	/* read 1st sector, It should be default value 0xFF */
	offset = 0;
	ret = flash_read(flash_dev, storage_offset + offset * FLASH_SECTOR_SIZE, &read_buf, sizeof(read_buf));
	zassert_equal(ret, 0, "flash 1st read failed");
	zassert_equal(memcmp(read_buf, def_buf, sizeof(read_buf)), 0, "read buf are not equal to write buf");

	/* read 2nd  sector, It should be the data that we wrote. */
	offset = 1;
	ret = flash_read(flash_dev, storage_offset + offset * FLASH_SECTOR_SIZE, &read_buf, sizeof(read_buf));
	zassert_equal(ret, 0, "flash 2nd read failed");
	zassert_equal(memcmp(read_buf, wr_buf, sizeof(read_buf)), 0, "read buf are not equal to write buf");


}

void test_flash_read_cache_issue(void)
{
	const struct device *flash_dev;
	int storage_offset;
	int rc = 0;
	uint32_t i, offset, rd_val, wr_val;
	uint8_t wr_buf[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
	uint8_t rd_buf[2];
	struct flash_pages_info info;

	flash_dev = DEVICE_DT_GET(FLASH_NODE);
	zassert_equal(!flash_dev, TC_PASS, "Flash device not found!");

	storage_offset = FLASH_AREA_OFFSET(storage);
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
		zassert_equal(flash_erase(flash_dev, offset, FLASH_SECTOR_SIZE), TC_PASS,
			      "Flash erase call failed!");
		zassert_equal(flash_read(flash_dev, offset, &rd_val, TEST_DATA_LEN), TC_PASS,
			      "Flash read call failed!");
		/* In case of erase all bits will be set to 1 */
		wr_val = 0xffffffff;
		zassert_equal(rd_val != wr_val, TC_PASS, "Flash Erase Test failed!!");
		TC_PRINT("PASS\n");

		/* Flash Write & Read Test */
		TC_PRINT("	Flash Write & Read Test...");
		wr_val = 0xAABBCCDD;
		zassert_equal(flash_write(flash_dev, offset, &wr_val, TEST_DATA_LEN), TC_PASS,
			      "Flash write call failed!");
		zassert_equal(flash_read(flash_dev, offset, &rd_val, TEST_DATA_LEN), TC_PASS,
			      "Flash read call failed!");
		zassert_equal(rd_val != wr_val, TC_PASS, "Flash Write & Read Test failed!!");
		TC_PRINT("PASS\n");

		/* Flash Unaligned Read Test */
		TC_PRINT("	Flash Unaligned Read Test...");
		zassert_equal(flash_write(flash_dev, offset + sizeof(wr_val), &wr_buf,
					  sizeof(wr_buf)),
			      TC_PASS, "Flash write call failed!");
		zassert_equal(flash_read(flash_dev, offset + sizeof(wr_val) + 1, &rd_buf,
					 sizeof(rd_buf)),
			      TC_PASS, "Flash read call failed!");
		zassert_equal(memcmp(wr_buf + 1, rd_buf, sizeof(rd_buf)), 0,
			      "Flash Write & Read Test failed!!, \n rd_buf[0]: 0x%02X, rd_buf[1]: 0x%02X \n", rd_buf[0], rd_buf[1]);
		TC_PRINT("PASS\n");
	}
}

void test_main(void)
{
	ztest_test_suite(flash_test_suite,
			 ztest_unit_test(test_flash_read_cache_issue),
			 ztest_unit_test(test_flash_erase)
             );

	ztest_run_test_suite(flash_test_suite);
}
