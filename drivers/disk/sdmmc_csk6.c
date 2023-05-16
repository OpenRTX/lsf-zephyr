/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_sdmmc

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/disk.h>
#include <zephyr/logging/log.h>
#include "hal/sdmmc/lib_sdc.h"
#include "hal/sdmmc/ftsdc021.h"
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

#ifdef CONFIG_DRIVER_NON_BLOCK_INIT
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

static bool is_addr_in_sdram(uint32_t addr)
{
	uint32_t sram_base = DT_REG_ADDR(DT_CHOSEN(zephyr_sram));
	uint32_t sram_size = DT_REG_SIZE(DT_CHOSEN(zephyr_sram));

	if(addr >= sram_base && addr < sram_base + sram_size){
		return true;
	}
	return false;
}

static int csk6_sdmmc_access_read(struct disk_info *disk, uint8_t *data_buf,
				   uint32_t start_sector, uint32_t num_sector)
{
	uint32_t err;
	uint8_t *p = data_buf;
	const struct device *dev = disk->dev;
	struct sdmmc_csk_data *data = dev->data;

	wait_init_done();
	LOG_DBG("%s start_sector=%d num_sector=%d", __FUNCTION__, start_sector, num_sector);

	k_mutex_lock(&sdmmc_mutex, K_FOREVER);


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

	k_mutex_unlock(&sdmmc_mutex);
	return 0;
}

static int csk6_sdmmc_access_write(struct disk_info *disk,
				    const uint8_t *data_buf,
				    uint32_t start_sector, uint32_t num_sector)
{
	uint32_t err;
	uint8_t *p = (uint8_t *)data_buf;
	const struct device *dev = disk->dev;
	struct sdmmc_csk_data *data = dev->data;

	wait_init_done();
	LOG_DBG("%s start_sector=%d num_sector=%d", __FUNCTION__, start_sector, num_sector);

	k_mutex_lock(&sdmmc_mutex, K_FOREVER);

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

	pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);

	cfg->irq_config_func(dev);
#ifdef CONFIG_DRIVER_NON_BLOCK_INIT
	s_init_dev = dev;
	k_work_queue_init(&sdmmc_workq);
	k_work_queue_start(&sdmmc_workq, sdmmc_workq_stack_area,
						K_THREAD_STACK_SIZEOF(sdmmc_workq_stack_area), 1, NULL);
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
