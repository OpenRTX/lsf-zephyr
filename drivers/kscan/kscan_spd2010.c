/*
 * Copyright (c) 2022 LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>

#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <drivers/kscan.h>
#include <logging/log.h>

#define DT_DRV_COMPAT solomon_spd2010_kscan

LOG_MODULE_REGISTER(spd2010, CONFIG_KSCAN_LOG_LEVEL);

#define SPD2010_CHIP_ADDR 0x53

#ifndef NULL
#define NULL 0
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

struct spd2010_data {
	struct k_work_delayable work;
	kscan_callback_t cb;
	struct gpio_callback int_gpio_cb;
	const struct device *device;
};

struct spd2010_config {
	struct gpio_dt_spec reset;
	struct gpio_dt_spec int_gpio;
	struct i2c_dt_spec bus;
};

struct spd2010_status_l {
	uint8_t pt_exist;
	uint8_t gesture;
	uint8_t key;
	uint8_t aux;
	uint8_t keep;
	uint8_t raworpt;
	uint8_t none6;
	uint8_t none7;
};

struct spd2010_status_h {
	uint8_t none0;
	uint8_t none1;
	uint8_t none2;
	uint8_t cpu_run;
	uint8_t tint_low;
	uint8_t tic_in_cpu;
	uint8_t tic_in_bios;
	uint8_t tic_in_busy;
};

struct spd2010_status {
	struct spd2010_status_h status_h;
	struct spd2010_status_l status_l;
	uint16_t readlen;
};

struct spd2010_tp_report {
	uint8_t id;
	uint8_t weight;
	uint16_t x;
	uint16_t y;
};

struct spd2010_touch {
	struct spd2010_tp_report reports[10];
	uint8_t touch_num;
	uint8_t pack_code;
	uint8_t down;
	uint8_t up;
	uint8_t gesture;
	uint16_t down_x;
	uint16_t down_y;
	uint16_t up_x;
	uint16_t up_y;
};

struct spd2010_hdp_status {
	uint8_t sta;
	uint16_t next_packet_len;
};

static int spd2010_hdp_remain_data_read(const struct device *i2c_dev, uint16_t len)
{
	uint8_t datas[32] = {0};
	int ret;

	datas[0] = 0x00;
	datas[1] = 0x03;

	ret = i2c_write(i2c_dev, datas, 2, SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
		return ret;
	}
	ret = i2c_read(i2c_dev, datas, len, SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
		return ret;
	}

	return ret;
}

static int spd2010_tp_status_read(const struct device *i2c_dev, struct spd2010_status *status)
{
	int ret;
	uint8_t datas[] = {0x20, 0x00, 0x00, 0x00};

	ret = i2c_write(i2c_dev, datas, 2, SPD2010_CHIP_ADDR);
	if (ret) {
		return ret;
	}

	ret = i2c_read(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		return ret;
	}

	status->status_l.pt_exist = datas[0] & 0x01;
	status->status_l.gesture = datas[0] & 0x02;
	status->status_h.tic_in_busy = (datas[1] & 0x80) >> 7;
	status->status_h.tic_in_bios = (datas[1] & 0x40) >> 6;
	status->status_h.tic_in_cpu = (datas[1] & 0x20) >> 5;
	status->status_h.tint_low = (datas[1] & 0x10) >> 4;
	status->status_h.cpu_run = (datas[1] & 0x08) >> 3;
	status->status_l.aux = (datas[0] & 0x08);

	status->readlen = (datas[3] << 8 | datas[2]);
	LOG_DBG("pt_exist:%d", status->status_l.pt_exist);
	LOG_DBG("gesture:%d", status->status_l.gesture);
	LOG_DBG("tic_in_busy:%d", status->status_h.tic_in_busy);
	LOG_DBG("tic_in_bios:%d", status->status_h.tic_in_bios);
	LOG_DBG("tic_in_cpu:%d", status->status_h.tic_in_cpu);
	LOG_DBG("tint_low:%d", status->status_h.tint_low);
	LOG_DBG("cpu_run:%d", status->status_h.cpu_run);
	LOG_DBG("aux:%d", status->status_l.aux);
	LOG_DBG("readlen:%d", status->readlen);
	LOG_DBG("------------");

	return ret;
}

static int spd2010_tp_hdp_read(const struct device *i2c_dev, struct spd2010_touch *touch,
			       struct spd2010_status *status)
{
	uint8_t datas[4 + 10 * 6] = {0};
	uint8_t check_id;
	uint8_t offset;
	uint8_t i;
	int ret;

	datas[0] = 0x00;
	datas[1] = 0x03;

	ret = i2c_write(i2c_dev, datas, 2, SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
		return ret;
	}

	ret = i2c_read(i2c_dev, datas, status->readlen, SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
		return ret;
	}

	check_id = datas[4];

	if (check_id <= 0x0A && status->status_l.pt_exist) {
		touch->touch_num = (status->readlen - 4) / 6;
		touch->gesture = 0x00;

		for (i = 0; i < touch->touch_num; i++) {
			offset = i * 6;
			touch->reports[i].id = datas[4 + offset];
			touch->reports[i].x =
				(((datas[7 + offset] & 0xF0) << 4) | datas[5 + offset]);
			touch->reports[i].y =
				(((datas[7 + offset] & 0x0F) << 8) | datas[6 + offset]);
			touch->reports[i].weight = datas[8 + offset];
		}

		/* for slide gesture recognize */
		if ((touch->reports[0].weight != 0) && (touch->down != 1)) {
			touch->down = 1;
			touch->up = 0;
			touch->down_x = touch->reports[0].x;
			touch->down_y = touch->reports[0].y;
		} else if ((touch->reports[0].weight == 0) && (touch->down == 1)) {
			touch->up = 1;
			touch->down = 0;
			touch->up_x = touch->reports[0].x;
			touch->up_y = touch->reports[0].y;
		}
	} else if (check_id == 0xF6 && status->status_l.gesture) {
		touch->touch_num = 0x00;
		touch->up = 0;
		touch->down = 0;
		touch->gesture = datas[6] & 0x07;
		LOG_INF("gesture : 0x%02x\n", touch->gesture);
	} else {
		touch->touch_num = 0x00;
		touch->gesture = 0x00;
	}

	return 0;
}

