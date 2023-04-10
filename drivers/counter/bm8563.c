/*

MIT License

Copyright (c) 2020-2021 Mika Tuupola

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

-cut-

This file is part of hardware agnostic I2C driver for BM8563 RTC:
https://github.com/tuupola/bm8563

SPDX-License-Identifier: MIT

*/
#define DT_DRV_COMPAT bm8563

#include <stdint.h>
#include <time.h>
#include <zephyr/device.h>
#include <zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/rtc/bm8563.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(bm8563, LOG_LEVEL_INF);

static inline uint8_t decimal2bcd (uint8_t decimal)
{
    return (((decimal / 10) << 4) | (decimal % 10));
}

static inline uint8_t bcd2decimal(uint8_t bcd)
{
   return (((bcd >> 4) * 10) + (bcd & 0x0f));
}

struct bm8563_config {
	/* Common structure first because generic API expects this here. */
	const char* i2c_dev;
    const char* i2c;
};

struct bm8563_data
{
    /* data */
    bm8563_t bm;
    void *id;
};


bm8563_err_t bm8563_init(const bm8563_t *bm)
{
    uint8_t clear = 0x00;
    int32_t status;
    status = bm->write(bm->handle, BM8563_ADDRESS, BM8563_CONTROL_STATUS1, &clear, 1);
    if (BM8563_OK != status) {
        LOG_ERR("bm8563_init: bm->write BM8563_CONTROL_STATUS1 failed");
        return status;
    }
    return bm->write(bm->handle, BM8563_ADDRESS, BM8563_CONTROL_STATUS2, &clear, 1);
}

