/* cache.c - d-cache support for CSK6  */

/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief d-cache manipulation
 *
 * This module contains functions for manipulation of the d-cache.
 */

#include <kernel.h>
#include <kernel_internal.h>
#include "cache.h"

#if defined(CONFIG_DCACHE_LINE_SIZE_DETECT)
size_t sys_cache_line_size;
#endif

void cache_data_enable(void)
{
	SCB_EnableDCache();
}

void cache_data_disable(void)
{
	SCB_DisableDCache();
}

void cache_instr_enable(void)
{
	SCB_EnableICache();
}

/* Disable i-cache */
void cache_instr_disable(void)
{
	
	SCB_DisableICache();
}

/* Write-back / Invalidate / Write-back + Invalidate all d-cache */
int cache_data_all(int op)
{
	/*TODO*/
	__ASSERT(0, "Unsupport the function!");
	return 0;
}

static void cache_data_flush(void *start_addr_ptr, size_t size)
{
	SCB_CleanInvalidateDCache_by_Addr(start_addr_ptr,size);
}

static void cache_data_invd(void *start_addr_ptr, size_t size)
{
	SCB_InvalidateDCache_by_Addr(start_addr_ptr,size);
}

int cache_data_range(void *addr, size_t size, int op)
{
	if (op == K_CACHE_INVD) {
		/*
		 * TODO: On invalidate we can contextually flush by setting the
		 * DC_CTRL_INVALID_FLUSH bit
		 */
		cache_data_invd(addr, size);
	} else if (op == K_CACHE_WB) {
		cache_data_flush(addr, size);
	} else {
		return -ENOTSUP;
	}

	return 0;
}

/* Write-back / Invalidate / Write-back + Invalidate all i-cache */
int cache_instr_all(int op)
{
	/*TODO*/
	__ASSERT(0, "unsupported the function!");
	return 0;
}

/* Write-back / Invalidate / Write-back + Invalidate i-cache lines */
int cache_instr_range(void *addr, size_t size, int op)
{
	/*TODO*/
	__ASSERT(0, "unsupported the function!");
	return 0;
}

#if defined(CONFIG_DCACHE_LINE_SIZE_DETECT)
static void init_dcache_line_size(void)
{
	/*TODO*/
	__ASSERT(0, "unsupported the function!");
}

size_t cache_data_line_size_get(void)
{
	/*TODO*/
	__ASSERT(0, "unsupported the function!");
}
#endif
