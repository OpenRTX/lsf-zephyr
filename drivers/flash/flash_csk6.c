/*
 * Copyright (c) 2021 LISTENAI Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_flash_controller
#define SOC_NV_FLASH_NODE DT_INST(0, soc_nv_flash)

#define FLASH_WRITE_BLK_SZ DT_PROP(SOC_NV_FLASH_NODE, write_block_size)
#define FLASH_ERASE_BLK_SZ DT_PROP(SOC_NV_FLASH_NODE, erase_block_size)
#define FLASH_CHIP_SZ DT_REG_SIZE_BY_IDX(SOC_NV_FLASH_NODE, 0)
#define FLASH_BASE_ADDRESS DT_REG_ADDR_BY_IDX(SOC_NV_FLASH_NODE, 0)

#include <string.h>
#include <kernel.h>
#include <cache.h>
#include <drivers/flash.h>
#include <venus_ap.h>
#include <spiflash.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(flash_csk6, CONFIG_FLASH_LOG_LEVEL);


#define AP_DMAC_BASE               	   (0x40000000) // size=1MB // AP DMA
#define AP_DMAC_SAR_CH0_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0))
#define AP_DMAC_SAR_CH1_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0x58))
#define AP_DMAC_SAR_CH2_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0xb0))
#define AP_DMAC_SAR_CH3_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0x108))
#define AP_DMAC_SAR_CH4_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0x160))
#define AP_DMAC_SAR_CH5_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0x1b8))
#define AP_DMAC_SAR_CH6_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0x210))
#define AP_DMAC_SAR_CH7_REG            (*(volatile uint32_t*)(AP_DMAC_BASE + 0x268))
#define AP_DMAC_CH_EN_REG              (*(volatile uint32_t*)(AP_DMAC_BASE + 0x3a0))

#define AP_FLASH_SADDR                 (0x18000000)
#define AP_FLASH_EADDR                 (0x20000000-1)
#define IS_AP_FLASH_RANGE(address) (((address >= AP_FLASH_SADDR)&&(address <= AP_FLASH_EADDR))?1:0)

#define AP_DMAC_WAIT_BITS()\
((IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH0_REG)<<0)|\
(IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH1_REG)<<1)|\
(IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH2_REG)<<2)|\
(IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH3_REG)<<3)|\
(IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH4_REG)<<4)|\
(IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH5_REG)<<5)|\
(IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH6_REG)<<6)|\
(IS_AP_FLASH_RANGE(AP_DMAC_SAR_CH7_REG)<<7))



#if (CONFIG_CSK6_FLASH_STALL_SECOND_CORE)
#define CP_DMAC_BASE               	   (0x4A200000) // size=1MB // CP DMA
#define CP_DMAC_SAR_CH0_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0))
#define CP_DMAC_SAR_CH1_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0x58))
#define CP_DMAC_SAR_CH2_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0xb0))
#define CP_DMAC_SAR_CH3_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0x108))
#define CP_DMAC_SAR_CH4_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0x160))
#define CP_DMAC_SAR_CH5_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0x1b8))
#define CP_DMAC_SAR_CH6_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0x210))
#define CP_DMAC_SAR_CH7_REG            (*(volatile uint32_t*)(CP_DMAC_BASE + 0x268))
#define CP_DMAC_CH_EN_REG              (*(volatile uint32_t*)(CP_DMAC_BASE + 0x3a0))

#define CP_FLASH_SADDR                 (0x68000000)
#define CP_FLASH_EADDR                 (0x70000000-1)
#define IS_CP_FLASH_RANGE(address) (((address >= CP_FLASH_SADDR)&&(address <= CP_FLASH_EADDR))?1:0)

#define CP_DMAC_WAIT_BITS()\
((IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH0_REG)<<0)|\
(IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH1_REG)<<1)|\
(IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH2_REG)<<2)|\
(IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH3_REG)<<3)|\
(IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH4_REG)<<4)|\
(IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH5_REG)<<5)|\
(IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH6_REG)<<6)|\
(IS_CP_FLASH_RANGE(CP_DMAC_SAR_CH7_REG)<<7))

#define FLASH_OPT_ENTER() \
	uint32_t primask = __get_PRIMASK();\
	__set_PRIMASK(1);\
	((SYSCFG_RegDef *)CMN_SYSCTRL_BASE)->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;\
	while(AP_DMAC_CH_EN_REG & AP_DMAC_WAIT_BITS());\
	while(CP_DMAC_CH_EN_REG & CP_DMAC_WAIT_BITS());

#define FLASH_OPT_EXIT() \
	((SYSCFG_RegDef *)CMN_SYSCTRL_BASE)->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;\
	__set_PRIMASK(primask);
#else

#define FLASH_OPT_ENTER() \
	uint32_t primask = __get_PRIMASK();\
	__set_PRIMASK(1);\
	while(AP_DMAC_CH_EN_REG & AP_DMAC_WAIT_BITS());

#define FLASH_OPT_EXIT() \
	__set_PRIMASK(primask);

#endif

typedef struct {
	FLASH_DEV flash_dev;
} csk6_flash_config_t;

csk6_flash_config_t csk6_flash_config = { .flash_dev = {
						  .base_addr = DT_INST_REG_ADDR(0),
						  .d_width = 4,
						  .sclk_div = 0xff, // 0xff: special div in chip,indicates that sclk frequency = SPI clock frequency
						  .run_mod = RUN_WITH_INT,
						  .timeout = 2000000,
					  } };

static const struct flash_parameters flash_csk6_parameters = {
	.write_block_size = FLASH_WRITE_BLK_SZ,
	.erase_value = 0xff,
};

static int flash_csk6_read(const struct device *dev, off_t address, void *buffer, size_t length)
{
	if (length == 0) {
		return 0;
	}
	if (buffer == NULL || address > FLASH_CHIP_SZ || address + length > FLASH_CHIP_SZ) {
		return -EINVAL;
	}
	sys_cache_data_range((void*)(FLASH_BASE_ADDRESS+address), length, K_CACHE_INVD);
	memcpy(buffer,(void*)(FLASH_BASE_ADDRESS+address),length);
	return 0;
}

/*If from data address in soc flash area ,copy to ram area first*/
#define ADDRESS_IS_IN_SOC_FLASH_AREA(address) (((address >= FLASH_BASE_ADDRESS)&&(address < FLASH_BASE_ADDRESS + FLASH_CHIP_SZ))?1:0)
static int inline flash_csk6_write_with_cp(const struct device *dev, off_t address, const void *buffer,size_t length){

	char cbuffer[32];
	char* pos = (char*) buffer;
	char* pTail = (char*)buffer + length;
	int iret = 0;
	int32_t cpLen = 0;

	do{
		cpLen = (pTail - pos)>sizeof(cbuffer)?sizeof(cbuffer):(pTail - pos);
		memcpy(cbuffer,pos,cpLen);
		if (0 != sflash_write((void *)dev->config, FLASH_BASE_ADDRESS + address+(pos - (char*)buffer), cbuffer, cpLen)) {
			iret = -EIO;
			break;
		}
		pos += cpLen;
	}while((uint32_t)pos < (uint32_t)buffer + length);

	return iret;
}
static int flash_csk6_write(const struct device *dev, off_t address, const void *buffer,
			       size_t length)
{
	int iret = 0;

	if (length == 0) {
		return 0;
	}
	if (buffer == NULL || address > FLASH_CHIP_SZ || address + length > FLASH_CHIP_SZ) {
		return -EINVAL;
	}

	FLASH_OPT_ENTER();
	sflash_write_protection_set((void *)dev->config, false);
	if(0 != ADDRESS_IS_IN_SOC_FLASH_AREA((uint32_t)buffer)){
		iret = flash_csk6_write_with_cp(dev,address,buffer,length);
	}
	else{
		if (0 != sflash_write((void *)dev->config, FLASH_BASE_ADDRESS + address, buffer, length)) {
			iret = -EIO;
		} else {
			iret = 0;
		}
	}
	sflash_write_protection_set((void *)dev->config, true);
	FLASH_OPT_EXIT();

	return iret;
}

