/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/printk.h>
#include <sys/sys_heap.h>
#include "cmd_etf.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk_wifi_819s_etf, LOG_LEVEL_DBG);

void main(void)
{    LOG_DBG("Hello csk_wifi_819s_etf");

    cmd_etf_init();
}
