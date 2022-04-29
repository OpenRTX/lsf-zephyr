/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>

extern void s_fs_mount(void);
extern void mcumgr_domain();
extern void sof_domain(void);
extern void network_domain(void);

void main(void)
{
	s_fs_mount();
	
#ifdef CONFIG_WIFI
	network_domain();
#endif

	sof_domain();

#ifdef CONFIG_MCUMGR
	mcumgr_domain();
#endif

	while (1) {
		k_msleep(1000);
	}
}

