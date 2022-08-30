cmake_minimum_required(VERSION 3.20.0)
# Copyright (c) 2018 Nordic Semiconductor ASA
# Copyright (c) 2019 Linaro Limited
#
# SPDX-License-Identifier: Apache-2.0
#
#set(ZEPHYR_EXTRA_MODULES $ENV{ZEPHYR_BASE}/../modules/lib/sof_host)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(algo_ap)
enable_language(C ASM)

if("${BOARD}" STREQUAL "csk6_tester" OR "${BOARD}" STREQUAL "csk6002_c3_nano"  OR "${BOARD}" STREQUAL "csk6002_9s_nano")
  set(BUILD_DSP_FIRMWARE ON)

  message(INFO " ${BOARD} compile as Master in this sample")
  add_subdirectory(./src)
  zephyr_compile_options(-g3 -gdwarf-2)

endif()


if(BUILD_DSP_FIRMWARE)

  set(REMOTE_ZEPHYR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
  message(INFO "REMOTE_ZEPHYR_DIR = ${REMOTE_ZEPHYR_DIR} ")

  if("${BOARD}" STREQUAL "csk6_tester" OR "${BOARD}" STREQUAL "csk6002_c3_nano"  OR "${BOARD}" STREQUAL "csk6002_9s_nano")
    set(BOARD_REMOTE "ls_adsp_csk6")
  else()
    message(FATAL_ERROR "${BOARD} was not supported for this sample")
  endif()



  include(ExternalProject)

  set(ENV{XCC_USE_CLANG} "1")
  set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc")

  ExternalProject_Add(
    sof_remote
    SOURCE_DIR ${APPLICATION_SOURCE_DIR}/remote
    INSTALL_COMMAND ""      # This particular build system has no install command
    CMAKE_CACHE_ARGS -DBOARD:STRING=${BOARD_REMOTE}
    BINARY_DIR "${REMOTE_ZEPHYR_DIR}"
    # NB: Do we need to pass on more CMake variables?
    BUILD_ALWAYS True
  )

  unset(ENV{XCC_USE_CLANG})
  unset(ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ./src)
    
  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)

  add_custom_target(second_core_inc_target DEPENDS ${gen_dir}/zephyr_csk_dsp.inc)

  generate_inc_file_for_gen_target(${ZEPHYR_CURRENT_LIBRARY}
            ${REMOTE_ZEPHYR_DIR}/zephyr/zephyr_csk6_dsp.bin
            ${gen_dir}/zephyr_csk_dsp.inc
            second_core_inc_target)
  
  add_dependencies(second_core_inc_target sof_remote)

  add_dependencies(app sof_remote)

endif()





