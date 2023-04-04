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

#include <drivers/dma.h>

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

#define SPD2010_PIXEL_SIZE 2
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
	volatile uint32_t te_time;
	struct device *dma;
};

#define CONFIG_TEST_DMA_CHANNEL_NUN (4)

K_SEM_DEFINE(dma_sem, 0, 1);

void dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
	if (status == 0) {
		k_sem_give(&dma_sem);
	}
}

__attribute__((section(".dtcm"))) volatile uint8_t dma_done = 0;
__attribute__((section(".itcm"))) static void
csk_dma_event_callback(uint32_t event_ctx, uint32_t xfer_bytes, uint32_t arg)
{
	dma_done = 1;
}
#include "dma.h"
__attribute__((section(".itcm"))) int32_t
test_rotate_90_1(const struct device *dma, void *src, void *dst, uint32_t h, uint32_t w, uint8_t b)
{
	uint8_t real_channel = 3;
	int r;
	uint32_t i = 0;
	uint32_t x_offset = 0;
	uint32_t y_offset = h * b;
	uint32_t dst_scat = (h - 1) | 1 << 20;

	extern void dmd_fast_relaod(uint8_t ch, uint32_t src, uint32_t dst, uint32_t size);

	x_offset = i * w * b;
	y_offset -= b;
	dma_done = 0;
	static uint8_t first = 1;
	if (first) {
		dma_memcpy_SG(real_channel, src + x_offset, dst + y_offset, w * b, 0, dst_scat, 1, 1);
		// first = 0;
	} else {
		dmd_fast_relaod(real_channel, src + x_offset, dst + y_offset, w * b);
	}
	while (!dma_done)
		;
	i++;
	x_offset = i * w * b;
	y_offset -= b;
	while (i < h) {
		dma_done = 0;
		dmd_fast_relaod(real_channel, src + x_offset, dst + y_offset, w * b);
		x_offset = i * w * b;
		y_offset -= b;
		i++;
		while (!dma_done)
			;
	}
}

int32_t test_rotate_90(const struct device *dma, void *src, void *dst, uint32_t h, uint32_t w,
		       uint8_t b)
{
	int r;

	struct dma_block_config block_config_1 = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = w * b,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.dest_scatter_en = 1,
		.dest_scatter_count = 1,
		.dest_scatter_interval = h - 1,
	};

	struct dma_config dma_cfg_1 = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config_1,
		.source_data_size = b,
		.dest_data_size = b,
		.channel_priority = 1,
	};

	if (!device_is_ready(dma)) {
		LOG_ERR("data->dma not ready");
		return -ENODEV;
	}
	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg_1)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	// memset(dst, 0, h * w * b);

	uint32_t i = 0;
	// uint32_t time = k_uptime_get_32();

	uint32_t x_offset = 0;
	uint32_t y_offset = h * b;

	while (i < h) {
		x_offset = i * w * b;
		y_offset -= b;
		r = dma_reload(dma, CONFIG_TEST_DMA_CHANNEL_NUN, src + x_offset, dst + y_offset,
			       w * b);

		if (r) {
			printk("DMA reload failed, error code:%d\n", r);
			return -1;
		}

		if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
			printk("DMA start failed, error code:%d\n", r);
			return -1;
		}
		k_sem_take(&dma_sem, K_FOREVER);
		i++;
	}

	return 0;
}

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

