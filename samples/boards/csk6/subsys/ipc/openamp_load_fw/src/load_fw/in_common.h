/*
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __IN__COMMON_H_
#define __IN__COMMON_H_

#include <metal/alloc.h>
#include <metal/io.h>
#include <metal/sys.h>
#include <metal/utilities.h>
#include <openamp/remoteproc.h>
#include <openamp/remoteproc_loader.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#define REMOTE_CORE_FW_NAME CONFIG_SECOND_CORE_IMAGE_NAME
#define REMOTE_CORE_BOOT_ADDRESS CONFIG_SECOND_CORE_BOOT_ADDRESS
#define REMOTE_CORE_IMAGE_ELF_FLASH_ADDRESS CONFIG_SECOND_CORE_IMAGE_ELF_ADDR
#define REMOTE_CORE_IMAGE_BINARY_FLASH_ADDRESS CONFIG_SECOND_CORE_IMAGE_BINARY_ADDR
#define REMOTE_CORE_IMAGE_BINARY_SIZE CONFIG_SECOND_CORE_IMAGE_BINARY_SIZE
#define PSRAM_LOGIC_ADDRESS_DIFF (0x30000000)
#define REMOTE_VECBASE_OFFSET (0x100400)
#define PSRAM_FW_WR_ADDRESS (REMOTE_CORE_BOOT_ADDRESS - PSRAM_LOGIC_ADDRESS_DIFF)

#define LOAD_FW_TARGET 2

#define LPRINTF(format, ...) printk(format, ##__VA_ARGS__)
#define LPERROR(format, ...) printk(format, ##__VA_ARGS__)

struct mem_file {
	const void *base;
};

struct rproc_priv {
	struct remoteproc *rproc;
	int cpu_id;
};

#endif /* COMMON_H_ */