static int sdp2010_hdp_status_read(const struct device *i2c_dev,
				   struct spd2010_hdp_status *hdp_status)
{
	uint8_t datas[8] = {0};
	int ret;

	datas[0] = 0xFC;
	datas[1] = 0x02;

	ret = i2c_write(i2c_dev, datas, 2, SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
		return ret;
	}

	ret = i2c_read(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
		return ret;
	}

	hdp_status->sta = datas[5];
	hdp_status->next_packet_len = (datas[2] | datas[3] << 8);

	return ret;
}

static int spd2010_set_tp_point_mode(const struct device *i2c_dev)
{
	uint8_t datas[] = {0x50, 0x00, 0x00, 0x00};
	int ret;

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
	}

	return ret;
}

static int spd20120_tp_start(const struct device *i2c_dev)
{
	uint8_t datas[] = {0x46, 0x00, 0x00, 0x00};
	int ret;

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp start failed.\n");
	}

	return ret;
}

static int spd20120_tp_cpu_start(const struct device *i2c_dev)
{
	uint8_t datas[] = {0x04, 0x00, 0x01, 0x00};
	int ret;

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
	}

	return ret;
}

static int spd2010_tp_clear_int(const struct device *i2c_dev)
{
	uint8_t datas[] = {0x02, 0x00, 0x01, 0x00};
	int ret;

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("clear int failed.\n");
	}

	return ret;
}

