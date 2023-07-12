# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# zephyr_library_sources_ifdef(CONFIG_USERSPACE          infrared_handlers.c)
# zephyr_library_sources_ifdef(CONFIG_INFRARED_SHELL     infrared_shell.c)


# zephyr_include_directories(
#   include
# )
target_include_directories(zephyr_interface BEFORE INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR})

zephyr_library_sources_ifdef(CONFIG_INFRARED_CSK6      infrared_csk6.c)
