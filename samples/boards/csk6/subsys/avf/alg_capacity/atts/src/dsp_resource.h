/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma GCC push_options
#pragma GCC optimize("O0")

static const unsigned char dsp_tplg[] = {
#include "dsp_tplg.inc"
};

__attribute__((section(".tts_data.text"))) static const unsigned char tts_data[] = {
#include "tts_data.inc"
};

__attribute__((section(".cp_image.text"))) static const unsigned char cp_firmware[] = {
#include "cp_firmware.inc"
};

#pragma GCC pop_options
