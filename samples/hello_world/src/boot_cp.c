#include <zephyr/zephyr.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include "venus_ap.h"
LOG_MODULE_REGISTER(boot_cp, CONFIG_LOG_DEFAULT_LEVEL);

#define CP_FLASH_BASE_ADDRESS (0x68000000)
#define FLASH_CP_IMAGE_REGION_OFFSET (0x90000)
#define CP_BOOT_ADDRESS   (CP_FLASH_BASE_ADDRESS + FLASH_CP_IMAGE_REGION_OFFSET)

#define CP_XIP (1)
extern void dcache_invalidate_range(unsigned long start, unsigned long end);
void boot_cp(void)
{

	// set boot address
	printk("CP XIP start addr(0x%x)\n", CP_BOOT_ADDRESS);
	IP_SYSCTRL->REG_CP_CTRL2.bit.CP_BOOT_ADDR = CP_BOOT_ADDRESS;

	IP_SYSCTRL->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 1;

	// stall HIFI4
	IP_SYSCTRL->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 1;

	// release CP & NPU
	IP_SYSCTRL->REG_SW_RESET_CP.all = 0xCAFE100B;
	if (IP_SYSCTRL->REG_SW_RESET_NPU.all)
		IP_SYSCTRL->REG_SW_RESET_NPU.all = 0xCAFE000C;
	__DMB();
	__DSB();

	// run cp
	IP_SYSCTRL->REG_SW_RESET_CP.all = 0xCAFE000B;
	IP_SYSCTRL->REG_SW_RESET_CP.all = 0xCAFE100B;
	IP_SYSCTRL->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

/*------------------------------private define------------------------------------*/
#define MEM_BANK_CNT 4
#define SHAREMEMORY_BANK0_REMAP 1
#define AP_SYS_RAM_BANK_SIZE 0x00010000
#define AP_SYS_RAM_BANK_ADDR (0x20080000 + AP_SYS_RAM_BANK_SIZE * MEM_BANK_CNT)
	IP_SYSCTRL->REG_AP_CTRL1.bit.AP_RAM_SEL = (SHAREMEMORY_BANK0_REMAP << MEM_BANK_CNT);
	memset((void *)AP_SYS_RAM_BANK_ADDR, 0x00, AP_SYS_RAM_BANK_SIZE);
	// clean & invalidate cache, 确保后续读取到的对象来自于对端写入的共享内存.
	dcache_invalidate_range(AP_SYS_RAM_BANK_ADDR, AP_SYS_RAM_BANK_ADDR + AP_SYS_RAM_BANK_SIZE);

	// #define MEM_BANK1_CNT 3
	// #define AP_SYS_RAM_BANK1_ADDR (0x20080000 + AP_SYS_RAM_BANK_SIZE * MEM_BANK1_CNT)
	// 	IP_SYSCTRL->REG_AP_CTRL1.bit.AP_RAM_SEL = (SHAREMEMORY_BANK0_REMAP << MEM_BANK1_CNT);
	// 	memset((void *)AP_SYS_RAM_BANK1_ADDR, 0x00, AP_SYS_RAM_BANK_SIZE);
	// 	// clean & invalidate cache, 确保后续读取到的对象来自于对端写入的共享内存.
	// 	dcache_invalidate_range(AP_SYS_RAM_BANK1_ADDR, AP_SYS_RAM_BANK1_ADDR + AP_SYS_RAM_BANK_SIZE);

	// enable usb clock
	IP_SYSCTRL->REG_MISC_CLK_CFG2.bit.ENA_USBC_CLK = 0x01;
}


