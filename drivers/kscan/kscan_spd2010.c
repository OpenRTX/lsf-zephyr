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

#define DT_DRV_COMPAT solomon_spd2010

LOG_MODULE_REGISTER(spd2010, CONFIG_KSCAN_LOG_LEVEL);

#define SPD2010_ID_REG_1	 0x0A
#define SPD2010_ID_REG_2	 0xF5
#define SPD2010_ID_REG_3	 0xE7
#define SPD2010_ID_REG_4	 0xE8
#define SPD2010_POINT_REG	 0x01
#define SPD2010_SLEEP_REG	 0xA5
#define SPD2010_SLEEP_VAL	 0x01
#define SPD2010_SELF_UID_REG	 0x16
#define SPD2010_GP_UID_VAL	 0x57FACE
#define SPD2010_GP_UID_DATA_SIZE 3
#define SPD2010_POINT_DATA_SIZE	 6

#define SPD2010_CHIP_ADDR 0x53

#ifndef NULL
#define NULL 0
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

typedef enum {
	SPD2010_TOUCH_EVENT_DOWN,
	SPD2010_TOUCH_EVENT_UP,
	SPD2010_TOUCH_EVENT_CONTACT_OR_MOVE,
	SPD2010_TOUCH_EVENT_NONE,
} spd2010_touch_event_t;

typedef struct spd2010_chip_data {
	uint8_t gesture_code;
	uint8_t point_number;
	spd2010_touch_event_t evnet;
	uint16_t x;
	uint16_t y;
	uint8_t touch_id;
} spd2010_chip_data_t;

typedef struct gpio_info {
	const char *port;
	gpio_pin_t pin;
	gpio_dt_flags_t flag;
} gpio_info_t;

typedef struct spd2010_dev_data {
	struct k_work work;
	kscan_callback_t cb;
	const struct device *dev;
	const struct device *i2c_dev;
	struct gpio_callback int_gpio_cb;
	gpio_info_t int_gpio;
	gpio_info_t reset_gpio;
} spd2010_dev_data_t;

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
	uint8_t datas[] = {0x20, 0x00, 0x00, 0x00};
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

	status->status_l.pt_exist = datas[0] & 0x01;
	status->status_l.gesture = datas[0] & 0x02;
	status->status_h.tic_in_busy = (datas[1] & 0x80) >> 7;
	status->status_h.tic_in_bios = (datas[1] & 0x40) >> 6;
	status->status_h.tic_in_cpu = (datas[1] & 0x20) >> 5;
	status->status_h.tint_low = (datas[1] & 0x10) >> 4;
	status->status_h.cpu_run = (datas[1] & 0x08) >> 3;
	status->status_l.aux = (datas[0] & 0x08);

	status->readlen = (datas[3] << 8 | datas[2]);

	return ret;
}

static int spd2010_tp_hdp_read(const struct device *i2c_dev, struct spd2010_touch *touch,
			       struct spd2010_status *status)
{
	uint8_t datas[4 + 10 * 6] = {0};
	uint8_t check_id;
	uint8_t offset;
	uint8_t i;

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
		if ((touch->reports[0].weight != 0) && (touch.down != 1)) {
			touch.down = 1;
			touch.up = 0;
			touch.down_x = touch->reports[0].x;
			touch.down_y = touch->reports[0].y;
		} else if ((touch->reports[0].weight == 0) && (touch.down == 1)) {
			touch.up = 1;
			touch.down = 0;
			touch.up_x = touch->reports[0].x;
			touch.up_y = touch->reports[0].y;
		}

		for (uint8_t finger_num = 0; finger_num < touch.touch_num; finger_num++) {
			LOG_INF("ID[%d], X[%d], Y[%d], Weight[%d]\n", touch->reports[finger_num].id,
				touch->reports[finger_num].x, touch->reports[finger_num].y,
				touch->reports[finger_num].weight);
		}
	} else if (check_id == 0xF6 && status->status_l.gesture) {
		touch.touch_num = 0x00;
		touch.up = 0;
		touch.down = 0;
		touch.gesture = sample_data[6] & 0x07;
		LOG_INF("gesture : 0x%02x\n", touch.gesture);
	} else {
		touch.touch_num = 0x00;
		touch.gesture = 0x00;
	}
}

static int sdp2010_hdp_status_read(const struct device *i2c_dev,
				   struct spd2010_hdp_status *hdp_status)
{
	uint8_t datas[8] = {0};

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

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
	}

	return ret;
}

static int spd20120_tp_start(const struct device *i2c_dev)
{
	uint8_t datas[] = {0x46, 0x00, 0x00, 0x00};

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
	}

	return ret;
}

static int spd20120_tp_cpu_start(const struct device *i2c_dev)
{
	uint8_t datas[] = {0x04, 0x00, 0x01, 0x00};

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
	}

	return ret;
}

static int spd2010_tp_clear_int(const struct device *i2c_dev)
{
	uint8_t datas[] = {0x02, 0x00, 0x01, 0x00};

	ret = i2c_write(i2c_dev, datas, sizeof(datas), SPD2010_CHIP_ADDR);
	if (ret) {
		LOG_ERR("set tp point mode faild.\n");
	}

	return ret;
}

static int32_t spd2010_chip_init(const struct device *i2c_dev)
{

	return 0;
}

static void spd2010_dev_isr_handler(const struct device *dev, struct gpio_callback *cb,
				    uint32_t pins)
{
	spd2010_dev_data_t *data = CONTAINER_OF(cb, spd2010_dev_data_t, int_gpio_cb);
	k_work_submit(&data->work);
}

