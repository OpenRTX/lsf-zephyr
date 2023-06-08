/*
 * Copyright (c) 2022 LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>

#include <drivers/kscan.h>
#include <drivers/i2c.h>
#include <drivers/gpio.h>
#include <logging/log.h>

#define DT_DRV_COMPAT betterlife_bl6xxx

LOG_MODULE_REGISTER(bl6xxx, CONFIG_KSCAN_LOG_LEVEL);

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

static int32_t bl6xxx_chip_sleep(const struct device *i2c_dev)
{
    int32_t ret;
    uint8_t tx_data[] = {BL6XXX_SLEEP_REG, BL6XXX_SLEEP_VAL};


    ret = i2c_write(i2c_dev, tx_data, sizeof(tx_data), BL6133_CHIP_ADDR);
    if (ret < 0) {
        LOG_ERR("[BL6XXX]send chip sleep cmd faild.\n");
    }

    return ret;
}

static int32_t bl6xxx_chip_data_read(const struct device *i2c_dev, bl6xxx_chip_data_t *data)
{
    int32_t ret;
    uint8_t buf[6] = {0};
    uint8_t reg = BL6XXX_POINT_REG;

    ret = i2c_write(i2c_dev, &reg, sizeof(reg), BL6133_CHIP_ADDR);
    if (ret < 0) {
        LOG_ERR("[BL6XXX]read data failed, ret:%d.\n", ret);
        return ret;
    }

    ret = i2c_read(i2c_dev, buf, sizeof(buf), BL6133_CHIP_ADDR);
    if (ret < 0) {
        LOG_ERR("[BL6XXX]read data failed, ret:%d.\n", ret);
        return ret;
    }

    /**
     * BYTE0 | gesture_code[7:0]                    |
     * BYTE1 | point_number[7:0]                    |
     * BYTE2 | evnet[7:6]    | x_high[5:0]          |
     * BYTE3 | x_low[7:0]                           |
     * BYTE4 | touch_id[7:4] | y_high[3:0]          |
     * BYTE5 | y_low[7:0]                           |
     * 
     */
    data->gesture_code = buf[0];
    data->point_number = buf[1];
    data->evnet = buf[2] >> 6;
    data->x = (((uint16_t)(buf[2] & 0x3F)) << 8) | buf[3];
    data->touch_id = buf[4] >> 4;
    data->y = (((uint16_t)(buf[4] & 0x0F)) << 8) | buf[5];

    return 0;
}

static uint8_t bl6xxx_chip_id_read(const struct device *i2c_dev)
{
    return 0;
}

static uint32_t bl6xxx_chip_uid_read(const struct device *i2c_dev)
{
    int32_t ret;
    uint8_t buf[BL6XXX_GP_UID_DATA_SIZE] = {0};
    uint8_t reg = BL6XXX_SELF_UID_REG;

    ret = i2c_write(i2c_dev, &reg, sizeof(reg), BL6133_CHIP_ADDR);
    if (ret < 0) {
        LOG_ERR("[BL6XXX]read data failed, ret:%d.\n", ret);
        return ret;
    }

    ret = i2c_read(i2c_dev, buf, sizeof(buf), BL6133_CHIP_ADDR);
    if (ret < 0) {
        LOG_ERR("[BL6XXX]read data failed, ret:%d.\n", ret);
        return ret;
    }
    
    if(buf[0] == 0){
        ret = -ENODATA;
        LOG_ERR("[BL6XXX]read uid failed, ret:%d.\n", ret);
        return ret;
    }
    
    return ret;
}

static int32_t bl6xxx_chip_init(const struct device *i2c_dev)
{
    /**
     * Note that:
     * These functions are not implemented in this version of this driver,
     * because there is no corresponding data support about this IC.
     * The reset pin is also not used.
     * We will improve it in future versions.
     * Just only make complier happy at here. 
     */
    int32_t ret = 0;

    ret = bl6xxx_chip_uid_read(i2c_dev);
    if(ret != 0){
        LOG_ERR("[BL6XXX]init failed, ret:%d.\n", ret);
        return ret;
    }
    (void)bl6xxx_chip_sleep(i2c_dev);
    (void)bl6xxx_chip_id_read(i2c_dev);

    return 0;
}

static void bl6xxx_dev_isr_handler(const struct device *dev,
	struct gpio_callback *cb, uint32_t pins)
{
    bl6xxx_dev_data_t *data = CONTAINER_OF(cb, bl6xxx_dev_data_t, int_gpio_cb);
    k_work_submit(&data->work);
}

