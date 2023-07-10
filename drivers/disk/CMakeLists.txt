# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_DISK_DRIVERS)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_SDMMC_CSK6 sdmmc_csk6.c)

endif()