static void spd2010_dev_work_handler(struct k_work *work)
{
	struct spd2010_dev_data *dev_data = CONTAINER_OF(work, struct spd2010_dev_data, work);
	int ret;
	struct spd2010_status status = {0};

	ret = spd2010_tp_status_read(dev_data->i2c_dev, &status);
	if (ret) {
		LOG_ERR("read tp status failed, ret=%d", ret);
		return;
	}

	if (status.status_h.tic_in_bios) {
		spd2010_tp_clear_int(dev_data->i2c_dev);
		spd20120_tp_cpu_start(dev_data->i2c_dev);
	} else if (status.status_h.tic_in_cpu) {
		spd2010_set_tp_point_mode(dev_data->i2c_dev);
		spd20120_tp_start(dev_data->i2c_dev);
		spd2010_tp_clear_int(dev_data->i2c_dev);
	} else if (status.status_h.cpu_run && (status.readlen == 0)) {
		spd2010_tp_clear_int(dev_data->i2c_dev);
	} else if (status.status_l.pt_exist || status.status_l.gesture) {
		struct spd2010_touch touch = {0};
		ret = spd2010_tp_hdp_read(dev_data->i2c_dev, &touch, &status);
		if (ret) {
		}

		struct spd2010_hdp_status hdp_staus = {0};
		do {
			ret = sdp2010_hdp_status_read(dev_data->i2c_dev, &hdp_staus);
			if (ret) {
			}

			if (hdp_staus.sta == 0x82) {
				spd2010_tp_clear_int(dev_data->i2c_dev);
			} else if (hdp_staus.sta == 0x00) {
				spd2010_hdp_remain_data_read(dev_data->i2c_dev,
							     hdp_staus.next_packet_len);
			}
		} while (hdp_staus.sta == 0x00);

	} else if (status.status_h.cpu_run && status.status_l.aux) {
		spd2010_tp_clear_int(dev_data->i2c_dev);
	}
}

static int kscan_driv_config(const struct device *dev, kscan_callback_t callback)
{
	if (callback == NULL) {
		return -1;
	}

	spd2010_dev_data_t *data = (spd2010_dev_data_t *)dev->data;
	data->cb = callback;

	return 0;
}

static int ksacn_driv_enable(const struct device *dev)
{
	spd2010_dev_data_t *data = (spd2010_dev_data_t *)dev->data;

	gpio_add_callback(device_get_binding(data->int_gpio.port), &data->int_gpio_cb);

	return 0;
}

static int kscan_driv_disable(const struct device *dev)
{
	spd2010_dev_data_t *data = (spd2010_dev_data_t *)dev->data;

	gpio_remove_callback(device_get_binding(data->int_gpio.port), &data->int_gpio_cb);

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
	spd2010_dev_data_t *data = (spd2010_dev_data_t *)(dev->data);
	const struct device *i2c_dev = data->i2c_dev;
	const struct device *gpio = device_get_binding(data->int_gpio.port);

	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("[SPD2010]i2c device is not ready.\n");
		return -1;
	}
	r = i2c_configure(i2c_dev, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER);
	if (r < 0) {
		LOG_ERR("[SPD2010] i2c config failed\n");
		return -1;
	}

	if (gpio == NULL) {
		LOG_ERR("[SPD2010]interrupt gpio is not binding.\n");
		return -EIO;
	}
	r = gpio_pin_configure(gpio, data->int_gpio.pin, GPIO_INPUT | GPIO_PULL_UP);
	if (r < 0) {
		LOG_ERR("[SPD2010]Could not configure interrupt GPIO pin");
		return r;
	}
	r = gpio_pin_interrupt_configure(gpio, data->int_gpio.pin, GPIO_INT_EDGE_TO_INACTIVE);
	if (r < 0) {
		LOG_ERR("[SPD2010]Could not configure interrupt GPIO interrupt.");
		return r;
	}

	r = spd2010_chip_init(i2c_dev);
	if (r < 0) {
		LOG_ERR("[SPD2010]Could not configure interrupt GPIO interrupt.");
		return r;
	}

	k_work_init(&(data->work), spd2010_dev_work_handler);

	gpio_init_callback(&data->int_gpio_cb, spd2010_dev_isr_handler, BIT(data->int_gpio.pin));
	gpio_add_callback(gpio, &data->int_gpio_cb);

	return 0;
}

#define SPD2010_DEFINE_GPIO(index, gpio_pha)                                                       \
	{                                                                                          \
		.port = DT_INST_GPIO_LABEL(index, gpio_pha),                                       \
		.pin = DT_INST_GPIO_PIN(index, gpio_pha),                                          \
		.flag = DT_INST_GPIO_FLAGS(index, gpio_pha),                                       \
	}

#define SPD2010_DEFINE_DATA(index)                                                                 \
	static struct spd2010_dev_data spd2010_dev_data_##index = {                                \
		.i2c_dev = DEVICE_DT_GET(DT_INST_BUS(index)),                                      \
		.int_gpio = SPD2010_DEFINE_GPIO(index, int_gpios),                                 \
		.reset_gpio = SPD2010_DEFINE_GPIO(index, reset_gpios),                             \
	}

#define SPD2010_INIT(index)                                                                        \
	SPD2010_DEFINE_DATA(index);                                                                \
	DEVICE_DT_INST_DEFINE(index, spd2010_dev_init, NULL, &spd2010_dev_data_##index, NULL,      \
			      POST_KERNEL, CONFIG_KSCAN_INIT_PRIORITY, &spd2010_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPD2010_INIT)
