cmake_minimum_required(VERSION 3.20.0)
# Copyright (c) 2018 Nordic Semiconductor ASA
# Copyright (c) 2019 Linaro Limited
#
# SPDX-License-Identifier: Apache-2.0
#
#set(ZEPHYR_EXTRA_MODULES $ENV{ZEPHYR_BASE}/../modules/lib/sof_host)

if("${BOARD}" STREQUAL "csk6_tester" OR "${BOARD}" STREQUAL "csk6002_c3_nano"  OR "${BOARD}" STREQUAL "csk6002_9s_nano")

  set(BUILD_DSP_FIRMWARE OFF)
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(avf_aplay)
  message(INFO " ${BOARD} compile as Master in this sample")
  target_sources(app PRIVATE src/main.c)
  add_subdirectory(./src)
  zephyr_compile_options(-g3 -gdwarf-2)

  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)
  generate_inc_file_for_target(
    app
    resource/zephyr.bin
    ${gen_dir}/dsp_firmware.inc
  )

  generate_inc_file_for_target(
    app
    resource/csk6_default_pcm.tplg
    ${gen_dir}/dsp_tplg.inc
  )

  generate_inc_file_for_target(
    app
    resource/earthquake_48k_16bit.wav
    ${gen_dir}/test_audio.inc
  )

endif()