static int flash_csk6_erase(const struct device *dev, off_t start, size_t len)
{
	int iret;

	if (start > FLASH_CHIP_SZ || start + len > FLASH_CHIP_SZ) {
		return -EINVAL;
	}

	if ((start % FLASH_ERASE_BLK_SZ != 0) || (len % FLASH_ERASE_BLK_SZ != 0)) {
		LOG_ERR(" address and length must be aligned with %d bytes", FLASH_ERASE_BLK_SZ);
		return -EINVAL;
	}

	FLASH_OPT_ENTER();
	sflash_write_protection_set((void *)dev->config, false);

	if (0 != sflash_erase((void *)dev->config, FLASH_BASE_ADDRESS + start, len)) {
		iret = -EIO;
	} else {
		iret = 0;
	}
	sflash_write_protection_set((void *)dev->config, true);

	FLASH_OPT_EXIT();

	return iret;
}

#if CONFIG_FLASH_PAGE_LAYOUT
static const struct flash_pages_layout flash_csk6_pages_layout = {
	.pages_count = DT_REG_SIZE(SOC_NV_FLASH_NODE) / FLASH_ERASE_BLK_SZ,
	.pages_size = DT_PROP(SOC_NV_FLASH_NODE, erase_block_size),
};

void flash_csk6_page_layout(const struct device *dev, const struct flash_pages_layout **layout,
			       size_t *layout_size)
{
	*layout = &flash_csk6_pages_layout;
	*layout_size = 1;
}
#endif /* CONFIG_FLASH_PAGE_LAYOUT */

