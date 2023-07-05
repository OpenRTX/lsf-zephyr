# SPDX-License-Identifier: Apache-2.0

zephyr_sources(
  firmware_verification_table.S
  soc.c
  )

zephyr_sources_ifdef(CONFIG_CPU_HAS_CUSTOM_FIXED_SOC_MPU_REGIONS mpu_regions.c)
zephyr_sources_ifdef(CONFIG_CACHE_MANAGEMENT cache.c)
zephyr_sources_ifdef(CONFIG_EXTRA_EXCEPTION_INFO fatal.c)
zephyr_linker_sources(SECTIONS csk6_private.ld)
zephyr_linker_sources(SECTIONS csk6_psram.ld)

if(NOT CONFIG_BOOTLOADER_MCUBOOT)
  if(CONFIG_BUILD_OUTPUT_HEX)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands COMMAND
      ${PYTHON_EXECUTABLE}
      ${SOC_DIR}/arm/csk6/patch_hex.py
      ${KERNEL_HEX_NAME}
    )
  endif()

  if(CONFIG_BUILD_OUTPUT_BIN)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands COMMAND
      ${PYTHON_EXECUTABLE}
      ${SOC_DIR}/arm/csk6/patch_bin.py
      ${KERNEL_BIN_NAME}
    )
  endif()

  if(CONFIG_BUILD_OUTPUT_BIN AND CONFIG_BUILD_OUTPUT_UF2)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands COMMAND
      ${PYTHON_EXECUTABLE}
      ${SOC_DIR}/arm/csk6/patch_uf2.py
      ${KERNEL_UF2_NAME}
      ${KERNEL_BIN_NAME}
    )
  endif()
endif()
