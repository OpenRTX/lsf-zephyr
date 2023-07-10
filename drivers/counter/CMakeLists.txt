# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__counter)

target_sources_ifdef(CONFIG_COUNTER_CSK6 ${TARGET_LIB} PRIVATE counter_csk6.c)
target_sources_ifdef(CONFIG_COUNTER_CSK6_DUAL ${TARGET_LIB} PRIVATE counter_dual_csk6.c)
target_sources_ifdef(CONFIG_COUNTER_CSK6_GPTCHANNEL ${TARGET_LIB} PRIVATE counter_csk6_gptchannel.c)
