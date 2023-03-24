/*
 * Copyright (c) 2020 Teslabs Engineering S.L.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT solomon_lwea2012


#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include "display_lwea2012.h"

LOG_MODULE_REGISTER(display_lwea2012, CONFIG_DISPLAY_LOG_LEVEL);

static int lwea2012_transmit(const struct device *dev, uint8_t cmd, const void *tx_data,
			     size_t tx_len)
{
	const struct lwea2012_config *config = dev->config;

	int r;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1U};

	/* send command */
	tx_buf.buf = &cmd;
	tx_buf.len = 1U;

	gpio_pin_set_dt(&config->cmd_data, LWEA2012_CMD);
	r = spi_write_dt(&config->bus, &tx_bufs);
	if (r < 0) {
		return r;
	}

	/* send data (if any) */
	if (tx_len != 0) {
		tx_buf.buf = (void *)tx_data;
		tx_buf.len = tx_len;

		gpio_pin_set_dt(&config->cmd_data, LWEA2012_DATA);
		r = spi_write_dt(&config->bus, &tx_bufs);
		if (r < 0) {
			return r;
		}
	}

	return 0;
}

static void lwea2012_reset_display(const struct device *dev)
{
	LOG_DBG("Resetting display");

	const struct lwea2012_config *config = dev->config;
	if (config->reset.port != NULL) {
		k_sleep(K_MSEC(50));
		gpio_pin_set_dt(&config->reset, 1);
		k_sleep(K_MSEC(50));
		gpio_pin_set_dt(&config->reset, 0);
		k_sleep(K_MSEC(100));
	} else {
		lwea2012_transmit(dev, LWEA2012_SOFTRESET, NULL, 0);
		k_sleep(K_MSEC(100));
	}
}

static int lwea2012_tearing_on(const struct device *dev, uint8_t telom)
{
	return lwea2012_transmit(dev, LWEA2012_TEON, &telom, 1);
}

static int lwea2012_blanking_on(const struct device *dev)
{
	return lwea2012_transmit(dev, LWEA2012_DISPOFF, NULL, 0);
}

static int lwea2012_blanking_off(const struct device *dev)
{
	return lwea2012_transmit(dev, LWEA2012_DISPON, NULL, 0);
}

static void lwea2012_set_mem_area(const struct device *dev, const uint16_t x, const uint16_t y,
				  const uint16_t w, const uint16_t h)
{
	struct lwea2012_data *data = dev->data;
	uint16_t spi_data[2];

	uint16_t ram_x = x + data->x_offset;
	uint16_t ram_y = y + data->y_offset;

	spi_data[0] = sys_cpu_to_be16(ram_x);
	spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	lwea2012_transmit(dev, LWEA2012_SETCOL, (uint8_t *)&spi_data[0], 4);

	spi_data[0] = sys_cpu_to_be16(ram_y);
	spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	lwea2012_transmit(dev, LWEA2012_SETPAGE, (uint8_t *)&spi_data[0], 4);
}

static int lwea2012_write(const struct device *dev, const uint16_t x, const uint16_t y,
			  const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct lwea2012_config *config = dev->config;
	const uint8_t *write_data_start = (uint8_t *)buf;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
	uint16_t write_cnt;
	uint16_t nbr_of_writes;
	uint16_t write_h;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
	__ASSERT((desc->pitch * LWEA2012_PIXEL_SIZE * desc->height) <= desc->buf_size,
		 "Input buffer to small");

	lwea2012_set_mem_area(dev, x, y, desc->width, desc->height);

	if (desc->pitch > desc->width) {
		write_h = 1U;
		nbr_of_writes = desc->height;
	} else {
		write_h = desc->height;
		nbr_of_writes = 1U;
	}

	lwea2012_transmit(dev, LWEA2012_WRMEMST, (void *)write_data_start,
			  desc->width * LWEA2012_PIXEL_SIZE * write_h);

	write_data_start += (desc->pitch * LWEA2012_PIXEL_SIZE);
	for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
		lwea2012_transmit(dev, LWEA2012_WRMEMCONT, (void *)write_data_start,
				  desc->width * LWEA2012_PIXEL_SIZE * write_h);
		write_data_start += (desc->pitch * LWEA2012_PIXEL_SIZE);
	}

	return 0;
}