static const struct flash_parameters *flash_csk6_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	return &flash_csk6_parameters;
}

static int flash_csk6_init(const struct device *dev)
{
	FLASH_OPT_ENTER();
	sflash_init((void *)dev->config);
	FLASH_OPT_EXIT();
	return 0;
}

#ifdef CONFIG_FLASH_CSK6_LOCK
int flash_csk6_lock(const struct device *dev)
{
	int iret;
	FLASH_OPT_ENTER();
	sflash_write_protection_set((void *)dev->config, false);

	if (0 != sflash_lock((void *)dev->config)) {
		iret = -EIO;
	} else {
		iret = 0;
	}
	
	sflash_write_protection_set((void *)dev->config, true);
	FLASH_OPT_EXIT();
	return iret;
}

int flash_csk6_unlock(const struct device *dev)
{
	int iret;
	FLASH_OPT_ENTER();
	sflash_write_protection_set((void *)dev->config, false);

	if (0 != sflash_unlock((void *)dev->config)) {
		iret = -EIO;
	} else {
		iret = 0;
	}

	sflash_write_protection_set((void *)dev->config, true);
	FLASH_OPT_EXIT();
	return iret;
}

int flash_csk6_get_lock_status(const struct device *dev)
{
	int status;
	FLASH_OPT_ENTER();
	sflash_write_protection_set((void *)dev->config, false);

	status = sflash_lock_status((void *)dev->config);

	sflash_write_protection_set((void *)dev->config, true);
	FLASH_OPT_EXIT();

	return status;
}
#endif

int32_t flash_csk6_read_id(const struct device *dev)
{
	int32_t id = 0;
	FLASH_OPT_ENTER();
	sflash_write_protection_set((void *)dev->config, false);

	id = sflash_read_id((void *)dev->config);

	sflash_write_protection_set((void *)dev->config, true);
	FLASH_OPT_EXIT();
	return id;
}


static const struct flash_driver_api flash_csk6_driver_api = {
	.read = flash_csk6_read,
	.write = flash_csk6_write,
	.erase = flash_csk6_erase,
	.get_parameters = flash_csk6_get_parameters,
#ifdef CONFIG_FLASH_CSK6_LOCK
	.lock = flash_csk6_lock,
	.unlock = flash_csk6_unlock,
	.get_lock_status = flash_csk6_get_lock_status,
#endif
	.read_id = flash_csk6_read_id,
#ifdef CONFIG_FLASH_PAGE_LAYOUT
	.page_layout = flash_csk6_page_layout,
#endif
};

DEVICE_DT_INST_DEFINE(0, flash_csk6_init, NULL, NULL, &csk6_flash_config, POST_KERNEL,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &flash_csk6_driver_api);
