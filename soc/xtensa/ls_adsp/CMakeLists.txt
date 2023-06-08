# LS CSK6 SoC family CMake file
#
# Copyright (c) 2021 ListenAI
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(common)

if($ENV{XCC_USE_CLANG})
    set_property(GLOBAL PROPERTY CSTD c11)
    zephyr_compile_options(--stdlib=libc++ -std=c11 -Wno-unused-command-line-argument -ffunction-sections -fdata-sections -mno-coproc -mlongcalls -mno-l32r-flix -DASYM)
else()
    zephyr_ld_options(-static -Wl,-gc-sections)
    zephyr_compile_options(-ffunction-sections -fdata-sections -mno-coproc -mlongcalls -mno-l32r-flix -Wall -Wno-format -Wno-unused -Wno-comment -MMD -Wa,--schedule)
endif()

add_definitions(-DXTENSA_TOOLCHAIN_PATH=$ENV{XTENSA_TOOLCHAIN_PATH})
add_definitions(-DZEPHYR_TOOLCHAIN_VARIANT=xcc})
