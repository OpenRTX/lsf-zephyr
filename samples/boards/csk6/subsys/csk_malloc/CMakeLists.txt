# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(csk_malloc)

target_sources(app PRIVATE src/main.c)
zephyr_compile_options(-g3 -gdwarf-2)