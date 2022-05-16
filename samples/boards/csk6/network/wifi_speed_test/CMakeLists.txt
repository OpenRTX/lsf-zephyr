# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)

# zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/sched.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/kheap.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/sem.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/mutex.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/kernel/mem.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/../modules/hal/listenai/csk6/drivers/cm33/sdmmc/ftsdc021.c SRAM_TEXT)
# zephyr_code_relocate($ENV{ZEPHYR_BASE}/../modules/hal/listenai/csk6/drivers/cm33/sdmmc/ftsdc021_sdio.c SRAM_TEXT)

