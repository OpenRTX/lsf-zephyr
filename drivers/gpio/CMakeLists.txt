# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__gpio)

target_sources_ifdef(CONFIG_GPIO_CSK6 ${TARGET_LIB} PRIVATE gpio_csk6.c)
target_sources_ifdef(CONFIG_GPIO_CSK6_CH32V003 ${TARGET_LIB} PRIVATE gpio_csk6_ch32v003.c)

target_include_directories_ifdef(CONFIG_GPIO_CSK6 ${TARGET_LIB} PRIVATE ${ZEPHYR_BASE}/drivers/gpio)