static int lwea2012_read(const struct device *dev, const uint16_t x, const uint16_t y,
			 const struct display_buffer_descriptor *desc, void *buf)
{
	return -ENOTSUP;
}

static void *lwea2012_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int lwea2012_set_brightness(const struct device *dev, const uint8_t brightness)
{
	return -ENOTSUP;
}

static int lwea2012_set_contrast(const struct device *dev, const uint8_t contrast)
{
	return -ENOTSUP;
}

static void lwea2012_get_capabilities(const struct device *dev,
				      struct display_capabilities *capabilities)
{
	struct lwea2012_data *data = dev->data;
	const struct lwea2012_config *config = dev->config;

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

static int lwea2012_set_pixel_format(const struct device *dev,
				     const enum display_pixel_format pixel_format)
{
	uint8_t data;

	switch (pixel_format)
	{
	case PIXEL_FORMAT_RGB_888:
		data = 7;
		break;
	case PIXEL_FORMAT_RGB_565:
	case PIXEL_FORMAT_BGR_565:
	default:
		return -ENOTSUP;
		break;
	}

	return lwea2012_transmit(dev, LWEA2012_SETPIXEL, &data, 1);
}

static int lwea2012_set_orientation(const struct device *dev,
				    const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}

	LOG_ERR("Changing display orientation not implemented");

	return -ENOTSUP;
}

static void lwea2012_te_gpio_callback_handler(const struct device *port, struct gpio_callback *cb,
					      gpio_port_pins_t pins)
{
	LOG_INF("lwea2012_te_gpio_callback_handler");
	struct lwea2012_data *data = CONTAINER_OF(cb, struct lwea2012_data, te_callback);
	k_sem_give(&data->te_sem);
}

typedef struct  {
	uint8_t cmd;
	uint8_t data_len;
	uint8_t data[4];
} lwea2012_init_cmd_item_t;

static uint8_t m_buffer_tx[512 * 2];

void show_test(const struct device *dev)
{
	uint16_t h;
	for (h = 0; h < 454; h++) {
		lwea2012_set_mem_area(dev, 4, h, 180, 454);
		
		if (h % 3 == 0) {
			uint16_t i;
			for (i = 0; i < 180; ++i) {
				m_buffer_tx[i * 3] = 0xFF;     // R
				m_buffer_tx[i * 3 + 1] = 0x00; // G
				m_buffer_tx[i * 3 + 2] = 0x00; // B
			}
		} else if (h % 3 == 1) {
			uint16_t i;
			for (i = 0; i < 180; ++i) {
				m_buffer_tx[i * 3] = 0x00;     // R
				m_buffer_tx[i * 3 + 1] = 0xFF; // G
				m_buffer_tx[i * 3 + 2] = 0x00; // B
			}
		} else if (h % 3 == 2) {
			uint16_t i;
			for (i = 0; i < 180; ++i) {
				m_buffer_tx[i * 3] = 0x00;     // R
				m_buffer_tx[i * 3 + 1] = 0x00; // G
				m_buffer_tx[i * 3 + 2] = 0xFF; // B
			}
		}
		int ret = lwea2012_transmit(dev, 0x2C, NULL, 0);
		if (ret) {
			LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
		}

		ret = lwea2012_transmit(dev, 0x3C, m_buffer_tx, (180 - 4) * 3);

		if (ret) {
			LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
		}
		LOG_INF("write h:%d", h);
		k_msleep(100);
	}
}

