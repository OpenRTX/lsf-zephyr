# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# zephyr_library_sources_ifdef(CONFIG_USERSPACE          infrared_handlers.c)
# zephyr_library_sources_ifdef(CONFIG_INFRARED_SHELL     infrared_shell.c)

zephyr_library_sources_ifdef(CONFIG_INFRARED_CSK6      infrared_csk6.c)

message(STATUS "infrared CSK_SDK: ${CSK_SDK}")
list(APPEND APPLICATION_SOURCE_DIR ${CSK_SDK}/include/zephyr/drivers)
message(STATUS "infrared APPLICATION_SOURCE_DIR: ${APPLICATION_SOURCE_DIR}")

