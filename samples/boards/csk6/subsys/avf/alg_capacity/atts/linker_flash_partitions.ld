/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Linker command/script file
 *
 * Linker script for the Cortex-M platforms.
 */

#include <zephyr/linker/sections.h>
#include <zephyr/devicetree.h>

#include <zephyr/linker/linker-defs.h>
#include <zephyr/linker/linker-tool.h>

/** enable CONFIG_SRAM2 or any other partition in soc Kconfig,
 * this is just an example to show relocation of code/data/bss script
 */

MEMORY
{
    FLASH_PARTITIONS1                  (r) : ORIGIN = (CONFIG_FLASH_BASE_ADDRESS + CONFIG_FLASH_CP_IAMGE_ADDRESS), LENGTH = CONFIG_FLASH_CP_IAMGE_SIZE
    FLASH_PARTITIONS2                  (r) : ORIGIN = (CONFIG_FLASH_BASE_ADDRESS + CONFIG_FLASH_TTS_DATA_ADDRESS), LENGTH = CONFIG_FLASH_TTS_DATA_SIZE
}

#include <zephyr/arch/arm/aarch32/cortex_m/scripts/linker.ld>
