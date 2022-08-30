# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(CONF_FILE
  prj.conf
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.conf")
  list(APPEND CONF_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.conf
  )
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resource/res.conf")
  list(APPEND CONF_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/res.conf
  )
endif()

set(DTC_OVERLAY_FILE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.overlay")
  list(APPEND DTC_OVERLAY_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD}.overlay
  )
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resource/res.overlay")
  list(APPEND DTC_OVERLAY_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/res.overlay
  )
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sample_head_shoulder_detect)
target_sources(app PRIVATE
  src/main.c
)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)
generate_inc_file_for_target(
  app
  resource/zephyr.bin
  ${gen_dir}/dsp_firmware.inc
)

generate_inc_file_for_target(
  app
  resource/sof-imx8mp-compr-mp3-wm8960.tplg
  ${gen_dir}/dsp_tplg.inc
)

generate_inc_file_for_target(
  app
  resource/head_shoulder_detect.bin
  ${gen_dir}/head_shoulder_res.inc
)
