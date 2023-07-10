# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__watchdog)

target_sources_ifdef(CONFIG_WDT_CSK6 ${TARGET_LIB} PRIVATE wdt_csk6.c)

