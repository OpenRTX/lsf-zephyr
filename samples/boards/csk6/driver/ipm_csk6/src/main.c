/*
 * Copyright (c) 2021, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/ipm.h>
#include "hal/PSRAMManager.h"

/*between 4byte and 16byte*/
#define IPM_CSK_DATA_SIZE 16
#define IPM_CSK_DATA_REGS 4

static void ipm_callback(const struct device *dev, void *context,
			 uint32_t id, volatile void *data)
{
	int i;
	uint32_t *data32 = (uint32_t *)data;

	printk("[MASTER]: %s: id = %u, data = ", __func__, id);
	for (i = 0; i < (IPM_CSK_DATA_SIZE / IPM_CSK_DATA_REGS); i++) {
		printk(" %d ", data32[i]);
	}
	printk("\n");
}

void main(void)
{
	PSRAM_Initialize();
	const struct device *ipm;
	/* 32bit data register */
	uint32_t data32[IPM_CSK_DATA_SIZE / IPM_CSK_DATA_REGS] = {0};
	ipm = DEVICE_DT_GET(DT_NODELABEL(mailbox0));
	if (!device_is_ready(ipm)) {
		while (1) {
		}
	}
	ipm_register_callback(ipm, ipm_callback, NULL);
	ipm_set_enabled(ipm, 1);
	printk("IPM initialized, data size = %d\n",
		IPM_CSK_DATA_SIZE);
	while (1) {
		for (uint8_t i = 0; i < (IPM_CSK_DATA_SIZE / 4); i++) {
			data32[i]++;
		}
		printk("data send [MASTER]--->[REMOTE]\n");
		for (uint8_t i = 0; i < 17; i++){
			ipm_send(ipm, 1, i, data32, IPM_CSK_DATA_SIZE);
			k_msleep(1000);
		}
	}
}
