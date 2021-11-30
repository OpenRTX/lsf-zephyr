#pragma once

typedef enum {
    PA_PORT,
    PB_PORT
} gpio_port_t;

#define GPIO_DATA_READY_Pin         4   // IO4
#define GPIO_DATA_READY_GPIO_Port   PA_PORT
#define GPIO_HANDSHAKE_Pin          5   // IO2
#define GPIO_HANDSHAKE_GPIO_Port    PA_PORT
#define GPIO_RESET_Pin              9   // EN
#define GPIO_RESET_GPIO_Port        PA_PORT

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1

#define HAL_GPIO_WritePin(port, pin, level) _write_pin(port, pin, level)
#define HAL_GPIO_ReadPin(port, pin) _read_pin(port, pin)

void hosted_gpio_init(void);
