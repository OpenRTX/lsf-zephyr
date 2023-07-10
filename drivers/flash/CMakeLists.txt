# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__flash)

target_sources_ifdef(CONFIG_SOC_FLASH_CSK6 ${TARGET_LIB} PRIVATE flash_csk6.c)
