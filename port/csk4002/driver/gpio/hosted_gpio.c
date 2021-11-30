#include "hosted_gpio.h"
#include "IOMuxManager.h"

#include "Driver_GPIO.h"
#include "venus_log.h"

extern void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

static void *gGpioDev = NULL;

static void gpio_isr(uint32_t event)
{
    int bit = 0;
    // CLOGD("Trigger GPIOA interrupt: %d", event);
    while (event) {
        if (event & 0x01) {
            HAL_GPIO_EXTI_Callback(bit);
        }
        bit += 1;
        event >>= 1;
    }
}

void* gpio_get_driver_context(gpio_port_t port)
{
    void* driver_context = NULL;
    if (port == PA_PORT) {
        driver_context = GPIOA();
    } else if (port == PB_PORT) {
        driver_context = GPIOB();
    } else {
        return NULL;
    }
    return driver_context;
}

void _write_pin(gpio_port_t port, int pin, int level)
{
    void* driver_context = gpio_get_driver_context(port);
    if (driver_context == NULL) {
        CLOGE("port err");
        return;
    }
    GPIO_PinWrite(driver_context, 1UL << pin, level);
}

uint8_t _read_pin(gpio_port_t port, int pin)
{
    void* driver_context = gpio_get_driver_context(port);
    if (driver_context == NULL) {
        CLOGE("port err");
        return;
    }
    return GPIO_PinRead(driver_context, 1UL << pin);
}

#include "gpio.h"
void hosted_gpio_init(void)
{
    gGpioDev = GPIOA();
    GPIO_Initialize(gGpioDev, gpio_isr, NULL);
    GPIO_SetDir(gGpioDev, 1UL << GPIO_HANDSHAKE_Pin, CSK_GPIO_DIR_INPUT);
    GPIO_SetDir(gGpioDev, 1UL << GPIO_RESET_Pin, CSK_GPIO_DIR_OUTPUT);
    GPIO_Control(gGpioDev, CSK_GPIO_SET_INTR_POSITIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                            1UL << GPIO_DATA_READY_Pin);
    GPIO_Control(gGpioDev, CSK_GPIO_SET_INTR_POSITIVE_EDGE | CSK_GPIO_INTR_ENABLE,
                            1UL << GPIO_HANDSHAKE_Pin);
}
