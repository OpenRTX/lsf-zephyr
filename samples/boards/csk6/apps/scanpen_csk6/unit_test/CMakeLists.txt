#
# Copyright (c) 2019 Peter Bigot Consulting, LLC
#
# SPDX-License-Identifier: Apache-2.0
#
zephyr_include_directories(include)
zephyr_library_sources(src/mem_test.c)
zephyr_library_sources(src/mutex_text.c)
zephyr_library_sources(src/queue_test.c)
zephyr_library_sources(src/sema_test.c)
zephyr_library_sources(src/thread_test.c)
zephyr_library_sources(src/time_test.c)
zephyr_library_sources(src/http_test.c)
zephyr_library_sources(src/websocket_test.c)

