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

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_NODE)

#define FLASH_TEST_REGION_OFFSET FLASH_AREA_OFFSET(storage)

#define NUM_OF_SECTORS_TO_TEST 10
#define FLASH_SECTOR_SIZE 4096
#define TEST_DATA_LEN 4

int storage_offset;

void test_qspi_flash(void)
{
	const struct device *flash_dev;
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
	TC_PRINT("wr_buf:%p, &wr_buf:%p.\n", wr_buf, &wr_buf);
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
		zassert_equal(memcmp(wr_buf + 1, rd_buf, sizeof(rd_buf)), TC_PASS,
			      "Flash Write & Read Test failed!!");
		TC_PRINT("PASS\n");
	}
	// sys_reboot(0);
}

void test_main(void)
{
	ztest_test_suite(nios2_qspi_test_suite, ztest_unit_test(test_qspi_flash));
	ztest_run_test_suite(nios2_qspi_test_suite);
}
