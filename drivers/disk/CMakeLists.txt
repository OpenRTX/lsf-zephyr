# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_DISK_DRIVERS)

set(TARGET_LIB drivers__disk)
target_sources_ifdef(CONFIG_SDMMC_CSK6 ${TARGET_LIB} PRIVATE sdmmc_csk6.c)

endif()
