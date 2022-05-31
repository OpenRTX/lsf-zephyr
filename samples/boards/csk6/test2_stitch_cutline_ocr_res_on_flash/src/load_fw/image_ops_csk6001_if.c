/*
 * Memory based image store Operation
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <sys/__assert.h>
#include <in_common.h>
#include <errno.h>
#include <cache.h>

int mem_image_open(void *store, const char *path, const void **image_data)
{
	struct mem_file *image = store;
	const void *fw_base = image->base;

	(void)(path);
	if (image_data == NULL) {
		LPERROR("%s: input image_data is NULL\r\n", __func__);
		return -EINVAL;
	}
	*image_data = fw_base;
	/* return an abitrary length, as the whole firmware is in memory */
	return 0x200000;
}

void mem_image_close(void *store)
{
	/* The image is in memory, does nothing */
	(void)store;
}

int mem_image_load(void *store, size_t offset, size_t size, const void **data, metal_phys_addr_t pa,
		   struct metal_io_region *io, char is_blocking)
{
	struct mem_file *image = store;
	const void *fw_base = image->base;
	uint32_t ilen;
	uint32_t *p_vatmp;
	uint32_t *p_fwtmp;

	(void)is_blocking;

	LPRINTF("%s: offset=0x%x, size=0x%x\n\r", __func__, offset, size);
	if (pa == METAL_BAD_PHYS) {
		if (data == NULL) {
			LPERROR("%s: data is NULL while pa is ANY\r\n", __func__);
			return -EINVAL;
		}
		*data = (const void *)((const char *)fw_base + offset);
	} else {
		void *va;
		if (io == NULL) {
			LPERROR("%s, io is NULL while pa is not ANY\r\n", __func__);
			return -EINVAL;
		}
		va = metal_io_phys_to_virt(io, pa);
		if (va == NULL) {
			LPERROR("%s: no va is found\r\n", __func__);
			return -EINVAL;
		}
		//load second core iram space ,must AGIN(4)
		if ((uint32_t)va < REMOTE_CORE_BOOT_ADDRESS) {
			p_vatmp = va;
			p_fwtmp = (uint32_t *)((uint32_t)fw_base + offset);
			for (ilen = 0; ilen < size; ilen += 4) {
				*p_vatmp++ = *p_fwtmp++;
			}
		} else {
			va = (char *)((uint32_t)va - PSRAM_LOGIC_ADDRESS_DIFF);
			memcpy(va, (const void *)((const char *)fw_base + offset), size);
		}
		sys_cache_data_range(va, size, K_CACHE_WB);
	}
	return (int)size;
}

struct image_store_ops mem_image_store_ops = {
	.open = mem_image_open,
	.close = mem_image_close,
	.load = mem_image_load,
	.features = SUPPORT_SEEK,
};
