/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
const struct device* i2c = DEVICE_DT_GET(DT_ALIAS(i2c_0));

/**
 * 测试固件中ADC 通道对应的引脚
 * CH0 ->PA2 
 * CH1 ->PA1
 * CH2 ->PC4 
 * CH3 ->PD2 
 * CH4 ->PD3 
 * CH5 -> NONE 
 * CH6 -> NONE
 * CH7 ->PD4 
 * 
 * PC3为从机i2c地址选择引脚 IIC地址见协议 1：0x6C,0:0x15
 * 
 */
void adc_test(void)
{
	int r;

	/* 这段代码测试固件未实现，加上后影响ADC运行 */
	// r = i2c_reg_write_byte_dt(&config->bus, 0x50, 0x00);
	// if (r) {
	// 	printk("write 0x40 failed, r:%d\n", r);
	// 	return r;
	// }

	// r = i2c_reg_write_byte_dt(&config->bus, 0x51, 0x0A);
	// if (r) {
	// 	printk("write 0x41 failed, r:%d\n", r);
	// 	return r;
	// }

	/* 测试固件中0x52寄存器中的每一位表示一个通道 */
	r = i2c_reg_write_byte(i2c, 0x6c, 0x52, 0xF0);
	// r = i2c_reg_write_byte_dt(&i2c, 0x52, 0xF0);
	if (r) {
		printk("write 0x42 failed, r:%d\n", r);
		return r;
	}

	k_msleep(500);

	/* 测试固件中状态寄存器未实现,读取一直是0 */
	// uint8_t status = 0x00;
	// r = i2c_reg_read_byte_dt(&i2c, 0x55, &status);
	// if (r) {
	// 	printk("write 0x45 failed, r:%d\n", r);
	// 	return;
	// }
	// LOG_INF("status:%d", status);

	uint8_t data[2] = {0};

	/* 测试固件中0x53表示通道0的数据寄存器，0x63表示通道1的数据寄存器，依次类推，这里需要再跟覃工讨论下协议 */
	// r = i2c_reg_read_byte_dt(&i2c, 0x93, &data[0]);
	r = i2c_reg_read_byte(i2c, 0x6C, 0x93, &data[0]);
	if (r) {
		printk("write 0x43 failed, r:%d\n", r);
	}
	// r = i2c_reg_read_byte_dt(&i2c, 0x94, &data[1]);
	r = i2c_reg_read_byte(i2c, 0x6C, 0x94, &data[1]);
	if (r) {
		printk("write 0x44 failed, r:%d\n", r);
	}
	uint32_t mv = (uint32_t)(((uint16_t)data[0]) << 8 | data[1]) * 3300 / 1023;
	printk("ADC:%dmv\n", mv );
}

/**
 * 注意：测试固件中CH32V003的端口C对应CSK6的端口E
 */
void main(void)
{
	int ret;

	if (!device_is_ready(led.port)) {
		printk("led.port is not ready %s\n", led.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}
		k_msleep(SLEEP_TIME_MS);
		adc_test();
		k_msleep(SLEEP_TIME_MS);
	}
}