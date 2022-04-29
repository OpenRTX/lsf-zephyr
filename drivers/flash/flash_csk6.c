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
#include <drivers/flash.h>
#include "hal/spiflash.h"

#define FALSH_DISABLE_IRQ (1)
#if FALSH_DISABLE_IRQ
#define FLASH_OPT_ENTER() int flash_irq_key = irq_lock()
#define FLASH_OPT_EXIT() irq_unlock(flash_irq_key)
#else
#define FLASH_OPT_ENTER()
#define FLASH_OPT_EXIT()
#endif

typedef struct {
	FLASH_DEV flash_dev;
} csk6_flash_config_t;

csk6_flash_config_t csk6_flash_config = { .flash_dev = {
							.base_addr = DT_INST_REG_ADDR(0),
							.spi_enum = FLASH_SPI_1_INN,
							.w_protect = false,
						} };

static const struct flash_parameters flash_csk6_parameters = {
	.write_block_size = FLASH_WRITE_BLK_SZ,
	.erase_value = 0xff,
};

static int flash_csk6_read(const struct device *dev, off_t address, void *buffer, size_t length)
{
	int iret = 0;
	if (length == 0) {
		return 0;
	}
	if (buffer == NULL || address > FLASH_CHIP_SZ || address + length > FLASH_CHIP_SZ) {
		return -EINVAL;
	}
	FLASH_OPT_ENTER();
	if (0 != sflash_read((void *)dev->config, FLASH_BASE_ADDRESS + address, buffer, length)) {
		iret = -EIO;
	}
	FLASH_OPT_EXIT();
	return iret;
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
	unsigned int key;
	key = irq_lock();
	sflash_init();
	irq_unlock(key);
	return 0;
}

static const struct flash_driver_api flash_csk6_driver_api = {
	.read = flash_csk6_read,
	.write = flash_csk6_write,
	.erase = flash_csk6_erase,
	.get_parameters = flash_csk6_get_parameters,
#ifdef CONFIG_FLASH_PAGE_LAYOUT
	.page_layout = flash_csk6_page_layout,
#endif
};

DEVICE_DT_INST_DEFINE(0, flash_csk6_init, NULL, NULL, &csk6_flash_config, POST_KERNEL,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &flash_csk6_driver_api);
