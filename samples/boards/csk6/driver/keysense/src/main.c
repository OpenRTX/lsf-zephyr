/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/gpio.h>
#include <soc.h>
#include <drivers/kscan.h>


#define KSCAN_DEVICE DT_LABEL(DT_ALIAS(analogkey))

//#define KSCAN_NODE DT_INST(0, listenai_csk_keysense)

static void kb_callback(const struct device *dev, uint32_t row, uint32_t col,
			bool pressed)
{
	ARG_UNUSED(dev);

	printk("Key row = %d col = %d Pressed = %u\n", row, col, pressed);

}

void main(void)
{
	const struct device *kscan_dev = device_get_binding(KSCAN_DEVICE);

	printk("csk6 keysense sample application\n");

	if (!device_is_ready(kscan_dev)) {
		printk("kscan device  not ready");
		return;
	}

	kscan_config(kscan_dev, kb_callback);

}