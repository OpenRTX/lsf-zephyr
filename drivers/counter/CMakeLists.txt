# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_COUNTER_CSK6                counter_csk6.c)
zephyr_library_sources_ifdef(CONFIG_COUNTER_CSK6_DUAL           counter_dual_csk6.c)
zephyr_library_sources_ifdef(CONFIG_COUNTER_CSK6_GPTCHANNEL     counter_csk6_gptchannel.c)
