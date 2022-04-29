#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lisa_porting)
zephyr_include_directories(tests/inc)
add_subdirectory_ifdef(CONFIG_HAS_CSK6 lisa_os)
add_subdirectory_ifdef(CONFIG_HAS_CSK6 lisa_net)
# add_subdirectory(tests)
FILE(GLOB app_sources tests/*.c) 
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} tests/inc) 
target_sources(app PRIVATE ${app_sources})

