#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_library_named(esp_hosted_glue)
zephyr_interface_library_named(esp_hosted_glue_interface)

add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR} esp-hosted)

zephyr_include_directories_ifdef(CONFIG_ESP_HOSTED
    include
)

zephyr_library_sources_ifdef(CONFIG_ESP_HOSTED
    platform_wrapper.c
    hosted.c
)

