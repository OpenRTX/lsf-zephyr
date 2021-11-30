#include <drivers/gpio.h>
#include "hosted_gpio.h"
#include <assert.h>

extern void spi_sync_callback(uint16_t pin);

static const struct device *gpio_dev = NULL;
static struct gpio_callback gpio_cb_body;

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

void csk_write_pin(gpio_port_t port, int pin, int level)
{
    gpio_pin_set(gpio_dev, pin, level);
}

int csk_read_pin(gpio_port_t port, int pin)
{
    return gpio_pin_get(gpio_dev, pin);
}

void hosted_gpio_init(void)
{
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    int ret = gpio_pin_configure(gpio_dev, GPIO_RESET_Pin, GPIO_OUTPUT_ACTIVE | 0);
    assert(ret == 0);
    ret |= gpio_pin_configure(gpio_dev, GPIO_DATA_READY_Pin, GPIO_INPUT);
    ret |= gpio_pin_interrupt_configure(gpio_dev, GPIO_DATA_READY_Pin, GPIO_INT_EDGE_TO_ACTIVE);
    ret |= gpio_pin_configure(gpio_dev, GPIO_HANDSHAKE_Pin, GPIO_INPUT);
    ret |= gpio_pin_interrupt_configure(gpio_dev, GPIO_HANDSHAKE_Pin, GPIO_INT_EDGE_TO_ACTIVE);
    assert(ret == 0);
    gpio_init_callback(&gpio_cb_body, gpio_isr_cb, BIT(GPIO_DATA_READY_Pin) | BIT(GPIO_HANDSHAKE_Pin));
    gpio_add_callback(gpio_dev, &gpio_cb_body);
}
