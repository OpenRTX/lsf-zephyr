# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_KSCAN_FT5336	kscan_ft5336.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_XEC		kscan_mchp_xec.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_SDL		kscan_sdl.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_HT16K33	kscan_ht16k33.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_GSL1686	kscan_gsl1686.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_BL6XXX	kscan_bl6xxx.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_BL6XXX	fw_update.c)
zephyr_library_sources_ifdef(CONFIG_KSCAN_CSK6	    kscan_csk6.c)

zephyr_library_sources_ifdef(CONFIG_USERSPACE	    kscan_handlers.c)
