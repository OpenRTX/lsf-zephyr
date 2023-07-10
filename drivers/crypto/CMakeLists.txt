# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_CRYPTO_CSK6		crypto_csk6.c)
