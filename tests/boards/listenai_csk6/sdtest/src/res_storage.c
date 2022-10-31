#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>

#include "res_storage.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(res_storage);

#define TRACE_TIME_COST (1)

typedef struct {
    const char *disk_pdrv;
    uint32_t disk_sector_count;
    uint32_t disk_sector_size;
    void *inner_buffer;
    uint32_t inner_ram_size;
} res_storage_info_t;

res_storage_info_t res_storage;

int res_storage_init(void)
{
    int rc = 0;
    uint32_t cmd_buf;

    res_storage.disk_pdrv = CONFIG_SDMMC_VOLUME_NAME;

	rc = disk_access_init(res_storage.disk_pdrv);
    if(rc != 0){
        LOG_ERR("Disk access initialization failed");
        return -1;
    }

	rc = disk_access_status(res_storage.disk_pdrv);
    if(rc != DISK_STATUS_OK){
        LOG_ERR("Disk status is not OK");
        return -1;
    }

	rc = disk_access_ioctl(res_storage.disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &cmd_buf);
    if(rc != 0){
        LOG_ERR("Disk ioctl get sector count failed");
        return -1;
    }

	LOG_INF("Disk reports %u sectors\n", cmd_buf);
	res_storage.disk_sector_count = cmd_buf;

	rc = disk_access_ioctl(res_storage.disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &cmd_buf);
    if(rc != 0){
        LOG_ERR("Disk ioctl get sector size faileds");
        return -1;
    }

	LOG_INF("Disk reports sector size %u\n", cmd_buf);
	res_storage.disk_sector_size = cmd_buf;

    return 0;
}

int res_storage_config(res_storage_cgf_t *cfg){

    if(res_storage.inner_buffer){
        k_free(res_storage.inner_buffer);
    }

    res_storage.inner_buffer = k_malloc(cfg->inner_ram_size);
    res_storage.inner_ram_size = cfg->inner_ram_size;
    if(!res_storage.inner_buffer){
        LOG_ERR("[%s %d] Failed to malloc", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int res_storage_copy(void *src, void *dst, uint32_t size){

    if(size % res_storage.disk_sector_size != 0 || size == 0){
        LOG_ERR("[%s %d] Failed to operation", __FUNCTION__, __LINE__);
        return -EIO;
    }

	int ret = 0;
	uint32_t read_time = 0;
	uint32_t num_sector = size/res_storage.disk_sector_size;
	uint32_t burst_num = res_storage.inner_ram_size/res_storage.disk_sector_size;

	read_time = k_uptime_get_32();
	for(uint32_t j = 0; j < num_sector; j+= burst_num){
		ret = disk_access_read(res_storage.disk_pdrv, res_storage.inner_buffer, j, burst_num);
		if (ret) {
			LOG_ERR("disk_access_write failed (%d)\n", ret);
			return -EIO;
		}
	}
	read_time = k_uptime_get_32() - read_time;

	LOG_INF(" read cost:%d(ms), rate:%3.1f MB/s  \n", read_time, (size * 1000.0)/(read_time * 1024 * 1024));

	return 0;
}