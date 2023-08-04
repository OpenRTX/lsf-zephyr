/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_sdmmc

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/disk.h>
#include <zephyr/logging/log.h>
#include "sdmmc/lib_sdc.h"
#include "sdmmc/ftsdc021.h"
#include <zephyr/drivers/pinctrl.h>

LOG_MODULE_REGISTER(csk6_sdmmc, CONFIG_SDMMC_LOG_LEVEL);

K_MUTEX_DEFINE(sdmmc_mutex);
struct sdmmc_csk_config {
	void (*irq_config_func)(const struct device *dev);
	const struct pinctrl_dev_config *pcfg;
	uint8_t bit_bus;
};

struct sdmmc_csk_data {
	/* driver handle */
	void *handle; 

	uint32_t blk_num;
	uint32_t blk_size;
	uint32_t erase_size;
};

static int csk6_sdmmc_bus_probe(const struct device *dev);
static int csk6_sdmmc_access_init(struct disk_info *disk);
static int csk6_sdmmc_access_status(struct disk_info *disk);
static int csk6_sdmmc_access_read(struct disk_info *disk, uint8_t *data_buf,
				   uint32_t start_sector, uint32_t num_sector);
static int csk6_sdmmc_access_write(struct disk_info *disk,
				    const uint8_t *data_buf,
				    uint32_t start_sector, uint32_t num_sector);
static int csk6_sdmmc_access_ioctl(struct disk_info *disk, uint8_t cmd,
				    void *buff);
static const struct disk_operations csk6_sdmmc_ops = {
	.init = csk6_sdmmc_access_init,
	.status = csk6_sdmmc_access_status,
	.read = csk6_sdmmc_access_read,
	.write = csk6_sdmmc_access_write,
	.ioctl = csk6_sdmmc_access_ioctl,
};

static struct disk_info csk6_sdmmc_info = {
	.name = CONFIG_SDMMC_VOLUME_NAME,
	.ops = &csk6_sdmmc_ops,
};

#ifdef CONFIG_SDMMC_NON_BLOCK_INIT
static struct k_work_q sdmmc_workq;
struct k_work init_work;
static K_THREAD_STACK_DEFINE(sdmmc_workq_stack_area, 2048);
static K_EVENT_DEFINE(event_group);
#define INIT_BIT 	BIT(0)
static const struct device *s_init_dev = NULL;

static int wait_init_done(void)
{
	int ret = k_event_wait(&event_group, INIT_BIT, false, K_FOREVER);
	if (!(ret & INIT_BIT)) {
		return -1;
	}
	return 0;
}

static void init_work_handler(struct k_work *item)
{
	ARG_UNUSED(item);
	const struct device *dev = s_init_dev;

	int err = csk6_sdmmc_bus_probe(dev);
	if (err < 0) {
		LOG_ERR("init failed");
		return;
	}
	k_event_set(&event_group, INIT_BIT);
}

#else
#define	wait_init_done()
#endif

static uint8_t sdcard_buf[sizeof(SDCardInfo)]     __attribute__ ((aligned(4)));

//SDIO驱动需要32字节对齐
#define _ALIGEN		(32)

/*
	目前上海SDIO驱动只支持sector为512字节的读写
	后期可能优化至支持1024字节的block size
*/
static uint8_t sdcard_w_buf[1024]     __attribute__((aligned(_ALIGEN)));

static uint8_t sdcard_r_buf[1024]     __attribute__((aligned(_ALIGEN)));

static void NullSetting()
{
}

static void csk6_sdmmc_isr(const struct device *dev)
{
    extern void ftsdc021_0_IntrHandler();
    ftsdc021_0_IntrHandler();
}


static int csk6_sdmmc_access_init(struct disk_info *disk)
{
	return 0;
}

static int csk6_sdmmc_access_status(struct disk_info *disk)
{
	return DISK_STATUS_OK;
}

static inline bool is_addr_in_sdram(uint32_t addr)
{
	uint32_t sram_base = DT_REG_ADDR(DT_CHOSEN(zephyr_sram));
	uint32_t sram_size = DT_REG_SIZE(DT_CHOSEN(zephyr_sram));

	if(addr >= sram_base && addr < sram_base + sram_size){
		return true;
	}
	return false;
}

