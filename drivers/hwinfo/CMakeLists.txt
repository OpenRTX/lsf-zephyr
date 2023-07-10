# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__hwinfo)

target_sources_ifdef(CONFIG_HWINFO_CSK6 ${TARGET_LIB} PRIVATE hwinfo_csk6.c)
