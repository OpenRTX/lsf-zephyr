# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

if(${CONFIG_SOC_LS_ADSP_CSK6})
zephyr_library_sources(src/i2s_wm8960_inout.c)
endif()

project(hello_world)
target_sources(app PRIVATE src/main.c)
