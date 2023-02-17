/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <zephyr/drivers/hwinfo.h>

void main(void)
{
    uint32_t chip_id[2] = {0};
    size_t length = 0;

    length = hwinfo_get_device_id((uint8_t *)&chip_id, sizeof(chip_id));
    printk("length: %d id: 0x%x 0x%x\r", length, chip_id[0], chip_id[1]);
}