void SPD2010_WriteImage(const struct device *dev, uint8_t idx)
{
	size_t tmp_len = 180 * 3;
	uint32_t err_code;
	// uint32_t lcd_addr;
	uint32_t i, ii;

	LCD_SetPage(dev, 0x00);
	lwea2012_set_mem_area(dev, 0, 0, 180, 454);
	if (idx == 0) {
		for (i = 0; i < 180; ++i) {
			m_buffer_tx[i * 3] = 0xFF;     // R
			m_buffer_tx[i * 3 + 1] = 0xFF; // G
			m_buffer_tx[i * 3 + 2] = 0xFF; // B
		}
	} else if (idx == 1) {
		for (i = 0; i < 180; ++i) {
			m_buffer_tx[i * 3] = 0x00;     // R
			m_buffer_tx[i * 3 + 1] = 0x00; // G
			m_buffer_tx[i * 3 + 2] = 0x00; // B
		}
	} else {
		for (i = 0; i < 180; ++i) {
			m_buffer_tx[i * 3] = 0x00;     // R
			m_buffer_tx[i * 3 + 1] = 0x00; // G
			m_buffer_tx[i * 3 + 2] = 0xFF; // B
		}
	}
	int ret = lwea2012_transmit(dev, 0x2C, NULL, 0);

	if (ret) {
		LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
	}
	for (i = 0; i < 454; i++) {

		ret = lwea2012_transmit(dev, 0x3C, m_buffer_tx, tmp_len);

		if (ret) {
			LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
		}
	}
}

void LCD_SetPage(const struct device *dev, uint8_t page)
{
	uint8_t page_data[4] = {0x20, 0x10, page, 0x00};
	page_data[2] = page;
	int ret = lwea2012_transmit(dev, 0xff, page_data, 3);

	if (ret) {
		LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
	}
}

void LCD_SetCmd1(const struct device *dev, uint8_t cmd, uint8_t value)
{
	int ret = lwea2012_transmit(dev, cmd, &value, sizeof(value));

	if (ret) {
		LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
	}
}

void LCD_SetCmd4(const struct device *dev, uint8_t cmd, uint8_t value1, uint8_t value2, uint8_t value3, uint8_t value4)
{
	uint8_t data[4];

	data[0] = value1;
	data[1] = value2;
	data[2] = value3;
	data[3] = value4;
	int ret = lwea2012_transmit(dev, cmd, data, sizeof(data));

	if (ret) {
		LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
	}
}

	const static lwea2012_init_cmd_item_t items[] = {
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
// //===TP 设置,若不使用TP功能,以下几行代码取消=== ==
	// {.cmd = 0xFF, .data_len =3,  .data = { 0x20, 0x10, 0x50},},
	// {.cmd = 0x08, .data_len =1,  .data = { 0x55,},},
	// {.cmd = 0x05, .data_len =1,  .data = { 0x08,},},
	// {.cmd = 0x01, .data_len =1,  .data = { 0xA6,},},
	// {.cmd = 0x00, .data_len =1,  .data = { 0xA6,},},
// //===========TP代码结束===================== 
	{.cmd = 0xFF, .data_len = 3, .data = {0x20, 0x10, 0xA0},},
	{.cmd = 0x08, .data_len = 1, .data = {0xdc},},
	{.cmd = 0xFF, .data_len = 3, .data = {0x20, 0x10, 0x00},},
	{.cmd = 0x2A, .data_len = 4, .data = {0x00, 0x00, 0x00, 0xB3},},
	{.cmd = 0x2B, .data_len = 4, .data = {0x00, 0x00, 0x01, 0xC5},},
	{.cmd = 0x3A, .data_len = 1, .data = {0x07},},
	{.cmd = 0x35, .data_len = 1, .data = {0x00},},
	{.cmd = 0x11, .data_len = 0, .data = {},},
};

const lwea2012_init_cmd_item_t items2[] = {
	{.cmd = 0xFF, .data_len = 3, .data = { 0x20, 0x10, 0x2D},},
	{.cmd = 0x02, .data_len = 1, .data = { 0x00},},
	{.cmd = 0xFF, .data_len = 3, .data = { 0x20, 0x10, 0x12},},
	{.cmd = 0x18, .data_len = 1, .data = { 0x02},},
	{.cmd = 0xFF, .data_len = 3, .data = { 0x20, 0x10, 0x00},},
	{.cmd = 0x11, .data_len = 1, .data = {},},
};

