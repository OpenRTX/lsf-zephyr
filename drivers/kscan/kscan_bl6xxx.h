#ifndef _KSCAN_BL6XXX_H_ 
#define _KSCAN_BL6XXX_H_

#include <stdint.h>
#include <logging/log.h>
#include <drivers/kscan.h>
#include <drivers/i2c.h>
#include <drivers/gpio.h>


#define BL6XXX_ID_REG_1     0x0A
#define BL6XXX_ID_REG_2     0xF5
#define BL6XXX_ID_REG_3     0xE7
#define BL6XXX_ID_REG_4     0xE8
#define BL6XXX_POINT_REG    0x01
#define BL6XXX_SLEEP_REG    0xA5
#define BL6XXX_SLEEP_VAL    0x01
#define BL6XXX_SELF_UID_REG 0x16
#define BL6XXX_GP_UID_VAL   0x57FACE
#define BL6XXX_GP_UID_DATA_SIZE 3
#define BL6XXX_POINT_DATA_SIZE  6

#define BL6133_CHIP_ADDR 0x2c

#ifndef NULL
#define NULL 0
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

typedef enum {
    BL6XXX_TOUCH_EVENT_DOWN,
    BL6XXX_TOUCH_EVENT_UP,
    BL6XXX_TOUCH_EVENT_CONTACT_OR_MOVE,
    BL6XXX_TOUCH_EVENT_NONE,
} bl6xxx_touch_event_t;

typedef struct bl6xxx_chip_data {
    uint8_t gesture_code;
    uint8_t point_number;
    bl6xxx_touch_event_t evnet;
    uint16_t x;
    uint16_t y;
    uint8_t touch_id;
} bl6xxx_chip_data_t;

typedef struct gpio_info {
    const char *port;
    gpio_pin_t pin;
    gpio_dt_flags_t flag;
} gpio_info_t;

typedef struct bl6xxx_dev_data {
    struct k_work work;
    kscan_callback_t cb;
    const struct device *dev;
    const struct device *i2c_dev;
    struct gpio_callback int_gpio_cb;
    gpio_info_t int_gpio;
    gpio_info_t reset_gpio;
} bl6xxx_dev_data_t;

#endif