#ifdef CONFIG_SDMMC_CSK6_SRAM_CACHE_ACCESS

#include <zephyr/drivers/dma.h>

#define SDMMC_CSK6_ACCESS_MODE_READ 0
#define SDMMC_CSK6_ACCESS_MODE_WRITE 1

static uint8_t sdmmc_sram_cache[CONFIG_SDMMC_CSK6_SRAM_CACHE_NUM][CONFIG_SDMMC_CSK6_SRAM_CACHE_SIZE]
	__attribute__((aligned(32))) = {0};

K_FIFO_DEFINE(sdmmc_queue_psram2sram);

typedef struct fifo_item {
	void *reserved;
	uint8_t *buffer;
} fifo_item_t;

static fifo_item_t fifo_items[CONFIG_SDMMC_CSK6_SRAM_CACHE_NUM] = {0};

#define SDMMC_CSK6_DMA_CHANNEL 1

#if (SDMMC_SRAM_CACHE_SIZE % 512) != 0
#error "(SDMMC_SRAM_CACHE_SIZE % 512) != 0"
#endif

static inline bool csk6_sdmmc_addr_can_fast_access(void *addr)
{
	return ((((uint32_t) addr) % _ALIGEN) == 0) && is_addr_in_sdram((uint32_t)addr);
}

static inline int csk6_sdmmc_hal_access(uint8_t *data_buf, uint32_t start_sector,
					uint32_t num_sector, uint8_t mode)
{
	return mode == SDMMC_CSK6_ACCESS_MODE_READ
		       ? gm_sdc_api_sdcard_sector_read(SD_0, start_sector, num_sector, data_buf)
		       : gm_sdc_api_sdcard_sector_write(SD_0, start_sector, num_sector, data_buf);
}

static void dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
	if (status == 0 && channel == SDMMC_CSK6_DMA_CHANNEL) {
		if (user_data) {
			k_fifo_put(&sdmmc_queue_psram2sram, user_data);
		}
	} else {
		LOG_ERR("dma callback failed: %d, ch: %d", status, channel);
	}
}

static inline int32_t  disk_mem_dma_copy(fifo_item_t *item, uint8_t *dst, uint8_t *src, uint32_t size)
{
	/* start dma to copy datas */
	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = size,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = item,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 4,
		.dest_data_size = 4,
		.channel_priority = 1,
		.source_burst_length = 4,
		.dest_burst_length = 4,
	};
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(dma0));
	int err = dma_config(dev, SDMMC_CSK6_DMA_CHANNEL, &dma_cfg);
	if (err != 0) {
		LOG_ERR("dma config failed: %d", err);
		return err;
	}

	err = dma_start(dev, SDMMC_CSK6_DMA_CHANNEL);
	if (err != 0) {
		LOG_ERR("dma start failed: %d", err);
		return err;
	}

	return 0;
}

static inline void csk6_sdmmc_inner_sram_cache_to_target(fifo_item_t *item, uint8_t *dst,
							 uint8_t *src, uint32_t size)
{
	if (size < 10 * 1024) {
		memcpy(dst, src, size);
		k_fifo_put(&sdmmc_queue_psram2sram, item);
	} else {
		/* item will be puted at dma callback */
		disk_mem_dma_copy(item, dst, src, size);
	}
}

