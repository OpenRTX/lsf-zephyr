# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__spi)

target_sources_ifdef(CONFIG_SPI_CSK6 ${TARGET_LIB} PRIVATE spi_csk6.c)
target_include_directories_ifdef(CONFIG_SPI_CSK6 ${TARGET_LIB} PRIVATE ${ZEPHYR_BASE}/drivers/spi)
