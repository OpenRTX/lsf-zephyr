# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__serial)

target_sources_ifdef(CONFIG_UART_CSK6 ${TARGET_LIB} PRIVATE uart_csk6.c)
