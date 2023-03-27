/*
 * Copyright (c) 2020 Teslabs Engineering S.L.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT solomon_spd2010_display

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(display_spd2010, CONFIG_DISPLAY_LOG_LEVEL);

#define SPD2010_SOFTRESET 0x01
#define SPD2010_ID	  0x04
#define SPD2010_RDISPMODE 0x09
#define SPD2010_RDDPM	  0x0A
#define SPD2010_RDDMADCTL 0x0B
#define SPD2010_RDPIX	  0x0C
#define SPD2010_RDDIM	  0x0D
#define SPD2010_RDDSM	  0x0E
#define SPD2010_RDDSDR	  0x0F
#define SPD2010_SLPIN	  0x10
#define SPD2010_SLPOUT	  0x11
#define SPD2010_NORON	  0x13
#define SPD2010_INVOFF	  0x20
#define SPD2010_INVON	  0x21
#define SPD2010_DISPOFF	  0x28
#define SPD2010_DISPON	  0x29
#define SPD2010_SETCOL	  0x2A
#define SPD2010_SETPAGE	  0x2B
#define SPD2010_WRMEMST	  0x2C
#define SPD2010_TEOFF	  0x34
#define SPD2010_TEON	  0x35
#define SPD2010_MADCTR	  0x36
#define SPD2010_IDMOFF	  0x38
#define SPD2010_IDMON	  0x39
#define SPD2010_SETPIXEL  0x3A
#define SPD2010_WRMEMCONT 0x3C
#define SPD2010_TESS	  0x44
#define SPD2010_RSS	  0x45
#define SPD2010_SETBR	  0x51
#define SPD2010_SETMODE	  0x53
#define SPD2010_RDMODE	  0x54
#define SPD2010_SETPWR	  0x55
#define SPD2010_RDPWR	  0x56
#define SPD2010_SETMINBR  0x5E
#define SPD2010_RDMINBR	  0x5F
#define SPD2010_RDDDBST	  0xA1
#define SPD2010_RDDDBCON  0xA8

/** Command/data GPIO level for commands. */
#define SPD2010_CMD 1U

/** Command/data GPIO level for data. */
#define SPD2010_DATA 0U

#define SPD2010_PIXEL_SIZE 3
struct spd2010_config {
	const struct spi_dt_spec bus;
	const struct gpio_dt_spec cmd_data;
	const struct gpio_dt_spec reset;
	const struct gpio_dt_spec te;
	uint8_t pixel_format;
	uint16_t height;
	uint16_t width;
	bool inversion_on;
};

struct spd2010_data {
	struct gpio_callback te_callback;
	struct k_sem te_sem;
	uint16_t x_offset;
	uint16_t y_offset;
	uint32_t pixel_format;
	uint32_t orientation;
};

static int spd2010_transmit(const struct device *dev, uint8_t cmd, const void *tx_data,
			    size_t tx_len)
{
	const struct spd2010_config *config = dev->config;

	int r;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1U};

	/* send command */
	tx_buf.buf = &cmd;
	tx_buf.len = 1U;

	gpio_pin_set_dt(&config->cmd_data, SPD2010_CMD);
	r = spi_write_dt(&config->bus, &tx_bufs);
	if (r < 0) {
		return r;
	}

	/* send data (if any) */
	if (tx_len != 0) {
		tx_buf.buf = (void *)tx_data;
		tx_buf.len = tx_len;

		gpio_pin_set_dt(&config->cmd_data, SPD2010_DATA);
		r = spi_write_dt(&config->bus, &tx_bufs);
		if (r < 0) {
			return r;
		}
	}

	return 0;
}

static void spd2010_reset_display(const struct device *dev)
{
	LOG_DBG("Resetting display");

	const struct spd2010_config *config = dev->config;
	if (config->reset.port != NULL) {
		k_sleep(K_MSEC(50));
		gpio_pin_set_dt(&config->reset, 1);
		k_sleep(K_MSEC(50));
		gpio_pin_set_dt(&config->reset, 0);
		k_sleep(K_MSEC(100));
	} else {
		spd2010_transmit(dev, SPD2010_SOFTRESET, NULL, 0);
		k_sleep(K_MSEC(100));
	}
}

