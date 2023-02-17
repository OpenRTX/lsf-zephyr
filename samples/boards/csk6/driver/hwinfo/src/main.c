/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <zephyr/drivers/hwinfo.h>
#include <string.h>


void main(void)
{
    uint32_t buffer[3] = {0};
    uint64_t uuid = 0;
    uint32_t chip_type = 0;
    size_t length = 0;

    length = hwinfo_get_device_id((uint8_t *)&buffer, sizeof(buffer));

    if(length != sizeof(buffer)){
        printk("length error (%d)\n", length);
        return;
    }

    memcpy((void*)&uuid, (void*)buffer, sizeof(uuid));

    printk("uuid: 0x%llx\n", uuid);

    memcpy((void*)&chip_type, (void*)&buffer[2], sizeof(chip_type));

    printk("chip type: %x\n", chip_type);

    printk("get device id success\n");
}
