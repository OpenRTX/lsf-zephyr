/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <time.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/zephyr.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(rtc, LOG_LEVEL_INF);

#define RTC DT_LABEL(DT_NODELABEL(rtc))

int rtc_user_set_time(const struct device *dev, struct tm *tm)
{
	LOG_INF("Set RTC Time: %d-%d-%d %d:%d:%d", tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	int ret = rtc_set_time(dev, (struct rtc_time *)tm);
	if (ret != 0) {
		LOG_ERR("Failed to set time: %d\n", ret);
		return ret;
	}
	return ret;
}

int rtc_user_get_time(const struct device *dev, struct tm *tm)
{
	int ret = 0;
	ret = rtc_get_time(dev, (struct rtc_time *)tm);

	if (ret != 0) {
		LOG_ERR("Failed to get time: %d\n", ret);
		return ret;
	}
	LOG_DBG("Get RTC Time: %d-%d-%d %d:%d:%d", tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	return ret;
}

const struct device *rtc_dev;

int main()
{
	struct tm time;
	rtc_dev = device_get_binding(RTC);
	if (!rtc_dev) {
		LOG_INF("Failed to get device binding %p\n", rtc_dev);
		return -ENODEV;
	}
#if SET_RTC_TIME
	time.tm_year = 2023 - 1900;
	time.tm_mon = 6;
	time.tm_mday = 5;
	time.tm_hour = 14;
	time.tm_min = 30;
	time.tm_sec = 10;
	rtc_user_set_time(rtc_dev, &time);
#endif

	for (;;) {
		rtc_user_get_time(rtc_dev, &time);
		printk("RTC Time: %d-%d-%d %d:%d:%d\n", time.tm_year + 1900, time.tm_mon,
		       time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
		k_sleep(K_SECONDS(1));
	}
	return 0;
}