static int spd2010_blanking_on(const struct device *dev)
{
	return spd2010_transmit(dev, SPD2010_DISPOFF, NULL, 0);
}

static int spd2010_blanking_off(const struct device *dev)
{
	return spd2010_transmit(dev, SPD2010_DISPON, NULL, 0);
}

static void spd2010_set_mem_area(const struct device *dev, const uint16_t x, const uint16_t y,
				 const uint16_t w, const uint16_t h)
{
	struct spd2010_data *data = dev->data;
	uint16_t spi_data[2];

	uint16_t ram_x = x + data->x_offset;
	uint16_t ram_y = y + data->y_offset;

	spi_data[0] = sys_cpu_to_be16(ram_x);
	spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	spd2010_transmit(dev, SPD2010_SETCOL, (uint8_t *)&spi_data[0], 4);

	spi_data[0] = sys_cpu_to_be16(ram_y);
	spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	spd2010_transmit(dev, SPD2010_SETPAGE, (uint8_t *)&spi_data[0], 4);
}

static int spd2010_write(const struct device *dev, const uint16_t x, const uint16_t y,
			 const struct display_buffer_descriptor *desc, const void *buf)
{
	const uint8_t *write_data_start = (uint8_t *)buf;
	uint16_t write_cnt;
	uint16_t nbr_of_writes;
	uint16_t write_h;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
	__ASSERT((desc->pitch * SPD2010_PIXEL_SIZE * desc->height) <= desc->buf_size,
		 "Input buffer to small");

	spd2010_set_mem_area(dev, x, y, desc->width, desc->height);

	if (desc->pitch > desc->width) {
		write_h = 1U;
		nbr_of_writes = desc->height;
	} else {
		write_h = desc->height;
		nbr_of_writes = 1U;
	}

	spd2010_transmit(dev, SPD2010_WRMEMST, (void *)write_data_start,
			 desc->width * SPD2010_PIXEL_SIZE * write_h);

	write_data_start += (desc->pitch * SPD2010_PIXEL_SIZE);
	for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
		spd2010_transmit(dev, SPD2010_WRMEMCONT, (void *)write_data_start,
				 desc->width * SPD2010_PIXEL_SIZE * write_h);
		write_data_start += (desc->pitch * SPD2010_PIXEL_SIZE);
	}

	return 0;
}

static int spd2010_read(const struct device *dev, const uint16_t x, const uint16_t y,
			const struct display_buffer_descriptor *desc, void *buf)
{
	return -ENOTSUP;
}

static void *spd2010_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int spd2010_set_brightness(const struct device *dev, const uint8_t brightness)
{
	return -ENOTSUP;
}

static int spd2010_set_contrast(const struct device *dev, const uint8_t contrast)
{
	return -ENOTSUP;
}

static void spd2010_get_capabilities(const struct device *dev,
				     struct display_capabilities *capabilities)
{
	struct spd2010_data *data = dev->data;
	const struct spd2010_config *config = dev->config;

	memset(capabilities, 0, sizeof(struct display_capabilities));

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_888;
	capabilities->current_pixel_format = data->pixel_format;

	if (data->orientation == DISPLAY_ORIENTATION_NORMAL ||
	    data->orientation == DISPLAY_ORIENTATION_ROTATED_180) {
		capabilities->x_resolution = config->width;
		capabilities->y_resolution = config->height;
	} else {
		capabilities->x_resolution = config->height;
		capabilities->y_resolution = config->width;
	}

	capabilities->current_orientation = data->orientation;
}

static int spd2010_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pixel_format)
{
	uint8_t data;

	switch (pixel_format) {
	case PIXEL_FORMAT_RGB_888:
		data = 7;
		break;
	case PIXEL_FORMAT_RGB_565:
	case PIXEL_FORMAT_BGR_565:
	default:
		return -ENOTSUP;
		break;
	}

	return spd2010_transmit(dev, SPD2010_SETPIXEL, &data, 1);
}

static int spd2010_set_orientation(const struct device *dev,
				   const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}

	LOG_ERR("Changing display orientation not implemented");

	return -ENOTSUP;
}

