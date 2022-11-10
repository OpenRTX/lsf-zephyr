#
# Copyright (c) 2019 Peter Bigot Consulting, LLC
#
# SPDX-License-Identifier: Apache-2.0
#
zephyr_include_directories(include)
zephyr_include_directories(src)
zephyr_library_sources(src/lisa_http.c)
zephyr_library_sources(src/lisa_websocket.c)


