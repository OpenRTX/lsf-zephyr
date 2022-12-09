cmake_minimum_required(VERSION 3.20.0)

# SPDX-License-Identifier: Apache-2.0
#

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(ENV{XCC_USE_CLANG} "1")
set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc")


find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(sample_sof)

target_sources(app PRIVATE
    src/main.c
  )

zephyr_ld_options(-Wl,--wrap,calloc -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,printf)

zephyr_interface_library_named(sof_lib)

zephyr_library_include_directories(app PUBLIC
    ${sof_module}/src/arch/xtensa/include
    ${sof_module}/src/include
  )