static void spd2010_te_gpio_callback_handler(const struct device *port, struct gpio_callback *cb,
					     gpio_port_pins_t pins)
{
	LOG_INF("spd2010_te_gpio_callback_handler");
	struct spd2010_data *data = CONTAINER_OF(cb, struct spd2010_data, te_callback);
	k_sem_give(&data->te_sem);
}

typedef struct {
	uint8_t cmd;
	uint8_t data_len;
	uint8_t data[4];
} spd2010_init_cmd_item_t;

const static spd2010_init_cmd_item_t items[] = {
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20 ,0x10 ,0x10},},
	{.cmd = 0x62, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x61, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x5C, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x58, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x55, .data_len = 1,  .data = {0x55},},
	{.cmd = 0x54, .data_len = 1,  .data = {0x44},},
	{.cmd = 0x51, .data_len = 1,  .data = {0x11},},
	{.cmd = 0x4B, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x4A, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x49, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x47, .data_len = 1,  .data = {0x77},},
	{.cmd = 0x46, .data_len = 1,  .data = {0x66},},
	{.cmd = 0x45, .data_len = 1,  .data = {0x55},},
	{.cmd = 0x44, .data_len = 1,  .data = {0x44},},
	{.cmd = 0x43, .data_len = 1,  .data = {0x33},},
	{.cmd = 0x42, .data_len = 1,  .data = {0x22},},
	{.cmd = 0x41, .data_len = 1,  .data = {0x11},},
	{.cmd = 0x37, .data_len = 1,  .data = {0x12},},
	{.cmd = 0x36, .data_len = 1,  .data = {0x12},},
	{.cmd = 0x35, .data_len = 1,  .data = {0x11},},
	{.cmd = 0x34, .data_len = 1,  .data = {0x11},},
	{.cmd = 0x33, .data_len = 1,  .data = {0x20},},
	{.cmd = 0x32, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x31, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x30, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x27, .data_len = 1,  .data = {0x12},},
	{.cmd = 0x26, .data_len = 1,  .data = {0x12},},
	{.cmd = 0x25, .data_len = 1,  .data = {0x11},},
	{.cmd = 0x24, .data_len = 1,  .data = {0x11},},
	{.cmd = 0x23, .data_len = 1,  .data = {0x20},},
	{.cmd = 0x22, .data_len = 1,  .data = {0x72},},
	{.cmd = 0x21, .data_len = 1,  .data = {0x82},},
	{.cmd = 0x20, .data_len = 1,  .data = {0x81},},
	{.cmd = 0x1B, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x1A, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x16, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x15, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x11, .data_len = 1,  .data = {0x12},},
	{.cmd = 0x10, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x0C, .data_len = 1,  .data = {0x12},},
	{.cmd = 0x0B, .data_len = 1,  .data = {0x43},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x11},},
	{.cmd = 0x6A, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x69, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x68, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x67, .data_len = 1,  .data = {0x04},},
	{.cmd = 0x66, .data_len = 1,  .data = {0x38},},
	{.cmd = 0x65, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x64, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x63, .data_len = 1,  .data = {0x04},},
	{.cmd = 0x62, .data_len = 1,  .data = {0x38},},
	{.cmd = 0x61, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x60, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x55, .data_len = 1,  .data = {0x06},},
	{.cmd = 0x50, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x30, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x1E, .data_len = 1,  .data = {0x88},},
	{.cmd = 0x1D, .data_len = 1,  .data = {0x88},},
	{.cmd = 0x1C, .data_len = 1,  .data = {0x88},},
	{.cmd = 0x16, .data_len = 1,  .data = {0x99},},
	{.cmd = 0x15, .data_len = 1,  .data = {0x99},},
	{.cmd = 0x14, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x13, .data_len = 1,  .data = {0xf0},},
	{.cmd = 0x0c, .data_len = 1,  .data = {0xde},},
	{.cmd = 0x0B, .data_len = 1,  .data = {0xde},},
	{.cmd = 0x0A, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x09, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x08, .data_len = 1,  .data = {0x60},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x12},},
	{.cmd = 0x2E, .data_len = 1,  .data = {0x1e},},
	{.cmd = 0x2D, .data_len = 1,  .data = {0x28},},
	{.cmd = 0x2C, .data_len = 1,  .data = {0x26},},
	{.cmd = 0x2B, .data_len = 1,  .data = {0x1e},},
	{.cmd = 0x2A, .data_len = 1,  .data = {0x28},},
	{.cmd = 0x21, .data_len = 1,  .data = {0x52},},   //VCOM
	{.cmd = 0x1F, .data_len = 1,  .data = {0xDC},},
	{.cmd = 0x15, .data_len = 1,  .data = {0x0F},},
	{.cmd = 0x12, .data_len = 1,  .data = {0x89},},
	{.cmd = 0x10, .data_len = 1,  .data = {0x0F},},
	{.cmd = 0x0D, .data_len = 1,  .data = {0x66},},
	{.cmd = 0x06, .data_len = 1,  .data = {0x06},},
	{.cmd = 0x00, .data_len = 1,  .data = {0x99},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x15},},
	{.cmd = 0x2F, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x2E, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x2D, .data_len = 1,  .data = {0x35},},
	{.cmd = 0x2C, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x2B, .data_len = 1,  .data = {0x32},},
	{.cmd = 0x2A, .data_len = 1,  .data = {0x33},},
	{.cmd = 0x29, .data_len = 1,  .data = {0x19},},
	{.cmd = 0x28, .data_len = 1,  .data = {0x17},},
	{.cmd = 0x27, .data_len = 1,  .data = {0x1D},},
	{.cmd = 0x26, .data_len = 1,  .data = {0x1B},},
	{.cmd = 0x25, .data_len = 1,  .data = {0x09},},
	{.cmd = 0x24, .data_len = 1,  .data = {0x05},},
	{.cmd = 0x23, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x22, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x21, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x20, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0F, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x0E, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x0D, .data_len = 1,  .data = {0x35},},
	{.cmd = 0x0C, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x0B, .data_len = 1,  .data = {0x32},},
	{.cmd = 0x0A, .data_len = 1,  .data = {0x33},},
	{.cmd = 0x09, .data_len = 1,  .data = {0x16},},
	{.cmd = 0x08, .data_len = 1,  .data = {0x18},},
	{.cmd = 0x07, .data_len = 1,  .data = {0x1A},},
	{.cmd = 0x06, .data_len = 1,  .data = {0x1C},},
	{.cmd = 0x05, .data_len = 1,  .data = {0x04},},
	{.cmd = 0x04, .data_len = 1,  .data = {0x08},},
	{.cmd = 0x03, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x02, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x01, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x00, .data_len = 1,  .data = {0x00},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x16},},
	{.cmd = 0x2F, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x2E, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x2D, .data_len = 1,  .data = {0x35},},
	{.cmd = 0x2C, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x2B, .data_len = 1,  .data = {0x32},},
	{.cmd = 0x2A, .data_len = 1,  .data = {0x33},},
	{.cmd = 0x29, .data_len = 1,  .data = {0x18},},
	{.cmd = 0x28, .data_len = 1,  .data = {0x16},},
	{.cmd = 0x27, .data_len = 1,  .data = {0x1C},},
	{.cmd = 0x26, .data_len = 1,  .data = {0x1A},},
	{.cmd = 0x25, .data_len = 1,  .data = {0x08},},
	{.cmd = 0x24, .data_len = 1,  .data = {0x04},},
	{.cmd = 0x23, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x22, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x21, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x20, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0F, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x0E, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x0D, .data_len = 1,  .data = {0x35},},
	{.cmd = 0x0C, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x0B, .data_len = 1,  .data = {0x32},},
	{.cmd = 0x0A, .data_len = 1,  .data = {0x33},},
	{.cmd = 0x09, .data_len = 1,  .data = {0x17},},
	{.cmd = 0x08, .data_len = 1,  .data = {0x19},},
	{.cmd = 0x07, .data_len = 1,  .data = {0x1B},},
	{.cmd = 0x06, .data_len = 1,  .data = {0x1D},},
	{.cmd = 0x05, .data_len = 1,  .data = {0x05},},
	{.cmd = 0x04, .data_len = 1,  .data = {0x09},},
	{.cmd = 0x03, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x02, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x01, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x00, .data_len = 1,  .data = {0x00},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x17},},
	{.cmd = 0x39, .data_len = 1,  .data = {0x3c},},
	{.cmd = 0x37, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x1F, .data_len = 1,  .data = {0x80},},
	{.cmd = 0x1A, .data_len = 1,  .data = {0x80},},
	{.cmd = 0x18, .data_len = 1,  .data = {0xA0},},
	{.cmd = 0x16, .data_len = 1,  .data = {0x12},},
	{.cmd = 0x14, .data_len = 1,  .data = {0xAA},},
	{.cmd = 0x11, .data_len = 1,  .data = {0xAA},},
	{.cmd = 0x10, .data_len = 1,  .data = {0x0E},},
	{.cmd = 0x0B, .data_len = 1,  .data = {0xC3},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x18},},
	{.cmd = 0x3A, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x1F, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x01, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x00, .data_len = 1,  .data = {0x1E},},
	{.cmd = 0xff, .data_len = 3,  .data = {0x20, 0x10, 0x2D},},
	{.cmd = 0x01, .data_len = 1,  .data = {0x3E},},
	{.cmd = 0x02, .data_len = 1,  .data = {0x00},},
	{.cmd = 0xff, .data_len = 3,  .data = {0x20, 0x10, 0x31},},
	{.cmd = 0x39, .data_len = 1,  .data = {0xf0},},
	{.cmd = 0x38, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x37, .data_len = 1,  .data = {0xe8},},
	{.cmd = 0x36, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x35, .data_len = 1,  .data = {0xCF},},
	{.cmd = 0x34, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x33, .data_len = 1,  .data = {0xBA},},
	{.cmd = 0x32, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x31, .data_len = 1,  .data = {0xA2},},
	{.cmd = 0x30, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x2f, .data_len = 1,  .data = {0x8E},},
	{.cmd = 0x2e, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x2d, .data_len = 1,  .data = {0x7e},},
	{.cmd = 0x2c, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x2b, .data_len = 1,  .data = {0x62},},
	{.cmd = 0x2a, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x29, .data_len = 1,  .data = {0x38},},
	{.cmd = 0x28, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x27, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x26, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x25, .data_len = 1,  .data = {0xd8},},
	{.cmd = 0x24, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x23, .data_len = 1,  .data = {0xa2},},
	{.cmd = 0x22, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x21, .data_len = 1,  .data = {0x78},},
	{.cmd = 0x20, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x1f, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x1e, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x1d, .data_len = 1,  .data = {0xfa},},
	{.cmd = 0x1c, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x1b, .data_len = 1,  .data = {0xbb},},
	{.cmd = 0x1a, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x19, .data_len = 1,  .data = {0x70},},
	{.cmd = 0x18, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x17, .data_len = 1,  .data = {0x3e},},
	{.cmd = 0x16, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x15, .data_len = 1,  .data = {0xf4},},
	{.cmd = 0x14, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x13, .data_len = 1,  .data = {0xd0},},
	{.cmd = 0x12, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x11, .data_len = 1,  .data = {0xa0},},
	{.cmd = 0x10, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0f, .data_len = 1,  .data = {0x90},},
	{.cmd = 0x0e, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0d, .data_len = 1,  .data = {0x7c},},
	{.cmd = 0x0c, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0b, .data_len = 1,  .data = {0x6a},},
	{.cmd = 0x0a, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x09, .data_len = 1,  .data = {0x58},},
	{.cmd = 0x08, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x07, .data_len = 1,  .data = {0x4c},},
	{.cmd = 0x06, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x05, .data_len = 1,  .data = {0x2f},},
	{.cmd = 0x04, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x03, .data_len = 1,  .data = {0x14},},
	{.cmd = 0x02, .data_len = 1,  .data = {0x00},},
	{.cmd = 0xff, .data_len = 3,  .data = {0x20, 0x10, 0x32},},
	{.cmd = 0x39, .data_len = 1,  .data = {0xf0},},
	{.cmd = 0x38, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x37, .data_len = 1,  .data = {0xe8},},
	{.cmd = 0x36, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x35, .data_len = 1,  .data = {0xCF},},
	{.cmd = 0x34, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x33, .data_len = 1,  .data = {0xBA},},
	{.cmd = 0x32, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x31, .data_len = 1,  .data = {0xA2},},
	{.cmd = 0x30, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x2f, .data_len = 1,  .data = {0x8E},},
	{.cmd = 0x2e, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x2d, .data_len = 1,  .data = {0x7E},},
	{.cmd = 0x2c, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x2b, .data_len = 1,  .data = {0x62},},
	{.cmd = 0x2a, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x29, .data_len = 1,  .data = {0x38},},
	{.cmd = 0x28, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x27, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x26, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x25, .data_len = 1,  .data = {0xd8},},
	{.cmd = 0x24, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x23, .data_len = 1,  .data = {0xa2},},
	{.cmd = 0x22, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x21, .data_len = 1,  .data = {0x78},},
	{.cmd = 0x20, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x1f, .data_len = 1,  .data = {0x34},},
	{.cmd = 0x1e, .data_len = 1,  .data = {0x02},},
	{.cmd = 0x1d, .data_len = 1,  .data = {0xfa},},
	{.cmd = 0x1c, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x1b, .data_len = 1,  .data = {0xbb},},
	{.cmd = 0x1a, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x19, .data_len = 1,  .data = {0x70},},
	{.cmd = 0x18, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x17, .data_len = 1,  .data = {0x3e},},
	{.cmd = 0x16, .data_len = 1,  .data = {0x01},},
	{.cmd = 0x15, .data_len = 1,  .data = {0xf4},},
	{.cmd = 0x14, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x13, .data_len = 1,  .data = {0xd0},},
	{.cmd = 0x12, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x11, .data_len = 1,  .data = {0xa0},},
	{.cmd = 0x10, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0f, .data_len = 1,  .data = {0x90},},
	{.cmd = 0x0e, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0d, .data_len = 1,  .data = {0x7c},},
	{.cmd = 0x0c, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x0b, .data_len = 1,  .data = {0x6a},},
	{.cmd = 0x0a, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x09, .data_len = 1,  .data = {0x58},},
	{.cmd = 0x08, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x07, .data_len = 1,  .data = {0x4c},},
	{.cmd = 0x06, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x05, .data_len = 1,  .data = {0x2F},},
	{.cmd = 0x04, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x03, .data_len = 1,  .data = {0x14},},
	{.cmd = 0x02, .data_len = 1,  .data = {0x00},},
	{.cmd = 0xff, .data_len = 3,  .data = {0x20, 0x10, 0x40},},
	{.cmd = 0x86, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x83, .data_len = 1,  .data = {0xC4},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x42},},
	{.cmd = 0x06, .data_len = 1,  .data = {0x03},},
	{.cmd = 0x05, .data_len = 1,  .data = {0x3D},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20, 0x10, 0x45},},
	{.cmd = 0x03, .data_len = 1,  .data = {0xB4},},
	{.cmd = 0x02, .data_len = 1,  .data = {0x00},},
	{.cmd = 0x01, .data_len = 1,  .data = {0xC6},},
