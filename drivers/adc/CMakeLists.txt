# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ADC_CSK6		adc_csk6.c)
zephyr_library_sources_ifdef(CONFIG_ADC_CSK6_CH32V003	adc_csk6_ch32v003.c)

if(${CONFIG_ADC_CSK6})
	zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/adc)
endif()

if(${CONFIG_ADC_CSK6_CH32V003})
	zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/adc)
endif()


