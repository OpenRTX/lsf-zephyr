/*
 * Copyright (c) 2022 LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kscan_bl6xxx.h"
#include "tp_ts.h"

#define DT_DRV_COMPAT betterlife_bl6xxx

LOG_MODULE_REGISTER(bl6xxx, CONFIG_KSCAN_LOG_LEVEL);

const struct device *g_i2c_dev;
const struct device *g_gpio_int;
int g_gpio_int_pin;
bl6xxx_dev_data_t * g_bl6xxx_dev_data;

void ctp_delay_us(unsigned int time)
{
	unsigned int i,j,k;
	for(i=0;i< time;i++)
	{
		for(j=0;j<100;j++)
		{
			k=0;
		}
	}
}

void ctp_delay_ms(unsigned short time)
{
    // uos_sleep(time/4);
	k_sleep(K_MSEC(time));
}

int CTP_FLASH_I2C_READ(unsigned char i2c_addr, unsigned char *value, unsigned short len)
{
	return i2c_read(g_bl6xxx_dev_data->i2c_dev, value, len, i2c_addr);
}

int CTP_FLASH_I2C_WRITE(unsigned char i2c_addr, unsigned char *value, unsigned short len)
{
	return i2c_write(g_bl6xxx_dev_data->i2c_dev, value, len, i2c_addr);
}

void bl_ts_set_intmode(char mode)
{
	if(0 == mode)
	{   
        //GPIO mode
		gpio_pin_configure(g_gpio_int, g_gpio_int_pin, GPIO_OUTPUT_ACTIVE);
	}
	else if(1 == mode)
	{
        //INT mode
		gpio_pin_configure(g_gpio_int, g_gpio_int_pin, GPIO_INPUT | GPIO_PULL_UP);
	}
}

void bl_ts_set_intup(char level)
{
    gpio_pin_set(g_gpio_int, g_gpio_int_pin, level);
}


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

	//printk("x:%d,y:%d\n", data->x, data->y);

    return 0;
}

int bl6xxx_reset_switch_int_wakemode(bl6xxx_dev_data_t *data)
{
    const struct device* i2c_dev = data->i2c_dev;

    int32_t ret;
	unsigned char cmd[4];
	cmd[0] = BL6XXX_ID_REG_1;
	cmd[1] = ~cmd[0];
	cmd[2] = BL6XXX_ID_REG_3;
	cmd[3] = 0xe8;

    ret = i2c_write(i2c_dev, cmd, 4, BL6133_CHIP_ADDR);
    if (ret < 0) {
        LOG_ERR("[BL6XXX]write data failed, ret:%d.\n", ret);
        return ret;
    }

	ctp_delay_ms(50);
	
    return ret;
}

uint8_t bl6xxx_set_int_low(bl6xxx_dev_data_t *data)
{
	const struct device *gpio = device_get_binding(data->int_gpio.port);
    int32_t r;
	r = gpio_pin_configure(gpio, data->int_gpio.pin, GPIO_OUTPUT_ACTIVE);
    if (r < 0) {
        LOG_ERR("[BL6XXX]Could not configure interrupt GPIO pin");
        return r;
    }
	// interrupt io => low level
	gpio_pin_set(gpio, data->int_gpio.pin, 0);
    
    return 0;
}

uint8_t bl6xxx_chip_get_id(uint8_t *buf, bl6xxx_dev_data_t *data)
{
    int32_t ret;
	unsigned char cmd[4];
    if (data == NULL)
    {
        data = g_bl6xxx_dev_data;
    }
    const struct device* i2c_dev = data->i2c_dev;

    cmd[0] = BL6XXX_ID_REG_1;
	cmd[1] = ~cmd[0];
	cmd[2] = BL6XXX_ID_REG_3;
    ret = i2c_write(i2c_dev, cmd, 3, BL6133_CHIP_ADDR);
    // ret = i2c_write(i2c_dev, cmd, sizeof(cmd), BL6133_CHIP_ADDR);
    
	ctp_delay_ms(1);
	ret = i2c_read(i2c_dev, buf, 1, BL6133_CHIP_ADDR);
    if (ret < 0) {
        LOG_ERR("[BL6XXX]read data failed, ret:%d.\n", ret);
        return ret;
    }
    return ret;

}

static uint8_t bl6xxx_chip_id_read(bl6xxx_dev_data_t *data)
{
    int32_t ret;
    uint8_t chipid;
	
	bl6xxx_set_int_low(data);
	ctp_delay_ms(10);

    bl6xxx_reset_switch_int_wakemode(data);
	
    ret = bl6xxx_chip_get_id(&chipid, data);
    SET_WAKEUP_HIGH;

    if((ret < 0)|| (chipid != BTL_FLASH_ID))
	{
		LOG_ERR("%s:Read chipID Fail:chipID = 0x%2x\n", __func__, chipid);
		return -1;
	}
	else
	{
		printk("%s:Read chipID succes:chipID = 0x%2x\n", __func__, chipid);
	}
    return 0;
}

static uint32_t bl6xxx_chip_uid_read(const struct device *i2c_dev)
{
    return 0;
}

static int32_t bl6xxx_chip_init(bl6xxx_dev_data_t *data)
{
    /**
     * Note that:
     * These functions are not implemented in this version of this driver,
     * because there is no corresponding data support about this IC.
     * The reset pin is also not used.
     * We will improve it in future versions.
     * Just only make complier happy at here. 
     */
    (void)bl6xxx_chip_id_read(data);
    (void)bl6xxx_chip_sleep(data->i2c_dev);
    (void)bl6xxx_chip_uid_read(data->i2c_dev);

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
			//printk("%s, x:%d,y:%d\n", __func__, chip_data.x, chip_data.y);
			if (chip_data.x <= 320 && chip_data.y <= 170)
			{
				dev_data->cb(dev_data->dev, chip_data.x, chip_data.y,
						chip_data.evnet == BL6XXX_TOUCH_EVENT_CONTACT_OR_MOVE);
			}
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
    int32_t r;
	printk("%s\n", __func__);
    r = gpio_pin_configure(device_get_binding(data->int_gpio.port), data->int_gpio.pin, GPIO_INPUT | GPIO_PULL_UP);
    if (r < 0) {
        LOG_ERR("[BL6XXX]Could not configure interrupt GPIO pin");
        return r;
    }
    r = gpio_pin_interrupt_configure(device_get_binding(data->int_gpio.port), data->int_gpio.pin,
            GPIO_INT_EDGE_TO_INACTIVE);
	if (r < 0) {
        LOG_ERR("[BL6XXX]Could not configure interrupt GPIO interrupt.");
        return r;
    }


    gpio_init_callback(&data->int_gpio_cb, bl6xxx_dev_isr_handler,
        BIT(data->int_gpio.pin));
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

	g_gpio_int = gpio;
	g_gpio_int_pin = data->int_gpio.pin; 
    g_i2c_dev = i2c_dev;
    g_bl6xxx_dev_data = data;


    

	r = bl6xxx_chip_init(data);
    if (r < 0) {
        LOG_ERR("[BL6XXX]Could not configure interrupt GPIO interrupt.");
        return r;
    }

    if (gpio == NULL) {
        LOG_ERR("[BL6XXX]interrupt gpio is not binding.\n");
        return -EIO;
    }
    // add by thierrycao
	#ifdef  BL_AUTO_UPDATE_FARMWARE
    bl_auto_update_fw();
    ctp_delay_ms(10);
	#endif

#if 0
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
#endif

    k_work_init(&(data->work), bl6xxx_dev_work_handler);
#if 0
    gpio_init_callback(&data->int_gpio_cb, bl6xxx_dev_isr_handler, BIT(data->int_gpio.pin));
    gpio_add_callback(gpio, &data->int_gpio_cb);
#endif
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
