# Copyright (c) 2021 ListenAI
# SPDX-License-Identifier: Apache-2.0

zephyr_interface_library_named(LS_ADSP_COMMON)

zephyr_library_named(ls_adsp_common)
zephyr_library_include_directories(include)
zephyr_library_include_directories(${ZEPHYR_BASE}/drivers)


zephyr_library_sources(
  src/hal/int_asm.S
  src/hal/interrupts.c
  soc.c
  )


zephyr_library_link_libraries(LS_ADSP_COMMON)

target_include_directories(LS_ADSP_COMMON INTERFACE include)
target_link_libraries(LS_ADSP_COMMON INTERFACE ls_adsp_common)
