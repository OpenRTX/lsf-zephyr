# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__kscan)

target_sources_ifdef(CONFIG_KSCAN_CSK6 ${TARGET_LIB} PRIVATE kscan_csk6.c)
