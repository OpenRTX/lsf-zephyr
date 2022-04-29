/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>


void main(void)
{
	uint32_t ui32cnt = 0;
	while(1){

		printk("[Remote][%s %d]cnt=%d\n",__FUNCTION__,__LINE__,ui32cnt++);
		k_msleep(100);	
		
	}

}
