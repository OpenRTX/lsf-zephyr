# Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(./main.c)


if(CONFIG_SOF_HOST)
zephyr_library_sources(./fs_domain.c)
zephyr_library_sources(./sof_domain.c)
endif()

if(CONFIG_WIFI)
zephyr_library_sources(./network_domain.c)
endif()

if(CONFIG_MCUMGR)
zephyr_library_sources(./mcumgr_domain.c)
zephyr_library_sources(./mcumgr_udp.c)
endif()

add_subdirectory(./lisa_mem)
