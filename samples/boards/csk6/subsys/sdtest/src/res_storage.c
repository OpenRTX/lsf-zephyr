#include <zephyr/zephyr.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <drivers/dma.h>
#include "res_storage.h"

// #define LOG_LEVEL 4
#include <logging/log.h>
LOG_MODULE_REGISTER(res_storage);

#define TRACE_TIME_COST (1)
#define TEST_DMA_COPY   (0)

#define DMA_COPY_MAX_SIZE   (4 * 4000)

typedef struct {
    const struct device *dma;
    struct k_sem dma_done;
    uint32_t src;
    uint32_t dst;
    uint32_t size;
    uint32_t burst_size;
} dma_copy_t;

typedef struct {
    const char *disk_pdrv;
    uint32_t disk_sector_count;
    uint32_t disk_sector_size;
    void *inner_buffer;
    uint8_t pingpong;
    uint32_t inner_ram_size;
    dma_copy_t dma_copy;
} res_storage_info_t;

res_storage_info_t res_storage;

static int _dma_copy(uint32_t src, uint32_t dst, uint32_t size);

void dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
    dma_copy_t *dma_copy = &res_storage.dma_copy;

    if(dma_copy->size > 0){

        dma_copy->src += dma_copy->burst_size;
        dma_copy->dst += dma_copy->burst_size;
        dma_copy->burst_size = dma_copy->size > DMA_COPY_MAX_SIZE ? DMA_COPY_MAX_SIZE : dma_copy->size;
        dma_copy->size -= dma_copy->burst_size;

        _dma_copy(dma_copy->src, dma_copy->dst, dma_copy->burst_size);
        // dma_reload(dma_copy->dma, 1, dma_copy->src, dma_copy->dst, dma_copy->burst_size);
        // dma_start(dma_copy->dma, 1);
    }else{
        k_sem_give(&dma_copy->dma_done);
    }

}

static int _dma_copy(uint32_t src, uint32_t dst, uint32_t size) 
{
    int ret = 0;
    dma_copy_t *dma_copy = &res_storage.dma_copy;

    // LOG_DBG("[%s] src:0x%x dst:0x%x size:0x%x\n", __FUNCTION__, src, dst, size);

    struct dma_block_config block_config = {
        .source_address = src,
        .dest_address = dst,
        .block_size = size,
        .dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
        .source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
    };

    struct dma_config dma_cfg = {
        .channel_direction = MEMORY_TO_MEMORY,
        .dma_callback = dma_callback,
        .user_data = NULL,
        .block_count = 1,
        .head_block = &block_config,
        .source_data_size = 4,
        .dest_data_size = 4,
        .source_burst_length = 1,
        .dest_burst_length = 1,
        .channel_priority = 1,
    };    

    ret = dma_config(dma_copy->dma, 1, &dma_cfg);
    if (ret != 0) {
        LOG_ERR("DMA config failed, return value:%d\n", ret);
        return -1;
    }

    ret = dma_start(dma_copy->dma, 1);
    if (ret != 0) {
        LOG_ERR("DMA start failed.\n");
        return -1;
    }    

    return 0;
}

static int dma_copy(uint32_t src, uint32_t dst, uint32_t size) 
{
    int ret = 0;
    dma_copy_t *dma_copy = &res_storage.dma_copy;

    LOG_DBG("[%s] src:0x%x dst:0x%x size:0x%x\n", __FUNCTION__, src, dst, size);

    // uint32_t time = k_cycle_get_32();

    ret = k_sem_take(&dma_copy->dma_done, K_NO_WAIT);
    if(ret == -EBUSY){
        LOG_INF("[%s]", __FUNCTION__);
    }
    
    dma_copy->burst_size = size > DMA_COPY_MAX_SIZE ? DMA_COPY_MAX_SIZE : size;
    dma_copy->size = size - dma_copy->burst_size;
    dma_copy->src = src;
    dma_copy->dst = dst;

    _dma_copy(dma_copy->src, dma_copy->dst, dma_copy->burst_size);

    // time = k_cycle_get_32() - time;

    // LOG_INF("DMA copy cost time: %d us\n", (uint32_t)((double)time / sys_clock_hw_cycles_per_sec() * 1000 * 1000));

    return 0;
}

