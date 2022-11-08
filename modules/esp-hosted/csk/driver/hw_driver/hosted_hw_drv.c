#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <assert.h>

#include "hosted_hw_drv.h"

struct esp_hosted_hw_obj {
	struct spi_dt_spec bus_spec;
	struct gpio_dt_spec dataready_spec;
	struct gpio_dt_spec handshake_spec;
	struct gpio_dt_spec reset_spec;
};

#if DT_NODE_HAS_STATUS(DT_NODELABEL(wifi_module), okay)

#if !DT_NODE_HAS_STATUS(DT_BUS(DT_NODELABEL(wifi_module)), okay)
#error "Bus node (spi) status is not okay"
#endif
#if !DT_NODE_HAS_STATUS(DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(wifi_module), dataready_gpios, 0), okay) || \
	!DT_NODE_HAS_STATUS(DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(wifi_module), handshake_gpios, 0), okay) || \
	!DT_NODE_HAS_STATUS(DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(wifi_module), reset_gpios, 0), okay)
#error "GPIO node status is not okay"
#endif

static const struct esp_hosted_hw_obj hosted_hw_obj = {
	.bus_spec = SPI_DT_SPEC_GET(
		DT_NODELABEL(wifi_module),
		SPI_OP_MODE_MASTER | SPI_WORD_SET(32) | SPI_MODE_CPOL | SPI_TRANSFER_LSB, 0),
	.dataready_spec = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(wifi_module), dataready_gpios, {0}),
	.handshake_spec = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(wifi_module), handshake_gpios, {0}),
	.reset_spec = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(wifi_module), reset_gpios, {0}),
};
#else
#error "WiFi Module node status is not okay"
#endif

extern void spi_sync_callback(uint16_t pin);

static struct gpio_callback gpio_cb_body;

int hosted_spi_init(void)
{
	if (!device_is_ready(hosted_hw_obj.bus_spec.bus)) {
		printk("SPI device %s is not ready\n", hosted_hw_obj.bus_spec.bus->name);
		return -EIO;
	}
	return 0;
}

int hosted_spi_send_recv(void *tx_data, void *rx_data, uint32_t max_buffer_len, k_timeout_t timeout)
{
	struct spi_buf tx_buf = {.buf = tx_data, .len = max_buffer_len};
	struct spi_buf rx_buf = {.buf = rx_data, .len = max_buffer_len};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1,
	};
	spi_transceive_dt(&hosted_hw_obj.bus_spec, &tx, &rx);
	return 0;
}

void gpio_isr_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int pin = 0;
	while (pins) {
		if (pins & 0x01) {
			spi_sync_callback(pin);
		}
		pin += 1;
		pins >>= 1;
	}
}

void hosted_gpio_init(void)
{
	int ret = 0;
	if (!device_is_ready(hosted_hw_obj.dataready_spec.port) ||
	    !device_is_ready(hosted_hw_obj.handshake_spec.port) ||
	    !device_is_ready(hosted_hw_obj.reset_spec.port)) {
		printk("Error: hosted gpio is not ready\n");
		return;
	}

	ret = gpio_pin_configure_dt(&hosted_hw_obj.dataready_spec, GPIO_INPUT);
	assert(ret == 0);
	ret = gpio_pin_configure_dt(&hosted_hw_obj.handshake_spec, GPIO_INPUT);
	assert(ret == 0);
	ret = gpio_pin_configure_dt(&hosted_hw_obj.reset_spec, GPIO_OUTPUT);
	assert(ret == 0);

	ret = gpio_pin_interrupt_configure_dt(&hosted_hw_obj.dataready_spec,
					      GPIO_INT_EDGE_TO_ACTIVE);
	assert(ret == 0);
	ret = gpio_pin_interrupt_configure_dt(&hosted_hw_obj.handshake_spec,
					      GPIO_INT_EDGE_TO_ACTIVE);
	assert(ret == 0);
	gpio_init_callback(&gpio_cb_body, gpio_isr_cb,
			   BIT(hosted_hw_obj.dataready_spec.pin) |
				   BIT(hosted_hw_obj.handshake_spec.pin));
	ret = gpio_add_callback(hosted_hw_obj.dataready_spec.port, &gpio_cb_body);
	assert(ret == 0);
	ret = gpio_add_callback(hosted_hw_obj.handshake_spec.port, &gpio_cb_body);
	assert(ret == 0);
}

int hosted_gpio_read_handshake(void)
{
	return gpio_pin_get_dt(&hosted_hw_obj.handshake_spec);
}

int hosted_gpio_read_dataready(void)
{
	return gpio_pin_get_dt(&hosted_hw_obj.dataready_spec);
}

void hosted_gpio_write_reset(int value)
{
	gpio_pin_set_dt(&hosted_hw_obj.reset_spec, value);
}

bool hosted_gpio_is_handshake(int pin)
{
	return hosted_hw_obj.handshake_spec.pin == pin;
}

bool hosted_gpio_is_dataready(int pin)
{
	return hosted_hw_obj.dataready_spec.pin == pin;
}
