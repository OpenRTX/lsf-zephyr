/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

extern void s_fs_mount(void);
extern void sof_domain(void);


void main(void)
{
	s_fs_mount();
#if CONFIG_SOF_HOST
	sof_domain();
#endif
	while (1) {
		k_msleep(1000);
	}
}

