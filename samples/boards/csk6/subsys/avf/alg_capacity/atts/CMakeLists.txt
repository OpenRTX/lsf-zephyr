# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sample_aplay)
target_sources(app PRIVATE src/main.c src/tts_playback.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)

generate_inc_file_for_target(
  app
  resource/csk6_default.tplg
  ${gen_dir}/dsp_tplg.inc
)

generate_inc_file_for_target(
  app
  resource/tts_data.bin
  ${gen_dir}/tts_data.inc
)

generate_inc_file_for_target(
  app
  resource/zephyr.bin
  ${gen_dir}/cp_firmware.inc
)

zephyr_linker_sources(SECTIONS custom-sections.ld)
