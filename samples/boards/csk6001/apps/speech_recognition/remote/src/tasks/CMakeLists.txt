# Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

# add_subdirectory(cJSON)
add_subdirectory(res)
add_subdirectory(algo)

if(${CONFIG_SOC_LS_ADSP_CSK6})
#zephyr_library()
zephyr_include_directories(./)
zephyr_library_sources(./task_algo.c)
else()
    message(FATAL_ERROR 'Unsupport the platform')
endif()	