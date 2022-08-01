# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(audio)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})


set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)
generate_inc_file_for_target(
  app
  resource/zephyr.bin
  ${gen_dir}/dsp_firmware.inc
)

generate_inc_file_for_target(
  app
  resource/csk6_default.tplg
  ${gen_dir}/dsp_tplg.inc
)

