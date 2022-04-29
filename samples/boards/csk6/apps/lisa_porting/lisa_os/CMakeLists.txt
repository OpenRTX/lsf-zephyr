#
# Copyright (c) 2019 Peter Bigot Consulting, LLC
#
# SPDX-License-Identifier: Apache-2.0
#
zephyr_include_directories(include)
zephyr_library_sources(src/lisa_mem.c)
zephyr_library_sources(src/lisa_mutex.c)
zephyr_library_sources(src/lisa_queue.c)
zephyr_library_sources(src/lisa_semaphore.c)
zephyr_library_sources(src/lisa_time.c)
zephyr_library_sources(src/lisa_timer.c)
zephyr_library_sources(src/lisa_thread.c)