static inline int csk_sdmmc_access_inner_sram(struct disk_info *disk, uint8_t *data_buf,
				       uint32_t start_sector, uint32_t num_sector, uint8_t mode)
{
	uint32_t cache_num_sector = CONFIG_SDMMC_CSK6_SRAM_CACHE_SIZE / 512;
	uint32_t cnt = num_sector / (cache_num_sector);
	int err;
	fifo_item_t *item = NULL;

	while (cnt--) {
		item = k_fifo_get(&sdmmc_queue_psram2sram, K_FOREVER);
		if (mode == SDMMC_CSK6_ACCESS_MODE_READ) {
			err = csk6_sdmmc_hal_access(item->buffer, start_sector, cache_num_sector,
						    mode);
			if (err) {
				return err;
			}
			csk6_sdmmc_inner_sram_cache_to_target(item, data_buf, item->buffer,
							      CONFIG_SDMMC_CSK6_SRAM_CACHE_SIZE);
		} else {
			csk6_sdmmc_inner_sram_cache_to_target(item, item->buffer, data_buf,
							      CONFIG_SDMMC_CSK6_SRAM_CACHE_SIZE);
			err = csk6_sdmmc_hal_access(item->buffer, start_sector, cache_num_sector, mode);
			if (err) {
				return err;
			}
		}

		start_sector += cache_num_sector;
		data_buf += CONFIG_SDMMC_CSK6_SRAM_CACHE_SIZE;
	}
	item = k_fifo_get(&sdmmc_queue_psram2sram, K_FOREVER);
	cnt = num_sector % cache_num_sector;
	if (cnt) {
		if (mode == SDMMC_CSK6_ACCESS_MODE_READ) {
			err = csk6_sdmmc_hal_access(item->buffer, start_sector, cnt, mode);
			if (err) {
				return err;
			}
			csk6_sdmmc_inner_sram_cache_to_target(item, data_buf, item->buffer,
							      cnt * 512);
		} else {
			csk6_sdmmc_inner_sram_cache_to_target(item, item->buffer, data_buf,
							      cnt * 512);
			err = csk6_sdmmc_hal_access(item->buffer, start_sector, cache_num_sector, mode);
			if (err) {
				return err;
			}
		}
	}

	/* have to wait for dma copy complete here */
	fifo_item_t *items[CONFIG_SDMMC_CSK6_SRAM_CACHE_NUM];
	uint8_t i;
	for (i = 0; i < CONFIG_SDMMC_CSK6_SRAM_CACHE_NUM; i++) {
		items[i] = k_fifo_get(&sdmmc_queue_psram2sram, K_FOREVER);
	}

	for (i = 0; i < CONFIG_SDMMC_CSK6_SRAM_CACHE_NUM; i++) {
		k_fifo_put(&sdmmc_queue_psram2sram, items[i]);
	}

	return 0;
}

static int csk6_sdmmc_access(struct disk_info *disk, uint8_t *data_buf, uint32_t start_sector,
			     uint32_t num_sector, uint8_t mode)
{
	int err = 0;

	if (csk6_sdmmc_addr_can_fast_access(data_buf)) {
		err = csk6_sdmmc_hal_access(data_buf, start_sector, num_sector, mode);
	} else {
		err = csk_sdmmc_access_inner_sram(disk, data_buf, start_sector, num_sector, mode);
	}

	if (err != ERR_SD_NO_ERROR) {
		LOG_ERR("sd read single block failed %d", err);
	}

	return (err != ERR_SD_NO_ERROR) ? -EIO : 0;
}
#endif

static int csk6_sdmmc_access_read(struct disk_info *disk, uint8_t *data_buf,
				   uint32_t start_sector, uint32_t num_sector)
{
	uint32_t err;

	wait_init_done();
	LOG_DBG("%s start_sector=%d num_sector=%d", __FUNCTION__, start_sector, num_sector);

	k_mutex_lock(&sdmmc_mutex, K_FOREVER);

#ifndef CONFIG_SDMMC_CSK6_SRAM_CACHE_ACCESS
	const struct device *dev = disk->dev;
	uint8_t *p = data_buf;
	struct sdmmc_csk_data *data = dev->data;
	if((((uint32_t)p % _ALIGEN) != 0) || !is_addr_in_sdram((uint32_t)p)){
		for(uint32_t i = start_sector; i < start_sector + num_sector; i++){
			err = gm_sdc_api_sdcard_sector_read(SD_0, i, 1, sdcard_r_buf);
			if (err != ERR_SD_NO_ERROR) {
				k_mutex_unlock(&sdmmc_mutex);
				LOG_ERR("sd read single block failed %d", err);
				return -EIO;
			}

			memcpy(p, sdcard_r_buf, data->blk_size);

			p += data->blk_size;
		}

	}else{
		err = gm_sdc_api_sdcard_sector_read(SD_0, start_sector, num_sector, data_buf);

		if (err != ERR_SD_NO_ERROR) {
			k_mutex_unlock(&sdmmc_mutex);
			LOG_ERR("sd read block failed %d", err);
			return -EIO;
		}
	}
#else
	err = csk6_sdmmc_access(disk, data_buf, start_sector, num_sector,
				SDMMC_CSK6_ACCESS_MODE_READ);
	if (err) {
		return err;
	}
#endif

	k_mutex_unlock(&sdmmc_mutex);
	return 0;
}

