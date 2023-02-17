/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "venus_ap.h"


#include <zephyr/drivers/hwinfo.h>
#include <string.h>
#include <zephyr/init.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_hwinfo);

EFUSE_RegDef *efuse = (EFUSE_RegDef *)AON_EFUSECTRL_BASE;
ANA_RegDef *ana_aon_regs = (ANA_RegDef *)AON_AON_BASE;
AON_RegDef *aon_pmuctrl_regs = (AON_RegDef *)AON_PMUCTRL_BASE;

struct csk6_uid {
	uint32_t id[2];
};

static int8_t efuse_read_word(uint8_t addr, uint32_t *val)
{
	int8_t ret = -1;

	if(addr < 0x10) {
		efuse->REG_CMD_CTL.bit.EFU_CMD_ADDR = addr;
		efuse->REG_CMD_CTL.bit.EFU_CMD_TYPE = 0;  //read mode
		efuse->REG_CMD_CTL.bit.EFU_CMD_START = 1;
		while(efuse->REG_CMD_CTL.bit.EFU_CMD_START);
		*val = efuse->REG_RD_DATA.all;
		ret = 0;
	}
	return ret;
}

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
    struct csk6_uid dev_id;

    if (length > sizeof(dev_id.id)) {
        length = sizeof(dev_id.id);
    }

	if(efuse_read_word(2, &dev_id.id[0]) !=0 || efuse_read_word(3, &dev_id.id[1]))
	{
		LOG_ERR("can't read efuse registers\n");
		return 0;
	}

    memcpy(buffer, dev_id.id, length);

    return length;
}

static int hwinfo_csk6_init(const struct device *arg)
{
//  enable 1.8v for efuse
	ana_aon_regs->REG_0X038.bit.LDO25VOSEL = 0;
	ana_aon_regs->REG_0X038.bit.LDO25_EN = 1;

//  select pclk for efuse
	aon_pmuctrl_regs->REG_AON_CRM_CTRL1.bit.SEL_EFUSE_CLK = 1;
	aon_pmuctrl_regs->REG_AON_CRM_CTRL1.bit.ENA_EFUSE_CLK = 1;

	return 0;
}

SYS_INIT(hwinfo_csk6_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
