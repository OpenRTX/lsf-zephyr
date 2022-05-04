#
# Copyright (c) 2019 Peter Bigot Consulting, LLC
#
# SPDX-License-Identifier: Apache-2.0
#
zephyr_include_directories(include)
zephyr_library_sources(src/lisa_vcapture.c)
zephyr_library_sources(src/lisa_acapture.c)
zephyr_library_sources(src/lisa_buttons.c)
