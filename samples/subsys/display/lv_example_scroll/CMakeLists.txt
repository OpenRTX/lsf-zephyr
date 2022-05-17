# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lvgl)

include($ENV{MICROPY_SDK}/ports/zephyr/lib_micropython.cmake)



# FILE(GLOB benchmark_sources src/demos/benchmark/assets/*.c src/demos/benchmark/*.c)

# message(STATUS "benchmark_sources = ${benchmark_sources}")

target_sources(app PRIVATE
    # ${benchmark_sources}
    # src/animation_demo.c
    # src/animation_demo1.c
    # "/home/abc/shares/csk6/sdk/lisa-micropython/sdk/lib/lv_bindings/"
    ${MPY_APP_SRC}
    src/main.c
)


# target_sources(app PRIVATE ${app_sources})

target_include_directories(app PRIVATE 
    ${MPY_APP_INCLUDE} 
    "/home/abc/shares/csk6/sdk/lisa-micropython/sdk/lib/lv_bindings"
)

target_include_directories(app PRIVATE src/)

target_link_libraries(app PRIVATE ${MICROPY_TARGET})
