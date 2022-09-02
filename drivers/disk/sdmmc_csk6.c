/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_sdmmc

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/disk.h>
#include <zephyr/drivers/pinmux.h>
#include <soc_dt.h>
#include <zephyr/logging/log.h>
#include "hal/sdmmc/lib_sdc.h"
#include "hal/sdmmc/ftsdc021.h"

LOG_MODULE_REGISTER(csk6_sdmmc, CONFIG_SDMMC_LOG_LEVEL);

struct sdmmc_csk_config {
	void (*irq_config_func)(const struct device *dev);

	/* I2C alternate configuration */
	const struct csk6_pinctrl_cfg *pinctrl_list;
	uint8_t pinctrl_list_size;

	uint8_t bit_bus;
};

struct sdmmc_csk_data {
	/* driver handle */
	void *handle; 
};


static uint8_t sdcard_buf[sizeof(SDCardInfo)]     __attribute__ ((aligned(4)));

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

static int csk6_sdmmc_access_read(struct disk_info *disk, uint8_t *data_buf,
				   uint32_t start_sector, uint32_t num_sector)
{
	uint32_t err;

	LOG_DBG("%s start_sector=%d num_sector=%d", __FUNCTION__, start_sector, num_sector);

	err = gm_sdc_api_sdcard_sector_read(SD_0, start_sector, num_sector, data_buf);

	if (err != ERR_SD_NO_ERROR) {
		LOG_ERR("sd read block failed %d", err);
		return -EIO;
	}

	return 0;
}

static int csk6_sdmmc_access_write(struct disk_info *disk,
				    const uint8_t *data_buf,
				    uint32_t start_sector, uint32_t num_sector)
{
	uint32_t err;

	LOG_DBG("%s start_sector=%d num_sector=%d", __FUNCTION__, start_sector, num_sector);

	err = gm_sdc_api_sdcard_sector_write(SD_0, start_sector, num_sector, (void *)data_buf);
	
	if (err != ERR_SD_NO_ERROR) {
		LOG_ERR("sd write block failed %d", err);
		return -EIO;
	}

	return 0;
}

static int csk6_sdmmc_access_ioctl(struct disk_info *disk, uint8_t cmd,
				    void *buff)
{
	uint32_t blk_len, blk_num, erase_size;
	uint32_t err;

	err = lib_sdc_get_card_info(SD_0, &blk_len, &blk_num, &erase_size);
	if (err != ERR_SD_NO_ERROR) {
		return -EIO;
	}

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
			/* we use a blocking API, so nothing to do for sync */
		default:
			return -EINVAL;
	}

	return 0;
}

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

int csk6_sdmmc_bus_probe(const struct device *dev)
{
	uint32_t err;
	const struct sdmmc_csk_config *cfg = dev->config;
	gm_api_sdc_platform_init((SDC_OPTION_ENABLE | SDC_OPTION_CD_INVERT), 0, NullSetting, (uint32_t) sdcard_buf);
	gm_sdc_api_action(SD_0, GM_SDC_ACTION_REG_SDIO_APP_INIT, (void*)NULL, NULL);

	err = gm_sdc_api_action(SD_0, GM_SDC_ACTION_CARD_DETECTION, NULL, NULL);
    if (err != ERR_SD_NO_ERROR) {
		LOG_ERR(" NO sdcard detect!! %d\r\n", err);
        return -EIO;
    }

	uint32_t card_type = 0;
    err = gm_sdc_api_action(SD_0, GM_SDC_ACTION_GET_CARD_TYPE, NULL, &card_type);
    if (err != ERR_SD_NO_ERROR || (card_type != MEMORY_CARD_TYPE_SD && card_type != MEMORY_CARD_TYPE_MMC)) {
        LOG_ERR(" NOT sd/mmc card (card_type = %u)!! \r\n", card_type);
        return -EIO;
    }

    uint32_t bus_width = cfg->bit_bus; 
    gm_sdc_api_action(SD_0, GM_SDC_ACTION_SET_BUS_WIDTH, &bus_width, NULL);

	return 0;
}

static int disk_csk6_sdmmc_init(const struct device *dev)
{
	const struct sdmmc_csk_config *cfg = dev->config;
	int err = 0;

	PINCTRL_CONFIG_PINS(cfg->pinctrl_list, cfg->pinctrl_list_size);

	cfg->irq_config_func(dev);

	err = csk6_sdmmc_bus_probe(dev);
	if (err < 0) {
		return err;
	}    


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
	static const struct csk6_pinctrl_cfg                                       						\
		sdmmc_alts_##n[DT_INST_NUM_PINCTRLS_BY_IDX(n, 0)] =          									\
			CSK6_DT_ALT_ITEMS_LIST(n);                        										\
			                                                            \
	static const struct sdmmc_csk_config sdmmc_config_##n = {			\
		.irq_config_func = csk6_sdmmc_irq_config_func,                    \
		.pinctrl_list = sdmmc_alts_##n, 													   \
		.pinctrl_list_size = ARRAY_SIZE(sdmmc_alts_##n),									   \
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