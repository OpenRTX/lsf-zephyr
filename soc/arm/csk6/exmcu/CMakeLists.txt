#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_include_directories(./include())

zephyr_library()

zephyr_library_sources(exmcu_comm.c)