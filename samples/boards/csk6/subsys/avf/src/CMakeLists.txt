# Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(./main.c)
zephyr_library_sources(./fs_domain.c)
if(CONFIG_SOF_HOST)
zephyr_library_sources(./sof_domain.c)
endif()

add_subdirectory(./lisa_mem)
