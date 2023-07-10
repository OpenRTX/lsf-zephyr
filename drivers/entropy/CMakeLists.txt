# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ENTROPY_CSK6_RNG           entropy_csk6.c)
