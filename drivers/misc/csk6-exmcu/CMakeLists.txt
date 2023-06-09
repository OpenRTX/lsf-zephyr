#
# SPDX-License-Identifier: Apache-2.0
#



zephyr_library()

zephyr_include_directories_ifdef(CONFIG_CSK6_EXMCU ./)
zephyr_library_sources_ifdef(CONFIG_CSK6_EXMCU exmcu_comm.c)