#if CONFIG_SPD2010_TOUCH
	{.cmd = 0xFF, .data_len =3,  .data = { 0x20, 0x10, 0x50},},
	{.cmd = 0x08, .data_len =1,  .data = { 0x55,},},
	{.cmd = 0x05, .data_len =1,  .data = { 0x08,},},
	{.cmd = 0x01, .data_len =1,  .data = { 0xA6,},},
	{.cmd = 0x00, .data_len =1,  .data = { 0xA6,},},
#endif /* end of CONFIG_SPD2010_TOUCH */
	{.cmd = 0xFF, .data_len = 3, .data = {0x20, 0x10, 0xA0},},
	{.cmd = 0x08, .data_len = 1, .data = {0xdc},},
	{.cmd = 0xFF, .data_len = 3, .data = {0x20, 0x10, 0x00},},
	{.cmd = 0x2A, .data_len = 4, .data = {0x00, 0x00, 0x00, 0xB3},},
	{.cmd = 0x2B, .data_len = 4, .data = {0x00, 0x00, 0x01, 0xC5},},
	{.cmd = 0x3A, .data_len = 1, .data = {0x05},},
	{.cmd = 0x36, .data_len = 1, .data = {0x00},},
	{.cmd = 0x35, .data_len = 1, .data = {0x03},},
	{.cmd = 0x11, .data_len = 0, .data = {},},
};

