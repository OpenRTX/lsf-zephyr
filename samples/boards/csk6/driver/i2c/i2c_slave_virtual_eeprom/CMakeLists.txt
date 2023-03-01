# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2c_slave_virtual_eeprom)

foreach(item ${ZEPHYR_EXTRA_MODULES})
    message(STATUS ${item})
endforeach()

FILE(GLOB app_sources src/*.c)

target_sources(app PRIVATE ${app_sources})
