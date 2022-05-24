# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lvgl)

include($ENV{MICROPY_SDK}/ports/zephyr/lib_micropython.cmake)

target_sources(app PRIVATE
    ${MPY_APP_SRC}
    src/main.c
    src/scan_pen_font.c
)

target_include_directories(app PRIVATE
    ${MPY_APP_INCLUDE}
)

target_include_directories(app PRIVATE src/)

target_link_libraries(app PRIVATE ${MICROPY_TARGET})
