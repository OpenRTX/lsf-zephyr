#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>

#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_typedef.h>
#include <lisa_err.h>

//#include "lisa_vcapture.h"
#define TAG "lisa_button"

#define SLEEP_TIME_MS 1

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, { 0 });
static struct gpio_callback button_cb_data;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, { 0 });

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int val = gpio_pin_get_dt(&button);
	if (val >= 0) {
		gpio_pin_set_dt(&led, val);
	}
	LISA_LOGD(TAG, "Button pressed val %d \n", val);
}

lisa_err_t button_init(void)
{
	int ret;

	if (!device_is_ready(button.port)) {
		LISA_LOGE(TAG, "Error: button device %s is not ready\n", button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LISA_LOGE(TAG, "Error %d: failed to configure %s pin %d\n", ret, button.port->name,
			  button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH | GPIO_INT_DEBOUNCE);
	if (ret != 0) {
		LISA_LOGE(TAG, "Error %d: failed to configure interrupt on %s pin %d\n", ret,
			  button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	LISA_LOGD(TAG, "Set up button at %s pin %d\n", button.port->name, button.pin);

	if (led.port && !device_is_ready(led.port)) {
		LISA_LOGE(TAG, "Error %d: LED device %s is not ready; ignoring it\n", ret,
			  led.port->name);
		led.port = NULL;
	}
	if (led.port) {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (ret != 0) {
			LISA_LOGE(TAG, "Error %d: failed to configure LED device %s pin %d\n", ret,
				  led.port->name, led.pin);
			led.port = NULL;
		} else {
			LISA_LOGD(TAG, "Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}

	LISA_LOGD(TAG, " button init success\n");
#if 0
	if (led.port) {
		while (1) {
			/* If we have an LED, match its state to the button's. */
			int val = gpio_pin_get_dt(&button);

			if (val >= 0) {
				gpio_pin_set_dt(&led, val);
			}
			k_msleep(SLEEP_TIME_MS);
		}
	}
#endif
}
