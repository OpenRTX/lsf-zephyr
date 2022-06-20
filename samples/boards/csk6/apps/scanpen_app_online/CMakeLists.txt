cmake_minimum_required(VERSION 3.20.0)
# Copyright (c) 2018 Nordic Semiconductor ASA
# Copyright (c) 2019 Linaro Limited
#
# SPDX-License-Identifier: Apache-2.0
#
#set(ZEPHYR_EXTRA_MODULES $ENV{ZEPHYR_BASE}/../modules/lib/sof_host)

if("${BOARD}" STREQUAL "csk6_tester" OR "${BOARD}" STREQUAL "csk6002_c3_nano"  OR "${BOARD}" STREQUAL "csk6002_9s_nano" OR "${BOARD}" STREQUAL "csk6012_9s_scanpen")
  set(BUILD_DSP_FIRMWARE OFF)
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
  message(INFO " ${BOARD} compile as Master in this sample")
  target_sources(app PRIVATE src/main.c)
  add_subdirectory(./src)
  zephyr_compile_options(-g3 -gdwarf-2)
  add_subdirectory(lisa_hal)
  add_subdirectory(scanpen_lisa/lisa_os)
  add_subdirectory(scanpen_lisa/lisa_net)
  add_subdirectory(scanpen_lisa/unit_test)
  add_subdirectory(scanpen_lisa/scanpen_sdk)
  # add_subdirectory(scanpen_lisa/scanpen_app)
  # zephyr_compile_options(-fno-short-enums)
  #zephyr_ld_options(-fno-short-enums)

  zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/sched.c SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/timer.c SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/thread.c SRAM_TEXT)

  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/prep_c.c SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cpu_idle.S SRAM_TEXT)

  zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/swap.c SRAM_TEXT)
  zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/swap_helper.S SRAM_TEXT)

  zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/vector_table.S SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/thread.c SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/timing.c SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/reset.S SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/thread.S SRAM_TEXT)

  zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/irq_manage.c SRAM_TEXT)
  zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/irq_relay.S SRAM_TEXT)
  zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/isr_wrapper.S SRAM_TEXT)
  zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/exc_exit.S SRAM_TEXT)

  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/soc/arm/csk6/cache.c SRAM_TEXT)
  # zephyr_code_relocate($ENV{ZEPHYR_BASE}/soc/arm/csk6/mpu_regions.c SRAM_TEXT)
  zephyr_code_relocate($ENV{ZEPHYR_BASE}/include/arch/arm/aarch32/asm_inline_gcc.h SRAM_TEXT)
  zephyr_code_relocate($ENV{ZEPHYR_BASE}/include/arch/arm/aarch32/irq.h SRAM_TEXT)

  zephyr_code_relocate($ENV{ZEPHYR_BASE}/drivers/dma/dma_csk6.c SRAM_TEXT)
  zephyr_code_relocate($ENV{ZEPHYR_BASE}/../modules/hal/listenai/csk6/drivers/cm33/dma.c SRAM_TEXT)

  if((CONFIG_MBEDTLS_ENABLE_HEAP) AND (CONFIG_CODE_DATA_RELOCATION))
    zephyr_code_relocate( ${ZEPHYR_BASE}/modules/mbedtls/zephyr_init.c PSRAMAP_BSS)
  endif()

  zephyr_code_relocate(${ZEPHYR_BASE}/driver/vedio/bf30a2.c RAM_TEXT)
  zephyr_compile_options(-g3 -gdwarf-2)
endif()


if(BUILD_DSP_FIRMWARE)

  set(REMOTE_ZEPHYR_DIR ${CMAKE_CURRENT_BINARY_DIR}/sof-dsp-prefix/src/sof-dsp-build/zephyr)

  if("${BOARD}" STREQUAL "csk6_tester" OR "${BOARD}" STREQUAL "csk6002_c3_nano"  OR "${BOARD}" STREQUAL "csk6002_9s_nano" OR "${BOARD}" STREQUAL "csk6012_9s_scanpen")
    set(BOARD_REMOTE "ls_adsp_csk6")
  else()
    message(FATAL_ERROR "${BOARD} was not supported for this sample")
  endif()

  project(sof-dsp)

  enable_language(C ASM)

  #target_sources(app PRIVATE src/load_fw.c src/mem_image_store.c src/platform_info.c src/zynqmp_csk6_rproc_example.c )

  include(ExternalProject)

  set(ENV{XCC_USE_CLANG} "1")
  set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc")

  ExternalProject_Add(
    sof_remote
    SOURCE_DIR ${APPLICATION_SOURCE_DIR}/remote
    INSTALL_COMMAND ""      # This particular build system has no install command
    CMAKE_CACHE_ARGS -DBOARD:STRING=${BOARD_REMOTE}
    BUILD_BYPRODUCTS "${REMOTE_ZEPHYR_DIR}/${KERNEL_BIN_NAME}"
    # NB: Do we need to pass on more CMake variables?
    BUILD_ALWAYS True
  )

  unset(ENV{XCC_USE_CLANG})
  unset(ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ./src)

endif()

zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/sched.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/timer.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/thread.c SRAM_TEXT)

# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/prep_c.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cpu_idle.S SRAM_TEXT)

zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/swap.c SRAM_TEXT)
zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/swap_helper.S SRAM_TEXT) #+20

# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/vector_table.S SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/thread.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/timing.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/reset.S SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/thread.S SRAM_TEXT)

zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/irq_manage.c SRAM_TEXT) # +40
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/irq_relay.S SRAM_TEXT) -5
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/isr_wrapper.S SRAM_TEXT) - 5
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/arch/arm/core/aarch32/cortex_m/exc_exit.S SRAM_TEXT) -8

# zephyr_code_relocate($ENV{ZEPHYR_BASE}/soc/arm/csk6/cache.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/soc/arm/csk6/mpu_regions.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/include/arch/arm/aarch32/asm_inline_gcc.h SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/include/arch/arm/aarch32/irq.h SRAM_TEXT)

zephyr_code_relocate($ENV{ZEPHYR_BASE}/drivers/dma/dma_csk6.c SRAM_TEXT)
zephyr_code_relocate($ENV{ZEPHYR_BASE}/../modules/hal/listenai/csk6/drivers/cm33/dma.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/../modules/hal/listenai/csk6/drivers/cm33/cache_ap.c SRAM_TEXT)