static int spd2010_lcd_init(const struct device *dev)
{
	uint16_t i;
	k_sleep(K_MSEC(20));
	for (i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		int ret = spd2010_transmit(dev, items[i].cmd, &items[i].data, items[i].data_len);

		if (ret) {
			LOG_ERR("spd2010_transmit failed, ret:%d", ret);
			return ret;
		}
	}

	k_sleep(K_MSEC(100));
	spd2010_blanking_off(dev);
	k_sleep(K_MSEC(100));

	return 0;
}

static int spd2010_init(const struct device *dev)
{
	const struct spd2010_config *config = dev->config;
	struct spd2010_data *data = dev->data;
	int ret;

	LOG_INF("spd2010_init");
	if (!spi_is_ready(&config->bus)) {
		LOG_ERR("SPI bus %s not ready", config->bus.bus->name);
		return -ENODEV;
	}

	if (config->reset.port != NULL) {
		if (!device_is_ready(config->reset.port)) {
			LOG_ERR("Reset GPIO port for display not ready");
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&config->reset, GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Couldn't configure reset pin");
			return ret;
		}
	}

	if (!device_is_ready(config->cmd_data.port)) {
		LOG_ERR("cmd/DATA GPIO port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->cmd_data, GPIO_OUTPUT);
	if (ret) {
		LOG_ERR("Couldn't configure cmd/DATA pin");
		return ret;
	}

	if (config->te.port != NULL) {
		if (!device_is_ready(config->te.port)) {
			LOG_ERR("TE GPIO port for display not ready");
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&config->te, GPIO_INPUT);
		if (ret) {
			LOG_ERR("Couldn't configure reset pin");
			return ret;
		}

		gpio_init_callback(&data->te_callback, spd2010_te_gpio_callback_handler,
				   BIT(config->te.pin));

		ret = gpio_add_callback(config->te.port, &data->te_callback);
		if (ret) {
			LOG_ERR("Te GPIO add callback failed, ret:%d", ret);
		}

		ret = gpio_pin_interrupt_configure_dt(&config->te, GPIO_INT_EDGE_TO_ACTIVE);

		if (ret) {
			LOG_ERR("Te GPIO add interrupt configure failed, ret:%d", ret);
		}
	}

	spd2010_reset_display(dev);
	ret = spd2010_lcd_init(dev);

	if (ret) {
		return ret;
	}

	return 0;
}

static const struct display_driver_api spd2010_api = {
	.blanking_on = spd2010_blanking_on,
	.blanking_off = spd2010_blanking_off,
	.write = spd2010_write,
	.read = spd2010_read,
	.get_framebuffer = spd2010_get_framebuffer,
	.set_brightness = spd2010_set_brightness,
	.set_contrast = spd2010_set_contrast,
	.get_capabilities = spd2010_get_capabilities,
	.set_pixel_format = spd2010_set_pixel_format,
	.set_orientation = spd2010_set_orientation,
};

#define SPD2010_INIT(inst)                                                                         \
	static struct spd2010_config spd2010_config_##inst = {                                     \
		.bus = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER | SPI_WORD_SET(8), 0),        \
		.cmd_data = GPIO_DT_SPEC_INST_GET(inst, cmd_data_gpios),                           \
		.reset = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {}),                          \
		.te = GPIO_DT_SPEC_INST_GET_OR(inst, te_gpios, {}),                                \
		.width = DT_INST_PROP(inst, width),                                                \
		.height = DT_INST_PROP(inst, height),                                              \
		.inversion_on = DT_INST_PROP(inst, inversion_on),                                  \
	};                                                                                         \
	static struct spd2010_data spd2010_data_##inst = {                                         \
		.x_offset = DT_INST_PROP(inst, x_offset),                                          \
		.y_offset = DT_INST_PROP(inst, y_offset),                                          \
		.te_sem = Z_SEM_INITIALIZER(spd2010_data_##inst.te_sem, 0, 1),                     \
		.pixel_format = PIXEL_FORMAT_RGB_565,                                              \
		.orientation = DISPLAY_ORIENTATION_NORMAL,                                         \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, spd2010_init, NULL, &spd2010_data_##inst,                      \
			      &spd2010_config_##inst, POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,   \
			      &spd2010_api);

DT_INST_FOREACH_STATUS_OKAY(SPD2010_INIT);
