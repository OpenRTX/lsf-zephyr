/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <ztest.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/device.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(sample, LOG_LEVEL_DBG);

/* 最大支持block size为512字节 */
#define SECTOR_SIZE 512


#define TEST_CNT	(10U)
static const char *disk_pdrv = CONFIG_SDMMC_VOLUME_NAME;
static uint32_t disk_sector_count;
static uint32_t disk_sector_size;

uint32_t bytes_per_tests[4] = {1024, 1024 *5, 1024 * 50, 1024 * 200};

int disk_sdmmc_tests(uint32_t bytes)
{
	int ret = 0;
	uint32_t write_time = 0;
	uint32_t read_time = 0;
	uint8_t *buf = 0;
	uint32_t num_sector = bytes/disk_sector_size;

	buf = (uint8_t *)k_malloc(num_sector * disk_sector_size);
	if(buf == NULL) {
		LOG_ERR(" buf malloc failed\n");
		ret = -ENOMEM;
		goto error;
		
	}

	for (uint32_t i = 0; i < num_sector * disk_sector_size; i++) {
		buf[i] = i % num_sector;
	}

	write_time = k_uptime_get_32();
	for(uint32_t j = 0; j < TEST_CNT; j++){
		ret = disk_access_write(disk_pdrv, buf, 10, num_sector);
		if (ret) {
			LOG_ERR("disk_access_write failed (%d)\n", ret);
			ret = -EIO;
			goto error;
		}
	}
	write_time = k_uptime_get_32() - write_time;


	read_time = k_uptime_get_32();
	for(uint32_t j = 0; j < TEST_CNT; j++){
		ret = disk_access_read(disk_pdrv, buf, 10, num_sector);
		if (ret) {
			LOG_ERR("disk_access_write failed (%d)\n", ret);
			ret = -EIO;
			goto error;
		}
	}
	read_time = k_uptime_get_32() - read_time;

	k_free(buf);
	LOG_DBG(" %d bytes --- %d us | %d us\n", bytes, write_time * 1000 / TEST_CNT, read_time * 1000 / TEST_CNT);
	return ret;
error:
	k_free(buf);
	return ret;
}

void test_csk_sdcard_setup(void)
{
	int rc;
	uint32_t cmd_buf;

	rc = disk_access_init(disk_pdrv);
	zassert_equal(rc, 0, "Disk access initialization failed");

	rc = disk_access_status(disk_pdrv);
	zassert_equal(rc, DISK_STATUS_OK, "Disk status is not OK");

	rc = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &cmd_buf);
	zassert_equal(rc, 0, "Disk ioctl get sector count failed");

	TC_PRINT("Disk reports %u sectors\n", cmd_buf);
	disk_sector_count = cmd_buf;

	rc = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &cmd_buf);
	zassert_equal(rc, 0, "Disk ioctl get sector size failed");
	TC_PRINT("Disk reports sector size %u\n", cmd_buf);
	disk_sector_size = cmd_buf;

	zassert_true(cmd_buf <= SECTOR_SIZE,
		"Test will fail, SECTOR_SIZE definition must be increased");
}

void test_csk_sdcard_wr(void)
{
	int ret = 0;

	LOG_DBG("  datelen  ---   write | read \n");

	for(uint8_t i = 0; i < sizeof(bytes_per_tests)/sizeof(bytes_per_tests[0]); i++){
		ret = disk_sdmmc_tests(bytes_per_tests[i]);
		zassert_equal(ret, 0, "disk_sdmmc_tests failed, %d", ret);
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