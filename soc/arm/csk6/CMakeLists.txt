# SPDX-License-Identifier: Apache-2.0

zephyr_compile_options(-DARMCM33_DSP_FP -DPLATFORM_AP -DCSK6 -DIC_BOARD)
zephyr_sources(
  firmware_verification_table.S
  soc.c
  )

zephyr_sources_ifdef(CONFIG_CPU_HAS_CUSTOM_FIXED_SOC_MPU_REGIONS mpu_regions.c)
zephyr_sources_ifdef(CONFIG_CACHE_MANAGEMENT cache.c)
zephyr_linker_sources(SECTIONS csk6_private.ld)
zephyr_linker_sources(SECTIONS csk6_psram.ld)
