/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

#include "lisa_buttons.h"

extern void s_fs_mount(void);
extern void mcumgr_domain();
extern void sof_domain(void);
extern void network_domain(void);

void main(void)
{
	printk("lisa porting test on %s !\n", CONFIG_BOARD);
	//s_fs_mount();

#ifdef CONFIG_WIFI
	network_domain();
#endif

	k_msleep(5000);
	//sof_domain();

#ifdef CONFIG_MCUMGR
	mcumgr_domain();
#endif

	k_msleep(10000);
	//start_scanpen_app();
	while (1) {
		printk("the main task is running!!\n");
		k_msleep(1000);
	}
}

