cmake_minimum_required(VERSION 3.20.0)
# Copyright (c) 2018 Nordic Semiconductor ASA
# Copyright (c) 2019 Linaro Limited
#
# SPDX-License-Identifier: Apache-2.0
#
#set(ZEPHYR_EXTRA_MODULES $ENV{ZEPHYR_BASE}/../modules/lib/sof_host)

if("${BOARD}" STREQUAL "csk6002_c3_nano")
  set(BUILD_DSP_FIRMWARE OFF)
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
  message(INFO " ${BOARD} compile as Master in this sample")
  target_sources(app PRIVATE src/main.c)
  add_subdirectory(./src)
  zephyr_compile_options(-fno-short-enums)
  zephyr_ld_options(-fno-short-enums)
endif()


if(BUILD_DSP_FIRMWARE)

  set(REMOTE_ZEPHYR_DIR ${CMAKE_CURRENT_BINARY_DIR}/sof-dsp-prefix/src/sof-dsp-build/zephyr)

  if("${BOARD}" STREQUAL "csk6002_c3_nano")
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
