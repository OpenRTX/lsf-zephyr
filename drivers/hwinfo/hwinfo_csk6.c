/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "venus_ap.h"

#include <zephyr/drivers/hwinfo.h>
#include <string.h>

#define CHIP_ID_LENGTH  (4)

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
    uint32_t chip_id = IP_SYSCTRL->REG_AON_WAIT_XTAL.bit.CHIP_ID;

    printk("chip_id: 0x%x \r\n", chip_id);

    if (length > CHIP_ID_LENGTH) {
        length = CHIP_ID_LENGTH;
    }

    memcpy(buffer, &chip_id, length);

    return length;
}
