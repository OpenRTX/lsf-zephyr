cmake_minimum_required(VERSION 3.20.0)

# SPDX-License-Identifier: Apache-2.0
#

if("${BOARD}" STREQUAL "ls_adsp_csk6")
set(ENV{XCC_USE_CLANG} "1")
set(ENV{ZEPHYR_TOOLCHAIN_VARIANT} "xcc")
endif()

if("${BOARD}" STREQUAL "ls_adsp_csk6")
	message(INFO " ${BOARD} compiles as remote in this sample")
else()
	message(FATAL_ERROR "${BOARD} was not supported for this sample")
endif()
find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(sample_sof)

target_sources(app PRIVATE
    src/main.c
  )

zephyr_interface_library_named(sof_lib)

zephyr_library_include_directories(app PUBLIC
    ${sof_module}/src/arch/xtensa/include
    ${sof_module}/src/include
  )
