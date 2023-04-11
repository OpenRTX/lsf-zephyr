#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/zephyr.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <zephyr/drivers/rtc/bm8563.h>
#include <zephyr/drivers/counter.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define RTC DT_LABEL(DT_NODELABEL(bm8563_0))

int main(void)
{
	struct tm rtc;
	int ret;

	// get the device
	const struct device *dev = device_get_binding(RTC);
	if (!dev) {
		LOG_INF("Failed to get device binding %p\n", dev);
		return -ENODEV;
	}
	
	rtc.tm_year = 2025;
	rtc.tm_mon = 3;
	rtc.tm_mday = 20;
	rtc.tm_hour = 13;
	rtc.tm_min = 20;
	rtc.tm_sec = 45;

	LOG_INF("Set RTC Time: %d-%d-%d %d:%d:%d", 	rtc.tm_year, rtc.tm_mon, rtc.tm_mday, 
													rtc.tm_hour, rtc.tm_min, rtc.tm_sec);
	ret = counter_set_time(dev, &rtc);
	if (ret != 0) {
		LOG_ERR("Failed to set time: %d\n", ret);
		return ret;
	}
	memset(&rtc, 0, sizeof(rtc));
	int cnt = 0;
	while (1)
	{
		cnt++;
		if (cnt > 5)
			return 0;
		ret = counter_get_time(dev, &rtc);
		if (ret != 0) {
			LOG_ERR("Failed to get time: %d\n", ret);
			return ret;
		}
		LOG_INF("Get RTC Time: %d-%d-%d %d:%d:%d", 	rtc.tm_year, rtc.tm_mon, rtc.tm_mday, 
												rtc.tm_hour, rtc.tm_min, rtc.tm_sec);	
		k_msleep(1000);
	}
	return 0;
}