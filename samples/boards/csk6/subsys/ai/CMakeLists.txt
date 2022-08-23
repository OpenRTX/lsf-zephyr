cmake_minimum_required(VERSION 3.20.0)
# Copyright (c) 2018 Nordic Semiconductor ASA
# Copyright (c) 2019 Linaro Limited
#
# SPDX-License-Identifier: Apache-2.0
#
#set(ZEPHYR_EXTRA_MODULES $ENV{ZEPHYR_BASE}/../modules/lib/sof_host)

if("${BOARD}" STREQUAL "csk6_tester" OR "${BOARD}" STREQUAL "csk6002_c3_nano"  OR "${BOARD}" STREQUAL "csk6002_9s_nano")
  set(BUILD_DSP_FIRMWARE ON)
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

  message(INFO "ZEPHYR_BINARY_DIR = ${ZEPHYR_BINARY_DIR} ")

  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)

  message(INFO " ${BOARD} compile as Master in this sample")
  target_sources(app PRIVATE src/main.c)
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

  project(sof_remote)

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
    BINARY_DIR "${REMOTE_ZEPHYR_DIR}"
    # NB: Do we need to pass on more CMake variables?
    BUILD_ALWAYS True
  )

  unset(ENV{XCC_USE_CLANG})
  unset(ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ./src)
  
  # if (EXISTS "${REMOTE_ZEPHYR_DIR}/zephyr/zephyr_csk6_dsp.bin")
    message(INFO "Found ${CMAKE_CURRENT_BINARY_DIR}")
    generate_inc_file_for_target(
      app
      ${CMAKE_CURRENT_BINARY_DIR}/sof_remote-prefix/src/sof_remote-build/zephyr/zephyr_csk6_dsp.bin
      src/zephyr_csk_dsp.inc
    )
  # else()
  #   message(WARNING "NOT Found ${REMOTE_ZEPHYR_DIR}/zephyr/zephyr_csk6_dsp.bin")
  # endif()

endif()



# add_custom_target(second_core_inc_target DEPENDS ${gen_dir}/zephyr_csk_dsp.inc)

# generate_inc_file_for_target(
#   second_core_inc_target
#   ${CMAKE_CURRENT_BINARY_DIR}/sof_remote-prefix/src/sof_remote-build/zephyr/zephyr_csk6_dsp.bin
#   ${gen_dir}/zephyr_csk_dsp.inc
#   sof_remote
# )

# add_dependencies(second_core_inc_target sof_remote)
# # add_dependencies(test app)

