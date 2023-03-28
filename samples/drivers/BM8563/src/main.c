#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/zephyr.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "bm8563.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#if DT_NODE_HAS_STATUS(DT_ALIAS(i2c_1), okay)
#define I2C1_DEV_NAME DT_LABEL(DT_ALIAS(i2c_1))
#else
#error "can't find I2C1 device"
#endif

#define I2C_DEV_NAME "I2C_1"
#define BM8563_ADDR  0x51
#define NODE_VIRTUAL_DEV DT_NODELABEL(i2cslavevdev0)
const struct device *i2c_dev = DT_LABEL(NODE_VIRTUAL_DEV);

struct tm rtc;
char buffer[128];
bm8563_t bm;


int i2c_dev_init(const struct device **pdev, const char *dev_name, uint32_t i2c_config)
{
	*pdev = device_get_binding(dev_name);
	if (*pdev == NULL) {
		printk("%s: Device is not found.\n", dev_name);
		return -ENODEV;
	}

	if (!device_is_ready(*pdev)) {
		printk("%s: Device is not ready.\n", dev_name);
		return -ENODEV;
	}

	if (i2c_configure(*pdev, i2c_config)) {
		printk("%s: config failed\n", dev_name);
		return -EINVAL;
	}

	return 0;
}


int main(void)
{
	LOG_INF("MAIN STARTED: BM8563 RTC");
	int ret = i2c_dev_init(&i2c_dev, I2C_DEV_NAME, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER);
	if (ret < 0) {
		printk("I2C: Failed to init device.\n");
		return;
	}
	LOG_INF("INIT SUCCESS: i2c_dev_init");

	bm.read = &i2c_burst_read;
	bm.write = &i2c_burst_write;
	bm.handle = i2c_dev;
    bm8563_init(&bm);
	LOG_INF("BM8563 RTC INITIALIZED");

	while(1){
		bm8563_read(&bm, &rtc);
		LOG_INF("RTC Time: %d-%d-%d %d:%d:%d", 	rtc.tm_year, rtc.tm_mon, rtc.tm_mday, 
											rtc.tm_hour, rtc.tm_min, rtc.tm_sec);
		k_msleep(1000);
	}

    return 0;
}