static void bl6xxx_dev_work_handler(struct k_work *work)
{
    bl6xxx_chip_data_t chip_data = {0};
    struct bl6xxx_dev_data *dev_data = CONTAINER_OF(work, struct bl6xxx_dev_data, work);

    if (bl6xxx_chip_data_read(dev_data->i2c_dev, &chip_data) == 0) {
        if (dev_data->cb != NULL) {
            /** 
             * It looks like this ic is always outputs event 2 when the screen is touched,
             * and event 1 (press event) never outputs.
             */
            dev_data->cb(dev_data->dev, chip_data.x, chip_data.y,
                chip_data.evnet == BL6XXX_TOUCH_EVENT_CONTACT_OR_MOVE);
        }
    } else {
        LOG_ERR("[BL6XXX]Read chip data failed!\n");
    }
}

static int kscan_driv_config(const struct device *dev, kscan_callback_t callback)
{
    if (callback == NULL) {
        return -1;
    }

    bl6xxx_dev_data_t *data = (bl6xxx_dev_data_t *)dev->data;
    data->cb = callback;

    return 0;
}

static int ksacn_driv_enable(const struct device *dev)
{
    bl6xxx_dev_data_t *data = (bl6xxx_dev_data_t *)dev->data;

    gpio_add_callback(device_get_binding(data->int_gpio.port), &data->int_gpio_cb);

    return 0;
}

static int kscan_driv_disable(const struct device *dev)
{
    bl6xxx_dev_data_t *data = (bl6xxx_dev_data_t *)dev->data;

    gpio_remove_callback(device_get_binding(data->int_gpio.port), &data->int_gpio_cb);

    return 0;
}

static const struct kscan_driver_api bl6xxx_driver_api = {
	.config = kscan_driv_config,
	.enable_callback = ksacn_driv_enable,
	.disable_callback = kscan_driv_disable,
};

static int bl6xxx_dev_init(const struct device *dev)
{
    int32_t r;
    bl6xxx_dev_data_t *data = (bl6xxx_dev_data_t *)(dev->data);
    const struct device *i2c_dev = data->i2c_dev;
    const struct device *gpio = device_get_binding(data->int_gpio.port);

    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("[BL6XXX]i2c device is not ready.\n");
        return -1;
    }
    r = i2c_configure(i2c_dev, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER);
    if (r < 0) {
        LOG_ERR("[BL6XXX] i2c config failed\n");
        return -1;
    }

    if (gpio == NULL) {
        LOG_ERR("[BL6XXX]interrupt gpio is not binding.\n");
        return -EIO;
    }
    r = gpio_pin_configure(gpio, data->int_gpio.pin, GPIO_INPUT | GPIO_PULL_UP);
    if (r < 0) {
        LOG_ERR("[BL6XXX]Could not configure interrupt GPIO pin");
        return r;
    }
    r = gpio_pin_interrupt_configure(gpio, data->int_gpio.pin,
            GPIO_INT_EDGE_TO_INACTIVE);
	if (r < 0) {
        LOG_ERR("[BL6XXX]Could not configure interrupt GPIO interrupt.");
        return r;
    }

    r = bl6xxx_chip_init(i2c_dev);
    if (r < 0) {
        LOG_ERR("[BL6XXX]Could not configure interrupt GPIO interrupt.");
        return r;
    }

    k_work_init(&(data->work), bl6xxx_dev_work_handler);

    gpio_init_callback(&data->int_gpio_cb, bl6xxx_dev_isr_handler,
        BIT(data->int_gpio.pin));
    gpio_add_callback(gpio, &data->int_gpio_cb);

    return 0;
}

#define BL6XXX_DEFINE_GPIO(index, gpio_pha)                                 \
    {                                                                       \
		.port = DT_INST_GPIO_LABEL(index, gpio_pha),			            \
		.pin = DT_INST_GPIO_PIN(index, gpio_pha),			                \
		.flag = DT_INST_GPIO_FLAGS(index, gpio_pha),			            \
    }

#define BL6XXX_DEFINE_DATA(index)                                           \
    static struct bl6xxx_dev_data bl6xxx_dev_data_##index = {               \
        .i2c_dev = DEVICE_DT_GET(DT_INST_BUS(index)),                       \
        .int_gpio = BL6XXX_DEFINE_GPIO(index, int_gpios),                   \
        .reset_gpio = BL6XXX_DEFINE_GPIO(index, reset_gpios),               \
    }
    
#define BL6XXX_INIT(index)                                                  \
    BL6XXX_DEFINE_DATA(index);                                              \
	DEVICE_DT_INST_DEFINE(index, bl6xxx_dev_init, NULL,			            \
	    &bl6xxx_dev_data_##index, NULL,                                     \
	    POST_KERNEL, CONFIG_KSCAN_INIT_PRIORITY,	                        \
	    &bl6xxx_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BL6XXX_INIT)
