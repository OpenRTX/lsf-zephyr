#include <stdlib.h>
#include <zephyr.h>
#include <init.h>
#include <kernel.h>
#include <sys/printk.h>

#include "tester_lisa.h"


struct k_sem g_wifi_ip_got;

void main(void)
{
	printk("lisa porting test on %s !\n", CONFIG_BOARD);
	k_sem_init(&g_wifi_ip_got, 0, 1);
	_wifi_init();
	//k_sleep(K_MSEC(5000));
	k_sem_take(&g_wifi_ip_got,K_FOREVER);	
	
	//lisa_https_test();
	//  lisa_os_test();
	lisa_websocket_test();

	while (1) {
		k_sleep(K_MSEC(5000));
		printk("lisa porting test on !\n");
	}
}

