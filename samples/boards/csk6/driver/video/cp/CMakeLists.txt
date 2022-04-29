# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if("${BOARD}" STREQUAL "ls_adsp_csk6")
set(ENV{XCC_USE_CLANG} "1")
set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)