static int spd2010_reset(const struct device *dev)
{
	int ret = -1;
	struct spd2010_config *config = (struct spd2010_config *)(dev->config);

	if (device_is_ready(config->reset.port)) {
		ret = gpio_pin_configure_dt(&config->reset, GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Couldn't configure reset pin");
			return ret;
		}

		k_sleep(K_MSEC(50));
		gpio_pin_set_dt(&config->reset, 1);
		k_sleep(K_MSEC(50));
		gpio_pin_set_dt(&config->reset, 0);
	} else {
		LOG_WRN("reset pin is not ready!");
	}

	return ret;
}

static int32_t spd2010_chip_init(const struct device *dev)
{
	/* nothing to do */
	return 0;
}

static void spd2010_dev_isr_handler(const struct device *dev, struct gpio_callback *cb,
				    uint32_t pins)
{
	struct spd2010_data *data = CONTAINER_OF(cb, struct spd2010_data, int_gpio_cb);
	k_work_reschedule(&data->work, K_NO_WAIT);
}

static void spd2010_dev_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct spd2010_data *dev_data = CONTAINER_OF(dwork, struct spd2010_data, work);
	const struct spd2010_config *config = (struct spd2010_config *)dev_data->device->config;
	int ret;
	struct spd2010_status status = {0};
	const struct device *i2c_dev = config->bus.bus;
	struct spd2010_hdp_status hdp_staus = {0};
	struct spd2010_touch touch = {0};

	ret = spd2010_tp_status_read(i2c_dev, &status);
	if (ret) {
		LOG_ERR("read tp status failed, ret=%d", ret);
		goto _work_handler_done;
	}

	if (status.status_h.tic_in_bios) {
		if (status.status_h.tic_in_busy) {
			/* not ready, retry, interrupt is not cleared */
			goto _work_handler_done;
		} else {
			spd2010_tp_clear_int(i2c_dev);
			spd20120_tp_cpu_start(i2c_dev);
		}
	} else if (status.status_h.tic_in_cpu) {
		if (status.status_h.cpu_run) {
			spd2010_set_tp_point_mode(i2c_dev);
			spd20120_tp_start(i2c_dev);
			spd2010_tp_clear_int(i2c_dev);
		}
	} else if (status.status_h.cpu_run && (status.readlen == 0)) {
		spd2010_tp_clear_int(i2c_dev);
	} else if (status.status_l.pt_exist || status.status_l.gesture) {
		ret = spd2010_tp_hdp_read(i2c_dev, &touch, &status);
		if (ret) {
			LOG_ERR("tp hdp read failed, ret:%d", ret);
			goto _work_handler_done;
		}

		if (dev_data->cb) {
			/* spd2010 supports multi-touch and pressing pressure, but zephyr does not
			 * support it yet, only one point is reported here  */
			if (touch.touch_num) {
				dev_data->cb(dev_data->device, touch.reports[0].x,
					     touch.reports[0].y, touch.reports[0].weight > 20);
				LOG_INF("x:%d, y:%d, w:%d, press:%d", touch.reports[0].x,
					touch.reports[0].y, touch.reports[0].weight,
					touch.reports[0].weight > 20);
			}
		}

		do {
			ret = sdp2010_hdp_status_read(i2c_dev, &hdp_staus);
			if (ret) {
				LOG_ERR("tp hdp status read failed, ret:%d", ret);
				goto _work_handler_done;
			}

			if (hdp_staus.sta == 0x82) {
				spd2010_tp_clear_int(i2c_dev);

			} else if (hdp_staus.sta == 0x00) {
				spd2010_hdp_remain_data_read(i2c_dev, hdp_staus.next_packet_len);
			}
		} while (hdp_staus.sta == 0x00);

	} else if (status.status_h.cpu_run && status.status_l.aux) {
		spd2010_tp_clear_int(i2c_dev);
	}

_work_handler_done:
	if (!device_is_ready(config->int_gpio.port)) {
		k_work_reschedule(&dev_data->work, K_MSEC(CONFIG_KSCAN_SPD2010_DELAY_TIME_MS));
	}
}

