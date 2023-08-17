/*
 * Copyright (c) 2021 ListenAi Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm/aarch32/mpu/arm_mpu.h>

#define ROM_CODE_BASE_ADDRESS 0x00000000UL
#define ROM_CODE_SIZE 1024

#define REGION_RAM_NOCACHE_ATTR(base, size) \
	{\
		.rbar = NOT_EXEC | \
			P_RW_U_NA_Msk | NON_SHAREABLE_Msk, /* AP, XN, SH */ \
		/* Cache-ability */ \
		.mair_idx = MPU_MAIR_INDEX_SRAM_NOCACHE, \
		.r_limit = REGION_LIMIT_ADDR(base, size),  /* Region Limit */ \
	}

#define REGION_ROM_CODE_ATTR(base, size) \
	{\
		.rbar = NOT_EXEC | \
			P_RO_U_NA_Msk, /* AP, XN, SH */ \
		/* Cache-ability */ \
		.mair_idx = MPU_MAIR_INDEX_SRAM, \
		.r_limit = REGION_LIMIT_ADDR(base, size),  /* Region Limit */ \
	}

static const struct arm_mpu_region mpu_regions[] = {
	/* Region 0 */
	MPU_REGION_ENTRY("FLASH_0",
			 	CONFIG_FLASH_BASE_ADDRESS,
			 	REGION_FLASH_ATTR(CONFIG_FLASH_BASE_ADDRESS,CONFIG_FLASH_SIZE * 1024)),

	/* Region 1 */
	MPU_REGION_ENTRY("SRAM_0",
			 	CONFIG_SRAM_BASE_ADDRESS,
			 	REGION_RAM_ATTR(CONFIG_SRAM_BASE_ADDRESS,CONFIG_SRAM_SIZE* 1024)),

	/* Region 2 */
	MPU_REGION_ENTRY("ROMCODE",
			 	ROM_CODE_BASE_ADDRESS,
			 	REGION_ROM_CODE_ATTR(ROM_CODE_BASE_ADDRESS,ROM_CODE_SIZE)),

#if defined(CONFIG_OPENAMP_IPC_SHM_BASE_ADDRESS)
	/* Region 2 */
	MPU_REGION_ENTRY("OPENAMP_SHARE_SRAM",
				CONFIG_OPENAMP_IPC_SHM_BASE_ADDRESS,
		 		REGION_RAM_NOCACHE_ATTR( CONFIG_OPENAMP_IPC_SHM_BASE_ADDRESS,CONFIG_OPENAMP_IPC_SHM_SIZE )),
#endif


#if defined(CONFIG_AVF_IPC_SHM_BASE_ADDRESS)
	/* Region 2 */
	MPU_REGION_ENTRY("AVF_SHARE_SRAM",
				CONFIG_AVF_IPC_SHM_BASE_ADDRESS,
		 		REGION_RAM_NOCACHE_ATTR( CONFIG_AVF_IPC_SHM_BASE_ADDRESS,CONFIG_AVF_IPC_SHM_SIZE )),
#endif
};

const struct arm_mpu_config mpu_config = {
	.num_regions = ARRAY_SIZE(mpu_regions),
	.mpu_regions = mpu_regions,
};
