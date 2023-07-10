# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

set(TARGET_LIB drivers__pinctrl)

target_sources_ifdef(CONFIG_PINCTRL_CSK6 ${TARGET_LIB} PRIVATE pinctrl_csk6.c)