static int lwea2012_lcd_init(const struct device *dev)
{
	uint16_t i;
	k_sleep(K_MSEC(20));
	for (i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		int ret = lwea2012_transmit(dev, items[i].cmd,
					&items[i].data ,
					items[i].data_len);

		if (ret) {
			LOG_ERR("lwea2012_transmit failed, ret:%d", ret);
		}
	}

	k_sleep(K_MSEC(100));
	lwea2012_blanking_off(dev);
	k_sleep(K_MSEC(100));

	i = 0;
	while (1) {
		// SPD2010_WriteImage(dev, i);
		// k_sleep(K_MSEC(500));
		// i++;
		// if (i == 3) {
		// 	i = 0;
		// }

		SPD2010_WriteImage(dev, 0);
		show_test(dev);
	}
}

static int lwea2012_init(const struct device *dev)
{
	const struct lwea2012_config *config = dev->config;
	struct lwea2012_data *data = dev->data;
	int ret;

	LOG_INF("lwea2012_init");
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

		gpio_init_callback(&data->te_callback, lwea2012_te_gpio_callback_handler,
				   BIT(config->te.pin));

		ret = gpio_add_callback(config->te.port, &data->te_callback);
		if (ret) {
			LOG_ERR("Te GPIO add callback failed, ret:%d", ret);
		}

		ret = gpio_pin_interrupt_configure_dt(&config->te.port, GPIO_INT_EDGE_TO_ACTIVE);

		if (ret) {
			LOG_ERR("Te GPIO add interrupt configure failed, ret:%d", ret);
		}
	}

	lwea2012_reset_display(dev);

	ret = lwea2012_lcd_init(dev);

	if (ret) {
		return ret;
	}

	return 0;
}


static const struct display_driver_api lwea2012_api = {
	.blanking_on = lwea2012_blanking_on,
	.blanking_off = lwea2012_blanking_off,
	.write = lwea2012_write,
	.read = lwea2012_read,
	.get_framebuffer = lwea2012_get_framebuffer,
	.set_brightness = lwea2012_set_brightness,
	.set_contrast = lwea2012_set_contrast,
	.get_capabilities = lwea2012_get_capabilities,
	.set_pixel_format = lwea2012_set_pixel_format,
	.set_orientation = lwea2012_set_orientation,
};

#define LWEA2012_INIT(inst)                                                                        \
	static struct lwea2012_config lwea2012_config_##inst = {                                   \
		.bus = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER | SPI_WORD_SET(8), 0),        \
		.cmd_data = GPIO_DT_SPEC_INST_GET(inst, cmd_data_gpios),                           \
		.reset = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {}),                          \
		.te = GPIO_DT_SPEC_INST_GET_OR(inst, te_gpios, {}),                                \
		.width = DT_INST_PROP(inst, width),                                                \
		.height = DT_INST_PROP(inst, height),                                              \
		.inversion_on = DT_INST_PROP(inst, inversion_on),                                  \
	};                                                                                         \
	static struct lwea2012_data lwea2012_data_##inst = {                                       \
		.x_offset = DT_INST_PROP(inst, x_offset),                                          \
		.y_offset = DT_INST_PROP(inst, y_offset),                                          \
		.te_sem = Z_SEM_INITIALIZER(lwea2012_data_##inst.te_sem, 0, 1),                    \
		.pixel_format = PIXEL_FORMAT_RGB_888,                                              \
		.orientation = DISPLAY_ORIENTATION_NORMAL,                                              \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, lwea2012_init, NULL, &lwea2012_data_##inst,                    \
			      &lwea2012_config_##inst, POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,  \
			      &lwea2012_api);

DT_INST_FOREACH_STATUS_OKAY(LWEA2012_INIT);