static int kscan_driv_config(const struct device *dev, kscan_callback_t callback)
{
	if (callback == NULL) {
		return -1;
	}

	struct spd2010_data *data = (struct spd2010_data *)dev->data;
	data->cb = callback;

	return 0;
}

static int ksacn_driv_enable(const struct device *dev)
{
	struct spd2010_data *data = (struct spd2010_data *)dev->data;
	struct spd2010_config *config = (struct spd2010_config *)dev->config;

	if (device_is_ready(config->int_gpio.port)) {
		gpio_add_callback(config->int_gpio.port, &data->int_gpio_cb);
	} else {
		LOG_INF("ksacn_driv_enable");
		k_work_reschedule(&data->work, K_MSEC(CONFIG_KSCAN_SPD2010_DELAY_TIME_MS));
	}

	return 0;
}

static int kscan_driv_disable(const struct device *dev)
{
	struct spd2010_data *data = (struct spd2010_data *)dev->data;
	struct spd2010_config *config = (struct spd2010_config *)dev->config;

	if (device_is_ready(config->int_gpio.port)) {
		gpio_remove_callback(config->int_gpio.port, &data->int_gpio_cb);
	} else {
		k_work_cancel_delayable(&data->work);
	}

	return 0;
}

static const struct kscan_driver_api spd2010_driver_api = {
	.config = kscan_driv_config,
	.enable_callback = ksacn_driv_enable,
	.disable_callback = kscan_driv_disable,
};

static int spd2010_dev_init(const struct device *dev)
{
	int32_t r;
	struct spd2010_data *data = (struct spd2010_data *)(dev->data);
	const struct spd2010_config *config = (struct spd2010_config *)(dev->config);

	data->device = dev;

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("[SPD2010]i2c device is not ready.\n");
		return -1;
	}

	r = i2c_configure(config->bus.bus, I2C_SPEED_SET(I2C_SPEED_FAST) | I2C_MODE_MASTER);
	if (r < 0) {
		LOG_ERR("[SPD2010] i2c config failed\n");
		return -1;
	}

	k_work_init_delayable(&data->work, spd2010_dev_work_handler);

	if (device_is_ready(config->int_gpio.port)) {
		r = gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT | GPIO_PULL_UP);
		if (r) {
			LOG_ERR("[SPD2010]Could not configure interrupt GPIO pin");
			return r;
		}

		gpio_init_callback(&data->int_gpio_cb, spd2010_dev_isr_handler,
				   BIT(config->int_gpio.pin));
		r = gpio_add_callback(config->int_gpio.port, &data->int_gpio_cb);

		if (r) {
			LOG_ERR("int gpio add callback failed! %d", r);
		}

		r = gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);

		if (r) {
			LOG_ERR("Te GPIO add interrupt configure failed, ret:%d", r);
		}
	} else {
		LOG_ERR("int gpio is not ready!");
		
	}

	spd2010_reset(dev);

	r = spd2010_chip_init(dev);

	if (r < 0) {
		LOG_ERR("[SPD2010]Could not configure interrupt GPIO interrupt.");
		return r;
	}

	return 0;
}

#define SPD2010_INIT(index)                                                                        \
	static const struct spd2010_config spd2010_config##index = {                               \
		.reset = GPIO_DT_SPEC_INST_GET_OR(index, reset_gpios, {}),                         \
		.int_gpio = GPIO_DT_SPEC_INST_GET_OR(index, int_gpios, {}),                        \
		.bus = I2C_DT_SPEC_INST_GET(index),                                                \
	};                                                                                         \
	static struct spd2010_data spd2010_data##index = {};                                       \
	DEVICE_DT_INST_DEFINE(index, spd2010_dev_init, NULL, &spd2010_data##index,                 \
			      &spd2010_config##index, POST_KERNEL, CONFIG_KSCAN_INIT_PRIORITY,     \
			      &spd2010_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPD2010_INIT);
