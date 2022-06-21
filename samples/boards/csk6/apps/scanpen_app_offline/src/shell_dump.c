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

static int cmd_dump_start(const struct shell *shell, int argc, char **argv)
{
    int addr = 0;
    int len = 0;
    sscanf(argv[1], "0x%x", &addr);
    sscanf(argv[2], "%d", &len);

    printk("[addr]: %x, [len]: %x\n", addr, len);
    
    for(int i = 0; i < len; i++){
        printk("%02x ", *((uint8_t *)addr + i));

        if((i+1) % 32 == 0){
            printk("\n");
        }
    }
    printk("\n");
}



SHELL_STATIC_SUBCMD_SET_CREATE(sub_dump,
	/* Alphabetically sorted to ensure correct Tab autocompletion. */
	SHELL_CMD_ARG(start,    NULL,	NULL,	cmd_dump_start,  3, 0),


	SHELL_SUBCMD_SET_END /* Array terminated. */
);


SHELL_CMD_REGISTER(dump, &sub_dump, "dump related commands.", NULL);
