#include <init.h>
#include <zephyr/zephyr.h>
#include <zephyr/multi_heap/shared_multi_heap.h>

typedef struct region_map {
	struct shared_multi_heap_region region;
	uintptr_t p_addr;
}region_map_t;

region_map_t sys_region_map[] =
{
	{										
		.region = {								
			.addr = (uintptr_t) DT_REG_ADDR(DT_NODELABEL(psram_ap_nocache)),		
			.size = DT_REG_SIZE(DT_NODELABEL(psram_ap_nocache)),			
			.attr = DT_ENUM_IDX_OR(DT_NODELABEL(psram_ap_nocache), zephyr_memory_region_mpu, SMH_REG_ATTR_NUM),	
		},									\
	},
};

static inline enum smh_reg_attr mpu_to_reg_attr(int mpu_attr)
{
	/*
	 * All the memory regions defined in the DT with the MPU property `RAM`
	 * can be accessed and memory can be retrieved from using the attribute
	 * `SMH_REG_ATTR_CACHEABLE`.
	 *
	 * All the memory regions defined in the DT with the MPU property
	 * `RAM_NOCACHE` can be accessed and memory can be retrieved from using
	 * the attribute `SMH_REG_ATTR_NON_CACHEABLE`.
	 *
	 * [MPU attr]   -> [SMH attr]
	 *
	 * RAM          -> SMH_REG_ATTR_CACHEABLE
	 * RAM_NOCACHE  -> SMH_REG_ATTR_NON_CACHEABLE
	 */
	switch (mpu_attr) {
	case 0: /* RAM */
		return SMH_REG_ATTR_CACHEABLE;
	case 1: /* RAM_NOCACHE */
		return SMH_REG_ATTR_NON_CACHEABLE;
	default:
		return SMH_REG_ATTR_NUM;/* How ? */
	
	}

	/* whatever */
	return 0;
}
static void fill_multi_heap(void)
{
	struct region_map *reg_map;

	for (size_t idx = 0; idx < sizeof(sys_region_map)/sizeof(sys_region_map[0]); idx++) {
		reg_map = &sys_region_map[idx];

		/* Convert MPU attributes to shared-multi-heap capabilities */
		reg_map->region.attr = mpu_to_reg_attr(reg_map->region.attr);

		/* Assume for now that phys == virt */
		reg_map->p_addr = reg_map->region.addr;

		shared_multi_heap_add(&reg_map->region, NULL);
	}
}

int app_multi_heap_init(const struct device *dev){
  shared_multi_heap_pool_init();
  fill_multi_heap();
  return 0;
}

SYS_INIT(app_multi_heap_init, POST_KERNEL, 0);
