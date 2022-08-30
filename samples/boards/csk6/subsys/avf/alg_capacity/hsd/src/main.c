/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsp_resource.h"
#include <zephyr.h>
#include <stdio.h>
#include <avf/framework/avf_platform.h>
#include <avf/modules/hsd/hsd.h>

void main(void) {
  if (NULL ==
      avf_stream_platform_register(dsp_firmware, sizeof(dsp_firmware), dsp_tplg,
				     sizeof(dsp_tplg))) {
    printk("avf platform register failed,exit.\n");
    return;
  }

  hsd_t *hsd = hsd_create();

  printk("AP EXEC END\n");
}
