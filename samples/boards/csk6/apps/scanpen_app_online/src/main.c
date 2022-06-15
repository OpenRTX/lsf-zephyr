/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <net/net_pkt.h>

extern void s_fs_mount(void);
extern void mcumgr_domain();
extern void sof_domain(void);
extern void network_domain(void);
extern void start_scanpen_online();
extern void lisa_heap_info(void);

#define PRINT_HEAP_INFO()  {extern struct k_heap _system_heap;sys_heap_print_info(&_system_heap.heap,0);}
void main(void)
{

	printk("sizeof(struct net_pkt)=%d bytes \n",sizeof(struct net_pkt));
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

	start_scanpen_online();

	while (1) {
		lisa_heap_info();
		PRINT_HEAP_INFO();
		k_msleep(2000);

	}
}

