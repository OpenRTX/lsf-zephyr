/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <avf_platform.h>

void sof_domain(void){
    avf_stream_platform_register();
}