static int spd2010_set_page(const struct device *dev, uint8_t page)
{
	uint8_t datas[] = {0x20, 0x10, 0x00};

	int ret = spd2010_transmit(dev, 0xFF, datas, sizeof(datas));

	return ret;
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

static int spd2010_te_on(const struct device *dev)
{
	uint8_t data = 0x00;
	return spd2010_transmit(dev, SPD2010_TEON, &data, 1);
}

static int spd2010_exit_sleep(const struct device *dev)
{
	return spd2010_transmit(dev, SPD2010_SLPOUT, NULL, 0);
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

#define BUFFER_COUNT 2
#define BUFFER_SIZE                                                                                \
	(CONFIG_LV_Z_HOR_RES_MAX * CONFIG_LV_Z_VER_RES_MAX * 2 * CONFIG_LV_Z_VDB_SIZE / 100)

static __attribute__((section(".psram_section")))
uint8_t item_buf[BUFFER_COUNT * BUFFER_SIZE] = {0};

typedef struct fifo_item {
	void *reserved;
	uint16_t x;
	uint16_t y;
	struct display_buffer_descriptor desc;
	void *buf;
} fifo_item_t;

K_FIFO_DEFINE(flush_fifo);
K_FIFO_DEFINE(empty_fifo);

static fifo_item_t flush_items[BUFFER_COUNT] = {0};
void flush_task(void *p1, void *p2, void *p3)
{
	fifo_item_t *item;
	const struct device *dev = DEVICE_DT_INST_GET(0);
	while (1) {
		item = k_fifo_get(&flush_fifo, K_FOREVER);
		uint32_t time = k_uptime_get_32();
		const uint8_t *write_data_start = (uint8_t *)item->buf;

		uint16_t write_cnt;
		uint16_t nbr_of_writes;
		uint16_t write_h;

		spd2010_set_mem_area(dev, item->x, item->y, item->desc.width, item->desc.height);
		if (item->desc.pitch > item->desc.width) {
			write_h = 1U;
			nbr_of_writes = item->desc.height;
		} else {
			write_h = item->desc.height;
			nbr_of_writes = 1U;
		}

		spd2010_transmit(dev, SPD2010_WRMEMST, (void *)write_data_start,
				 item->desc.width * SPD2010_PIXEL_SIZE * write_h);
		write_data_start += (item->desc.pitch * SPD2010_PIXEL_SIZE);
		for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
			spd2010_transmit(dev, SPD2010_WRMEMCONT, (void *)write_data_start,
					 item->desc.width * SPD2010_PIXEL_SIZE * write_h);
			write_data_start += (item->desc.pitch * SPD2010_PIXEL_SIZE);
		}
		LOG_INF("flush datas cost %dms\n", k_uptime_get_32() - time);
		k_fifo_put(&empty_fifo, item);
	}
}

K_THREAD_DEFINE(flush_task_id, 4096, flush_task, NULL, NULL, NULL, -16, 0, 0);

static int spd2010_write(const struct device *dev, const uint16_t x, const uint16_t y,
			 const struct display_buffer_descriptor *desc2, const void *buf)
{
	const struct spd2010_config *config = dev->config;
	struct spd2010_data *data = dev->data;
	const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma0));

	fifo_item_t *item;
	item = k_fifo_get(&empty_fifo, K_MSEC(1000));

	/* TODO:在初始化后这里存在NULL,需要排查 */
	if (item == NULL) {
		printk("item == NULL\n");
		return -1;
	}

	uint32_t time = k_uptime_get_32();
	/* 图像数据旋转，这里最后一个参数表示一个像素点用2个字节表示，不能使用RGB88，只能是RGB565或BGR565 */
	test_rotate_90_1(dma, buf, item->buf, desc2->height, desc2->width, 2);
	LOG_INF("rotate cost %dms\n", k_uptime_get_32() - time);

	/* 坐标转换 */
	memcpy(&item->desc, desc2, sizeof(struct display_buffer_descriptor));
	item->desc.height = desc2->width;
	item->desc.width = desc2->height;
	item->desc.pitch = item->desc.width;
	item->y = x;
	item->x = config->width - (y + desc2->height);

	/* 发送给刷新线程 */
	k_fifo_put(&flush_fifo, item);

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
	uint8_t pixel_value;
	;
	uint8_t madctr_value;
	struct spd2010_data *data = dev->data;
	int r;

	if (data->orientation == DISPLAY_ORIENTATION_NORMAL) {
		madctr_value = 0x00;
	} else {
		madctr_value = 0x03;
	}

	switch (pixel_format) {
	case PIXEL_FORMAT_RGB_888:
		pixel_value = 7;
		break;
	case PIXEL_FORMAT_RGB_565:
		pixel_value = 5;
		break;
	case PIXEL_FORMAT_BGR_565:
		madctr_value |= 1 << 3;
		pixel_value = 5;
		break;
	default:
		return -ENOTSUP;
		break;
	}

	r = spd2010_transmit(dev, SPD2010_MADCTR, &madctr_value, 1);
	if (r) {
		return r;
	}

	r = spd2010_transmit(dev, SPD2010_SETPIXEL, &pixel_value, 1);
	data->pixel_format = pixel_format;

	return r;
}

