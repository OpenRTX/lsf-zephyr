/*
 * 
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sys/sys_heap.h"
#include <stdbool.h>
#if (CONFIG_SHELL)

#include <stdlib.h>
#include <string.h>

#include <zephyr.h>
#include <sys/printk.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <fs/fs.h>
#include <device.h>
#include <soc.h>
#include <stdlib.h>

#include "cmd_etf.h"
// #include "thread_analyzer.h"

LOG_MODULE_REGISTER(csk_wifi_819s_shell_etf, LOG_LEVEL_DBG);

#define SHELL_ETF_ROOT_CMD_MIN_ARGC 2

#if (CONFIG_SHELL_ARGC_MAX > 4)
#define ARGC_MAX (CONFIG_SHELL_ARGC_MAX - 4)
#else
#error Please increase CONFIG_SHELL_ARGC_MAX parameter.
#endif

extern void thread_analyzer_print(void);
extern struct heap_t _system_heap;


static int cmd_usage(const struct shell *shell, size_t argc, char **argv)
{
	thread_analyzer_print();
	sys_heap_print_info((struct sys_heap *)&_system_heap, false);
	return 0;
}

static int cmd_etf(const struct shell *shell, size_t argc, char **argv)
{
	LOG_DBG("cmd_etf, argc: %d", argc);
	


	// if (argc >= SHELL_ETF_ROOT_CMD_MIN_ARGC) {
	// 	for (int i = 1; i < argc; i++) {
	// 		LOG_INF("cmd_etf argv[%d]:%s", i, (char *)argv[i]);
	// 	}
	// }
	
	size_t argc_without_prefix_cmd = argc - 1;
	char **argv_without_prefix_cmd = argv + 1;
	return cmd_etf_cli_exec((const char **)(argv_without_prefix_cmd), argc_without_prefix_cmd);
}
// static int cmd_etf_connect(const struct shell *shell, size_t argc, char **argv)
// {

// 	cmd_etf_cli_exec((const char **)argv, argc);
// 	return 0;
// }

// static int cmd_etf_disconnect(const struct shell *shell, size_t argc, char **argv)
// {
// 	return 0;
// }


SHELL_CMD_REGISTER(etf, NULL, "etf related commands.", &cmd_etf);
SHELL_CMD_REGISTER(usage, NULL, "stack usage and heap usage.", &cmd_usage);
#endif
