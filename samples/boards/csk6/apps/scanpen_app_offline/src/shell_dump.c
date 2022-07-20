/*
 * 
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>

#include <zephyr.h>
#include <sys/printk.h>
#include <shell/shell.h>
#include <stdlib.h>

#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <device.h>

#define DUMP_USE_UART   (0)
#define DUMP_USE_CDC    (0)
#define DUMP_USE_FLASH  (1)  


#if DUMP_USE_FLASH
    #if DT_NODE_HAS_STATUS(DT_INST(0, listenai_csk_flash_controller), okay)
    #define FLASH_DEVICE DT_LABEL(DT_INST(0, listenai_csk_flash_controller))
    #define FLASH_NAME "LISTENAI INN-FLASH"
    #define FLASH_WRITE_ADDR (0x1a00000)
    #define FLASH_ERASE_SIZE (0x400000)
    #else
    #error Unsupported flash driver
    #endif
#endif

static int cmd_dump_start(const struct shell *shell, int argc, char **argv)
{
    uint32_t addr = 0;
    int len = 0;

    sscanf(argv[1], "0x%x", &addr);
    sscanf(argv[2], "%d", &len);

    printk("[addr]: %x, [len]: %x\n", addr, len);

#if DUMP_USE_UART
    for(int i = 0; i < len; i++){
        printk("%02x ", *((uint8_t *)addr + i));
        // printk("%02x ", i % 32);

        if((i+1) % 32 == 0){
            printk("\n");
        }

        if((i+1) % 32 == 0){
            k_msleep(1);
        }
    }
    printk("\n");

#elif DUMP_USE_FLASH
	const struct device *flash_dev;
    

	/* flash dev */
	flash_dev = device_get_binding(FLASH_DEVICE);
	if (!flash_dev) {
		printk("Litenai CSK6 flash driver was not found!\n");
		return 0;
	}else{
        printk("flash found!\n");
    }

	/* flash erase */
    #define FLASH_ERASE_STEP_SIZE (256 * 1024)
    for(int i = 1; i != FLASH_ERASE_SIZE / FLASH_ERASE_STEP_SIZE; i++){
        if (flash_erase(flash_dev, FLASH_WRITE_ADDR+i*FLASH_ERASE_STEP_SIZE, FLASH_ERASE_STEP_SIZE) != 0) {
            printk("Flash erase %d failed!\n", i);
        }else{
            printk("Flash erase done!\n");
        }
    }


	/* flash write */
	if (flash_write(flash_dev, FLASH_WRITE_ADDR, addr, len) != 0) {
		printk("Flash write 1 failed!\n");
		return 0;
	}else{
        printk("Flash write done!\n");
    }

    printk("FLASH WRITE DONE ! start: %x, len: %x\n", (FLASH_WRITE_ADDR + 0x18000000), len);
    return 0;

#endif
}



SHELL_STATIC_SUBCMD_SET_CREATE(sub_dump,
	/* Alphabetically sorted to ensure correct Tab autocompletion. */
	SHELL_CMD_ARG(start,    NULL,	NULL,	cmd_dump_start,  3, 0),


	SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_CMD_REGISTER(dump, &sub_dump, "dump related commands.", NULL);