int res_storage_init(void)
{
    int rc = 0;
    uint32_t cmd_buf;
    dma_copy_t *dma_copy = &res_storage.dma_copy;

    dma_copy->dma = DEVICE_DT_GET(DT_NODELABEL(dma0));
    if (!device_is_ready(dma_copy->dma)) {
        LOG_ERR("DMA device failed.\n");
        return -ENODEV;
    }

    k_sem_init(&dma_copy->dma_done, 1, 1);

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

int res_storage_config(res_storage_cgf_t *cfg)
{

    if(res_storage.inner_buffer){
        k_free(res_storage.inner_buffer);
    }

    res_storage.pingpong = 0;
    res_storage.inner_buffer = k_malloc(cfg->inner_ram_size * 2);
    res_storage.inner_ram_size = cfg->inner_ram_size;
    if(!res_storage.inner_buffer){
        LOG_ERR("[%s %d] Failed to malloc", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int res_storage_copy(void *src, void *dst, uint32_t size)
{
    int ret = 0;
    uint32_t i;
    uint8_t *databuf;
    uint32_t dst_addr = (uint32_t)dst;
	uint32_t read_time = 0;
	uint32_t num_sector = size/res_storage.disk_sector_size;
	uint32_t burst_num = res_storage.inner_ram_size/res_storage.disk_sector_size;

    if(size % res_storage.disk_sector_size != 0 || size == 0){
        LOG_ERR("[%s %d] Failed to operation", __FUNCTION__, __LINE__);
        return -EIO;
    }

    LOG_INF("[%s] num_sector:%d burst_num:%d\n", __FUNCTION__, num_sector, burst_num);

	read_time = k_uptime_get_32();
	for(i = 0; i < num_sector/burst_num; i++){
        databuf = (uint8_t *)res_storage.inner_buffer + res_storage.pingpong * res_storage.inner_ram_size;
        res_storage.pingpong ^= 0x01;
#if TEST_DMA_COPY
        for(uint32_t j = 0; j < res_storage.inner_ram_size; j++){
            databuf[j] = j;
        }
#else
		ret = disk_access_read(res_storage.disk_pdrv, databuf, (uint32_t)src + i * burst_num, burst_num);
		if (ret) {
			LOG_ERR("disk_access_write failed (%d)\n", ret);
			return -EIO;
		}
#endif
        dma_copy((uint32_t)databuf, dst_addr, res_storage.inner_ram_size);
        dst_addr += res_storage.inner_ram_size;
	}

    if(num_sector % burst_num != 0){
        databuf = (uint8_t *)res_storage.inner_buffer + res_storage.pingpong * res_storage.inner_ram_size;
        res_storage.pingpong ^= 0x01;
#if TEST_DMA_COPY
        uint8_t *buf = (uint8_t *)res_storage.inner_dma_buffer;
        for(uint32_t j = 0; j < res_storage.inner_ram_size; j++){
            buf[j] = j;
        }
#else
		ret = disk_access_read(res_storage.disk_pdrv, databuf, (uint32_t)src + i * burst_num, num_sector % burst_num);
		if (ret) {
			LOG_ERR("disk_access_write failed (%d)\n", ret);
			return -EIO;
		}
#endif
        dma_copy((uint32_t)databuf, dst_addr, (num_sector % burst_num) * res_storage.disk_sector_size);
    }

	read_time = k_uptime_get_32() - read_time;

	LOG_INF(" read cost:%d(ms), rate:%3.1f MB/s  \n", read_time, (size * 1000.0)/(read_time * 1024 * 1024));

#if TEST_DMA_COPY
    uint8_t *dst_buf = (uint8_t *)dst;
    SCB_InvalidateDCache_by_Addr((void *)dst_buf, size);
    for(uint32_t j = 0; j < size; j++){
        if(dst_buf[j] != (uint8_t)j){
            LOG_ERR("dma copy err (index:%d value:%d)\n", j, dst_buf[j]);
            break;
        }
    }
#endif
	return 0;
}

