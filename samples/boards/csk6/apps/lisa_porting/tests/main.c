#include <stdlib.h>
#include <zephyr.h>
#include <init.h>
#include <sys/printk.h>

#include "tester_lisa.h"




void main(void)
{
	printk("lisa porting test on %s !\n", CONFIG_BOARD);

	_wifi_init();
	k_sleep(K_MSEC(5000));
	
	lisa_https_test();
	//  lisa_os_test();
	// lisa_websocket_test();

	while (1) {
		k_sleep(K_MSEC(5000));
		printk("lisa porting test on !\n");
	}
}
static int PSRAM_init(const struct device *unused)
{
	extern void PSRAM_Initialize(void);
	ARG_UNUSED(unused);
	PSRAM_Initialize();
	return 0;
}

SYS_INIT(PSRAM_init, PRE_KERNEL_2,0);