static int csk6_sdmmc_access_write(struct disk_info *disk,
				    const uint8_t *data_buf,
				    uint32_t start_sector, uint32_t num_sector)
{
	uint32_t err;

	wait_init_done();
	LOG_DBG("%s start_sector=%d num_sector=%d", __FUNCTION__, start_sector, num_sector);

	k_mutex_lock(&sdmmc_mutex, K_FOREVER);

#ifndef CONFIG_SDMMC_CSK6_SRAM_CACHE_ACCESS
	const struct device *dev = disk->dev;
	uint8_t *p = (uint8_t *)data_buf;
	struct sdmmc_csk_data *data = dev->data;
	if(((uint32_t)p % _ALIGEN) != 0 || !is_addr_in_sdram((uint32_t)p)){
		for(uint32_t i = start_sector; i < start_sector + num_sector; i++){
			memcpy(sdcard_w_buf, p, data->blk_size);
			err = gm_sdc_api_sdcard_sector_write(SD_0, i, 1, sdcard_w_buf);
			if (err != ERR_SD_NO_ERROR) {
				k_mutex_unlock(&sdmmc_mutex);
				LOG_ERR("sd write single block failed %d", err);
				return -EIO;
			}
			p += data->blk_size;
		}
	}else{
		err = gm_sdc_api_sdcard_sector_write(SD_0, start_sector, num_sector, (void *)data_buf);
		
		if (err != ERR_SD_NO_ERROR) {
			k_mutex_unlock(&sdmmc_mutex);
			LOG_ERR("sd write block failed %d", err);
			return -EIO;
		}
	}
#else
	err = csk6_sdmmc_access(disk, (uint8_t *)data_buf, start_sector, num_sector,
				SDMMC_CSK6_ACCESS_MODE_WRITE);
	if (err) {
		return err;
	}
#endif
	k_mutex_unlock(&sdmmc_mutex);
	return 0;
}

static int csk6_sdmmc_access_ioctl(struct disk_info *disk, uint8_t cmd,
				    void *buff)
{
	uint32_t blk_len, blk_num, erase_size;
	uint32_t err;
	wait_init_done();
	k_mutex_lock(&sdmmc_mutex, K_FOREVER);
	err = lib_sdc_get_card_info(SD_0, &blk_len, &blk_num, &erase_size);
	if (err != ERR_SD_NO_ERROR) {
		k_mutex_unlock(&sdmmc_mutex);
		return -EIO;
	}
	k_mutex_unlock(&sdmmc_mutex);
	switch (cmd) {
		case DISK_IOCTL_GET_SECTOR_COUNT:
			*(uint32_t *)buff = blk_num;
			break;
		case DISK_IOCTL_GET_SECTOR_SIZE:
			*(uint32_t *)buff = blk_len;
			break;
		case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
			*(uint32_t *)buff = erase_size;
			break;
		case DISK_IOCTL_CTRL_SYNC:
			break;
			/* we use a blocking API, so nothing to do for sync */
		default:
			return -EINVAL;
	}

	return 0;
}

