#include "sys/util.h"
#include <stdlib.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/sys_heap.h>
#include <kernel.h>
#include "cmd_etf.h"
#include "wifi_819s_etf.h"
#include <logging/log.h>


int cmd_etf_cli_exec(const char **argv, const size_t argc)
{
    return etf_cli_main(argv);
}


int cmd_etf_init()
{

    etf_init();

    return 0;
}
