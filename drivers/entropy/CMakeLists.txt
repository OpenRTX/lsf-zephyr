# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__entropy)

target_sources_ifdef(CONFIG_ENTROPY_CSK6_RNG ${TARGET_LIB} PRIVATE entropy_csk6.c)