bm8563_err_t bm8563_read(const bm8563_t *bm, struct tm *time)
{
    uint8_t bcd;
    uint8_t data[BM8563_TIME_SIZE] = {0};
    uint16_t century;
    int32_t status;

    status = bm->read(
        bm->handle, BM8563_ADDRESS, BM8563_SECONDS, data, BM8563_TIME_SIZE
    );

    if (BM8563_OK != status) {
        return status;
    }

    /* 0..59 */
    bcd = data[0] & 0b01111111;
    time->tm_sec = bcd2decimal(bcd);

    /* 0..59 */
    bcd = data[1] & 0b01111111;
    time->tm_min = bcd2decimal(bcd);

    /* 0..23 */
    bcd = data[2] & 0b00111111;
    time->tm_hour = bcd2decimal(bcd);

    /* 1..31 */
    bcd = data[3] & 0b00111111;
    time->tm_mday = bcd2decimal(bcd);

    /* 0..6 */
    bcd = data[4] & 0b00000111;
    time->tm_wday = bcd2decimal(bcd);

    /* 0..11 */
    bcd = data[5] & 0b00011111;
    time->tm_mon = bcd2decimal(bcd) - 1;

    /* If century bit set assume it is 2000. Note that it seems */
    /* that unlike PCF8563, the BM8563 does NOT automatically */
    /* toggle the century bit when year overflows from 99 to 00. */
    century = (data[5] & BM8563_CENTURY_BIT) ? 100 : 0;

    /* Number of years since 1900 */
    bcd = data[6] & 0b11111111;
    time->tm_year = bcd2decimal(bcd) + century;

    /* Calculate tm_yday. */
    // mktime(time);

    /* low voltage warning */
    if (data[0] & 0b10000000) {
        return BM8563_ERR_LOW_VOLTAGE;
    }
    LOG_DBG("bm8563_read: %d-%d-%d %d:%d:%d", time->tm_year, time->tm_mon, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
    return BM8563_OK;
}

bm8563_err_t bm8563_write(const bm8563_t *bm, const struct tm *time)
{
    int ret = 0;
    uint8_t bcd;
    uint8_t data[BM8563_TIME_SIZE] = {0};
    /* 0..59 */
    bcd = decimal2bcd(time->tm_sec);
    data[0] = bcd & 0b01111111;
    
    /* 0..59 */
    bcd = decimal2bcd(time->tm_min);
    data[1] = bcd & 0b01111111;

    /* 0..23 */
    bcd = decimal2bcd(time->tm_hour);
    data[2] = bcd & 0b00111111;

    /* 1..31 */
    bcd = decimal2bcd(time->tm_mday);
    data[3] = bcd & 0b00111111;

    /* 0..6 */
    bcd = decimal2bcd(time->tm_wday);
    data[4] = bcd & 0b00000111;

    /* 1..12 */
    bcd = decimal2bcd(time->tm_mon + 1);
    data[5] = bcd & 0b00011111;

    /* If 2000 set the century bit. */
    if (time->tm_year >= 100) {
        data[5] |= BM8563_CENTURY_BIT;
    }

    /* 0..99 */
    bcd = decimal2bcd(time->tm_year % 100);
    data[6] = bcd & 0b11111111;

    for (int i = 0; i < BM8563_TIME_SIZE; i++) {
        ret = i2c_reg_write_byte(bm->handle, BM8563_ADDRESS, BM8563_SECONDS + i, data[i]);
    }

    return ret;
}

bm8563_err_t bm8563_ioctl(const bm8563_t *bm, int16_t command, void *buffer)
{
    uint8_t reg = command >> 8;
    uint8_t data[BM8563_ALARM_SIZE] = {0};
    uint8_t status;
    struct tm *time;

    switch (command) {
    case BM8563_ALARM_SET:
        time = (struct tm *)buffer;

        /* 0..59 */
        if (BM8563_ALARM_NONE == time->tm_min) {
            data[0] = BM8563_ALARM_DISABLE;
        } else {
            data[0] = decimal2bcd(time->tm_min);
        }

        /* 0..23 */
        if (BM8563_ALARM_NONE == time->tm_hour) {
            data[1] = BM8563_ALARM_DISABLE;
        } else {
            data[1] = decimal2bcd(time->tm_hour);
            data[1] &= 0b00111111;
        }

        /* 1..31 */
        if (BM8563_ALARM_NONE == time->tm_mday) {
            data[2] = BM8563_ALARM_DISABLE;
        } else {
            data[2] = decimal2bcd(time->tm_mday);
            data[2] &= 0b00111111;
        }

        /* 0..6 */
        if (BM8563_ALARM_NONE == time->tm_mday) {
            data[3] = BM8563_ALARM_DISABLE;
        } else {
            data[3] = decimal2bcd(time->tm_wday);
            data[3] &= 0b00000111;
        }

        return bm->write(
            bm->handle, BM8563_ADDRESS, reg, data, BM8563_ALARM_SIZE
        );

        break;

    case BM8563_ALARM_READ:
        time = (struct tm *)buffer;

        /* 0..59 */
        status = bm->read(
            bm->handle, BM8563_ADDRESS, reg, data, BM8563_ALARM_SIZE
        );

        if (BM8563_OK != status) {
            return status;
        }

        if (BM8563_ALARM_DISABLE & data[0]) {
            time->tm_min = BM8563_ALARM_NONE;
        } else {
            data[0] &= 0b01111111;
            time->tm_min = bcd2decimal(data[0]);
        }

        /* 0..23 */
        if (BM8563_ALARM_DISABLE & data[1]) {
            time->tm_hour = BM8563_ALARM_NONE;
        } else {
            data[1] &= 0b00111111;
            time->tm_hour = bcd2decimal(data[1]);
        }

        /* 1..31 */
        if (BM8563_ALARM_DISABLE & data[2]) {
            time->tm_mday = BM8563_ALARM_NONE;
        } else {
            data[2] &= 0b00111111;
            time->tm_mday = bcd2decimal(data[2]);
        }

        /* 0..6 */
        if (BM8563_ALARM_DISABLE & data[3]) {
            time->tm_wday = BM8563_ALARM_NONE;
        } else {
            data[3] &= 0b00000111;
            time->tm_wday = bcd2decimal(data[3]);
        }

        return BM8563_OK;
        break;

    case BM8563_CONTROL_STATUS1_READ:
    case BM8563_CONTROL_STATUS2_READ:
    case BM8563_TIMER_CONTROL_READ:
    case BM8563_TIMER_READ:
        return bm->read(
            bm->handle, BM8563_ADDRESS, reg, (uint8_t *)buffer, 1
        );
        break;

    case BM8563_CONTROL_STATUS1_WRITE:
    case BM8563_CONTROL_STATUS2_WRITE:
    case BM8563_TIMER_CONTROL_WRITE:
    case BM8563_TIMER_WRITE:
        return bm->write(
            bm->handle, BM8563_ADDRESS, reg, (uint8_t *)buffer, 1
        );
        break;

    }

    return BM8563_ERROR_NOTTY;
}

bm8563_err_t bm8563_close(const bm8563_t *bm)
{
    return BM8563_OK;
}

static int bm8563_counter_start(const struct device *dev)
{
	return -EALREADY;
}

static int bm8563_counter_stop(const struct device *dev)
{
	return -ENOTSUP;
}

static int bm8563_counter_get_time(const struct device *dev,
				    void *rtc)
{
	struct bm8563_data *data = dev->data;    
	struct tm *_rtc = (struct tm *)rtc;

    int rc = bm8563_read(&data->bm, _rtc);
    if (rc < 0) {
        LOG_ERR("bm8563_read failed: %d", rc);
        return rc;
    }

	if (rc >= 0) {
        LOG_DBG("get time: %d-%d-%d %d:%d:%d", _rtc->tm_year, _rtc->tm_mon, _rtc->tm_mday, _rtc->tm_hour, _rtc->tm_min, _rtc->tm_sec);
        _rtc->tm_year += 1900;
        _rtc->tm_mon += 1;
	}

	return rc;
}

// set time
static int bm8563_counter_set_time(const struct device *dev,
                    void* rtc)
{
    struct bm8563_data *data = dev->data;
    struct tm *_rtc = (struct tm *)rtc;

    _rtc->tm_year -= 1900;
    _rtc->tm_mon -= 1;

    int rc = bm8563_write(&data->bm, _rtc);
    if (rc < 0) {
        LOG_ERR("bm8563_write failed: %d", rc);
        return rc;
    }
    LOG_DBG("set time: %d-%d-%d %d:%d:%d", _rtc->tm_year, _rtc->tm_mon, _rtc->tm_mday, _rtc->tm_hour, _rtc->tm_min, _rtc->tm_sec);

    return rc;
}


int i2c_dev_init(const struct device **pdev, const char *dev_name, uint32_t i2c_config)
{
	*pdev = device_get_binding(dev_name);
	if (*pdev == NULL) {
		LOG_ERR("%s: Device is not found.", dev_name);
		return -ENODEV;
	}

	if (!device_is_ready(*pdev)) {
		LOG_ERR("%s: Device is not ready.", dev_name);
		return -ENODEV;
	}

	if (i2c_configure(*pdev, i2c_config)) {
		LOG_ERR("%s: config failed", dev_name);
		return -EINVAL;
	}

	return 0;
}

int bm8563_i2c_write(void *handle, uint8_t address, uint8_t reg, const uint8_t *buffer, uint16_t size){
    int ret = i2c_burst_write((const struct device *)handle, (uint16_t)address, reg, buffer, (uint32_t)size);
    return ret;
}

int bm8563_i2c_read(void *handle, uint8_t address, uint8_t reg, uint8_t *buffer, uint16_t size){
    return i2c_burst_read(handle, address, reg, buffer, size);
}

static int bm8563_device_init(const struct device *dev)
{
    struct bm8563_data *data = dev->data;
	const struct bm8563_config *cfg = dev->config;

    const struct device *i2c_dev = device_get_binding(cfg->i2c);

	int ret = i2c_dev_init(&i2c_dev, (const char *)cfg->i2c, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER);
	if (ret < 0) {
		LOG_ERR("I2C: Failed to init device.\n");
		return -1;
	}
    
	data->bm.read = bm8563_i2c_read;
	data->bm.write = bm8563_i2c_write;
	data->bm.handle = (void *)i2c_dev;
    bm8563_init(&data->bm);
    i2c_reg_write_byte(i2c_dev, BM8563_ADDRESS, BM8563_CONTROL_STATUS1, 0x00);      // 启动RTC
    return 0;
}

static int bm8563_counter_get_value(const struct device *dev, uint32_t *ticks)
{
    return -ENOTSUP;
}


static uint32_t bm8563_counter_get_frequency(const struct device *dev)
{
    return -ENOTSUP;
}



static const struct counter_driver_api bm8563_api = {
	.start = bm8563_counter_start,
	.stop = bm8563_counter_stop,
	.get_value = bm8563_counter_get_value,
    .get_time = bm8563_counter_get_time,
    .set_time = bm8563_counter_set_time,
    // .set_value = bm8563_counter_set_value,
    .get_freq = bm8563_counter_get_frequency,
	// .set_alarm = bm8563_counter_set_alarm,
	// .cancel_alarm = bm8563_counter_cancel_alarm,
	// .set_top_value = bm8563_counter_set_top_value,
	// .get_pending_int = bm8563_counter_get_pending_int,
	// .get_top_value = bm8563_counter_get_top_value,
};


#define BM8563_INIT(n)						 \
										 \
    static struct bm8563_data bm8563_data_##n;  \
										 \
    static const struct bm8563_config bm8563_config_##n = { \
        .i2c_dev = DT_LABEL(DT_NODELABEL(bm8563_##n)),    \
        .i2c = DT_LABEL(DT_BUS(DT_NODELABEL(bm8563_##n)))    \
    };                                                      \
    DEVICE_DT_INST_DEFINE(n, bm8563_device_init, NULL, &bm8563_data_##n,   \
                &bm8563_config_##n, \
                APPLICATION, CONFIG_COUNTER_INIT_PRIORITY,  \
                &bm8563_api);\

DT_INST_FOREACH_STATUS_OKAY(BM8563_INIT)
