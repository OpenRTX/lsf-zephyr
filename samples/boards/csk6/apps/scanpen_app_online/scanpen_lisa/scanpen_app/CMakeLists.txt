#
# Copyright (c) 2019 Peter Bigot Consulting, LLC
#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_include_directories(scanpen_auth)
zephyr_include_directories(scanpen_config)
zephyr_include_directories(scanpen_uart)
# zephyr_include_directories(scanpen_unit_test)
zephyr_include_directories(include)

zephyr_library_sources(src/scanpen_app.c)
zephyr_library_sources(scanpen_auth/evs_auth_mgr.c)
zephyr_library_sources(scanpen_uart/listenai_uart_protocol.c)
# zephyr_library_sources(scanpen_unit_test/unit_test_rule.c)





