/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/device.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(sample, LOG_LEVEL_DBG);

#define NUM_SECTORS 	(8U)
static const char *disk_pdrv = CONFIG_SDMMC_VOLUME_NAME;
static uint32_t disk_sector_count;
static uint32_t disk_sector_size;
static uint8_t *wbuf, *rbuf;

void main(void)
{
	int ret = 0;
	uint32_t cmd_buf;

	ret = disk_access_init(disk_pdrv);

	ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &cmd_buf);
	if(ret != 0) {
		LOG_ERR("Disk ioctl get sector count failed (%d)\n", ret);
		return;
	}

	disk_sector_count = cmd_buf;
	LOG_DBG("Disk reports %u sectors\n", cmd_buf);

	ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &cmd_buf);
	if(ret != 0) {
		LOG_ERR("Disk ioctl get sector size failed (%d)\n", ret);
		return;
	}

	disk_sector_size = cmd_buf;
	LOG_DBG("Disk reports sector size %u\n", cmd_buf);

	wbuf = (uint8_t *)k_malloc(NUM_SECTORS * disk_sector_size);

	/* First, fill the write buffer with data */
	for (uint32_t i = 0; i < NUM_SECTORS * disk_sector_size; i++) {
		wbuf[i] = (i & (~NUM_SECTORS));
	}
	/* Now write data to the sector */
	ret = disk_access_write(disk_pdrv, wbuf, 0, NUM_SECTORS);
	if (ret) {
		LOG_ERR("disk_access_write failed (%d)\n", ret);
		return; /* Let calling function handle disk error */
	}

	/* Read back the written data into another buffer */
	rbuf = (uint8_t *)k_malloc(NUM_SECTORS * disk_sector_size);
	memset(rbuf, 0, NUM_SECTORS * disk_sector_size);

	ret = disk_access_read(disk_pdrv, rbuf, 0, NUM_SECTORS);
	if (ret) {
		LOG_ERR("disk_access_write failed (%d)\n", ret);
		return;
	}

	for(uint32_t i = 0; i < NUM_SECTORS * disk_sector_size; i++) {
		if(rbuf[i] != wbuf[i]){
			LOG_ERR("Read data did not match data written to disk %d\n", i);
		}
	}

	LOG_DBG("sdcard sample done \n");
}
