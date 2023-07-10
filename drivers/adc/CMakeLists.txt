# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__adc)

target_sources_ifdef(CONFIG_ADC_CSK6 ${TARGET_LIB} PRIVATE adc_csk6.c)
target_include_directories_ifdef(CONFIG_ADC_CSK6 ${TARGET_LIB} PRIVATE ${ZEPHYR_BASE}/drivers/adc)

target_sources_ifdef(CONFIG_ADC_CSK6_CH32V003 ${TARGET_LIB} PRIVATE adc_csk6_ch32v003.c)
target_include_directories_ifdef(CONFIG_ADC_CSK6_CH32V003 ${TARGET_LIB} PRIVATE ${ZEPHYR_BASE}/drivers/adc)