static int csk6_sdmmc_bus_probe(const struct device *dev)
{
	uint32_t err;
	const struct sdmmc_csk_config *cfg = dev->config;
	struct sdmmc_csk_data *data = dev->data;
	k_mutex_lock(&sdmmc_mutex, K_FOREVER);
	gm_api_sdc_platform_init((SDC_OPTION_ENABLE | SDC_OPTION_CD_INVERT), 0, NullSetting, (uint32_t) sdcard_buf);
	gm_sdc_api_action(SD_0, GM_SDC_ACTION_REG_SDIO_APP_INIT, (void*)NULL, NULL);

	err = gm_sdc_api_action(SD_0, GM_SDC_ACTION_CARD_DETECTION, NULL, NULL);
    if (err != ERR_SD_NO_ERROR) {
		k_mutex_unlock(&sdmmc_mutex);
		LOG_ERR(" NO sdcard detect!! %d\r\n", err);
        return -EIO;
    }

	uint32_t card_type = 0;
    err = gm_sdc_api_action(SD_0, GM_SDC_ACTION_GET_CARD_TYPE, NULL, &card_type);
    if (err != ERR_SD_NO_ERROR || (card_type != MEMORY_CARD_TYPE_SD && card_type != MEMORY_CARD_TYPE_MMC)) {
		k_mutex_unlock(&sdmmc_mutex);
        LOG_ERR(" NOT sd/mmc card (card_type = %u)!! \r\n", card_type);
        return -EIO;
    }

    uint32_t bus_width = cfg->bit_bus; 
    gm_sdc_api_action(SD_0, GM_SDC_ACTION_SET_BUS_WIDTH, &bus_width, NULL);

	err = lib_sdc_get_card_info(SD_0, &data->blk_size, &data->blk_num, &data->erase_size);
	if (err != ERR_SD_NO_ERROR) {
		k_mutex_unlock(&sdmmc_mutex);
		return -EIO;
	}

	k_mutex_unlock(&sdmmc_mutex);
	return 0;
}

static int disk_csk6_sdmmc_init(const struct device *dev)
{
	const struct sdmmc_csk_config *cfg = dev->config;
	int err = 0;

#ifdef CONFIG_SDMMC_CSK6_SRAM_CACHE_ACCESS
	uint8_t i;
	for (i = 0; i < CONFIG_SDMMC_CSK6_SRAM_CACHE_NUM; i++) {
		fifo_items[i].buffer = &sdmmc_sram_cache[i][0];
		k_fifo_put(&sdmmc_queue_psram2sram, &fifo_items[i]);
	}
#endif

	pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);

	cfg->irq_config_func(dev);
#ifdef CONFIG_SDMMC_NON_BLOCK_INIT
	s_init_dev = dev;
	k_work_queue_init(&sdmmc_workq);
	k_work_queue_start(&sdmmc_workq, sdmmc_workq_stack_area,
						K_THREAD_STACK_SIZEOF(sdmmc_workq_stack_area), K_LOWEST_APPLICATION_THREAD_PRIO, NULL);
	k_work_init(&init_work, init_work_handler);
	k_work_submit_to_queue(&sdmmc_workq, &init_work);
#else
	err = csk6_sdmmc_bus_probe(dev);
	if (err < 0) {
		return err;
	}
#endif

	csk6_sdmmc_info.dev = dev;
	err = disk_access_register(&csk6_sdmmc_info);
	if (err) {
		return err;
	}

	return 0;
}

static void csk6_sdmmc_irq_config_func(const struct device *dev)
{
	IRQ_CONNECT(DT_INST_IRQN(0),
		DT_INST_IRQ(0, priority),
		csk6_sdmmc_isr, DEVICE_DT_INST_GET(0),
		0);
	irq_enable(DT_INST_IRQN(0));
}

#define DISK_ACCESS_SDMMC_INIT(n)						\
    PINCTRL_DT_INST_DEFINE(n);                           \
			                                                            \
	static const struct sdmmc_csk_config sdmmc_config_##n = {			\
		.irq_config_func = csk6_sdmmc_irq_config_func,                    \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                         \
		.bit_bus = DT_INST_PROP(n, bit_bus),                                \
	};									\
										\
	static struct sdmmc_csk_data sdmmc_data_##n = {};				\
										\
	DEVICE_DT_INST_DEFINE(n,						\
			&disk_csk6_sdmmc_init,					\
			NULL,							\
			&sdmmc_data_##n,					\
			&sdmmc_config_##n,					\
			POST_KERNEL,						\
			CONFIG_SDMMC_INIT_PRIORITY,				\
			NULL);

DT_INST_FOREACH_STATUS_OKAY(DISK_ACCESS_SDMMC_INIT)
