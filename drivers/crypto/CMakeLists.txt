# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__crypto)

target_sources_ifdef(CONFIG_CRYPTO_CSK6 ${TARGET_LIB} PRIVATE crypto_csk6.c)
