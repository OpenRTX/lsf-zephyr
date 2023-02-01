# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(SUPPORT_BOARDS csk6012_nano csk6011a_nano)
if(NOT ${BOARD} IN_LIST SUPPORT_BOARDS)
    message("Please choose one of the following boards:")
    foreach(item ${SUPPORT_BOARDS})
        message(STATUS ${item})
    endforeach()
    message(FATAL_ERROR "\nThe project does not support the ${BOARD}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(code_relocation)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# Code relocation feature
zephyr_code_relocate(src/test_file1.c PSRAMAP)
zephyr_code_relocate(src/test_file2.c SRAM)
zephyr_code_relocate(src/test_file3.c PSRAMAP_TEXT)
zephyr_code_relocate(src/test_file3.c SRAM_DATA)
zephyr_code_relocate(src/test_file3.c PSRAMAP_BSS)

zephyr_code_relocate(${ZEPHYR_BASE}/kernel/sem.c SRAM)

