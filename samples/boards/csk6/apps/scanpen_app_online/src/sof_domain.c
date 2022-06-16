/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <lisa_mem.h>
#include <avf_platform.h>

extern void example_vcapture_mem_register(void* pmalloc,void* pcalloc,void* pfree);
void sof_domain(void){
#if(CONFIG_AVF_EXAMPLE)
    example_vcapture_mem_register(lisa_mem_alloc,lisa_mem_calloc,lisa_mem_free);
#endif
    avf_stream_platform_register();
}