static int spd2010_set_orientation(const struct device *dev,
				   const enum display_orientation orientation)
{
	struct spd2010_data *data = dev->data;
	uint8_t value;
	int r;

	if (orientation == DISPLAY_ORIENTATION_ROTATED_90 ||
	    orientation == DISPLAY_ORIENTATION_ROTATED_270) {
		return -ENOTSUP;
	}

	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		value = 0x00;
	} else if (orientation == DISPLAY_ORIENTATION_ROTATED_180) {
		value = 0x03;
	}

	if (data->pixel_format == PIXEL_FORMAT_RGB_565) {

	} else if (data->pixel_format == PIXEL_FORMAT_BGR_565) {
		value |= (1 << 3);
	}
	r = spd2010_transmit(dev, SPD2010_MADCTR, &value, 1);

	data->orientation = orientation;

	return r;
}

static void spd2010_te_gpio_callback_handler(const struct device *port, struct gpio_callback *cb,
					     gpio_port_pins_t pins)
{
	struct spd2010_data *data = CONTAINER_OF(cb, struct spd2010_data, te_callback);
	data->te_time = k_uptime_get_32();
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
};

static uint8_t init_datas[2 * 2 * 180] = {0};

static int spd2010_lcd_init(const struct device *dev)
{
	uint16_t i;
	const struct spd2010_config *config = dev->config;

	k_sleep(K_MSEC(20));
	for (i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		int ret = spd2010_transmit(dev, items[i].cmd, &items[i].data, items[i].data_len);

		if (ret) {
			LOG_ERR("spd2010_transmit failed, ret:%d", ret);
			return ret;
		}
	}

#if CONFIG_SPD2010_ORIENTATION_NORMAL
	spd2010_set_orientation(dev, DISPLAY_ORIENTATION_NORMAL);
#else
	spd2010_set_orientation(dev, DISPLAY_ORIENTATION_ROTATED_180);
#endif

#if CONFIG_SPD2010_FIXEL_FORMAT_RGB565
	spd2010_set_pixel_format(dev, PIXEL_FORMAT_RGB_565);
#elif CONFIG_SPD2010_FIXEL_FORMAT_BGR565
	spd2010_set_pixel_format(dev, PIXEL_FORMAT_BGR_565);
#elif CONFIG_SPD2010_FIXEL_FORMAT_RGB888
	spd2010_set_pixel_format(dev, PIXEL_FORMAT_RGB_888);
#endif

	if (config->te.port != NULL) {
		spd2010_te_on(dev);
	}

	spd2010_exit_sleep(dev);

	k_sleep(K_MSEC(100));
	spd2010_blanking_off(dev);
	k_sleep(K_MSEC(100));

	/*TODO:  */
	memset(init_datas, 0x00, sizeof(init_datas));
	struct display_buffer_descriptor desc = {
		.buf_size = desc.width,
		.width = 180,
		.height = 2,
		.pitch = 180,
	};
	spd2010_write(dev, 0, 452, &desc, init_datas);

	return 0;
}

struct dma_block_config block_config = {
	.source_address = (uint32_t)NULL,
	.dest_address = (uint32_t)NULL,
	.block_size = 2 * 2,
	.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	.dest_scatter_en = 1,
	.dest_scatter_count = 1,
	.dest_scatter_interval = 2 - 1,
};

struct dma_config dma_cfg = {
	.channel_direction = MEMORY_TO_MEMORY,
	.dma_callback = dma_callback,
	.user_data = NULL,
	.block_count = 1,
	.head_block = &block_config,
	.source_data_size = 2,
	.dest_data_size = 2,
	.channel_priority = 1,
};

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
	const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma0));

	data->dma = dma;

	if (!device_is_ready(data->dma)) {
		LOG_ERR("data->dma not ready");
		return -ENODEV;
	}

	if ((ret = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", ret);
		return -1;
	}

	dma_channel_reserve(3, csk_dma_event_callback, NULL, DMA_CACHE_SYNC_NOP);

	uint8_t i;
	for (i = 0; i < BUFFER_COUNT; i++) {
		flush_items[i].buf = &item_buf[0] + i * BUFFER_SIZE;
		k_fifo_put(&empty_fifo, &flush_items[i]);
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
