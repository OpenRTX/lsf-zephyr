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
#ifdef CONFIG_SOF_HOST
	s_fs_mount();
#endif

/*Initialization of the SOF must be performed before wifi is enabled，
**because of heap space limitations,*/
#ifdef CONFIG_SOF_HOST
	sof_domain();
#endif


#ifdef CONFIG_WIFI
	network_domain();
#endif


#ifdef CONFIG_MCUMGR
	mcumgr_domain();
#endif

	while (1) {
		k_msleep(1000);
	}
}

