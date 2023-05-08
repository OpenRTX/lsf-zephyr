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

#include <zephyr/drivers/pwm.h>
#include <drivers/dma.h>
#include "spi.h"

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
	const struct pwm_dt_spec bl_pwm;
	uint8_t bl_brightness;
	uint8_t pixel_format;
	uint16_t height;
	uint16_t width;
	bool inversion_on;
};

struct spd2010_display_send_ctx {
	const uint8_t *buffer;
	uint32_t h;
	uint32_t w;
	uint32_t curr_w;
	uint32_t start_offset;
	uint8_t pixel_size;
};

struct spd2010_data {
	struct gpio_callback te_callback;
	struct k_sem te_sem;
	struct k_sem busy_sem;
	struct k_sem spi_done_sem;
	uint16_t x_offset;
	uint16_t y_offset;
	uint32_t pixel_format;
	uint32_t orientation;
	volatile uint32_t te_time;
	const struct device *dma;
	SPI_DEV *spi;
	struct spd2010_display_send_ctx display_ctx;
};

static int spd2010_lcd_init(const struct device *dev);
static int spd2010_set_brightness(const struct device *dev, const uint8_t brightness);
__attribute__((section(".itcm"))) static void csk_spi_isr_callback(uint32_t event,
								   uint32_t usr_param);
static void spd2010_te_gpio_callback_handler(const struct device *port, struct gpio_callback *cb,
					     gpio_port_pins_t pins);

#ifdef CONFIG_DRIVER_NON_BLOCK_INIT
static K_EVENT_DEFINE(event_group);
#define INIT_BIT 	BIT(0)
static struct k_work_delayable init_work;
static const struct device *s_init_dev = NULL;

static int wait_init_done(void)
{
	int ret = k_event_wait(&event_group, INIT_BIT, false, K_FOREVER);
	if (!(ret & INIT_BIT)) {
		return -1;
	}
	return 0;
}

static void init_work_handler(struct k_work *item)
{
	ARG_UNUSED(item);
	static int init_step = 0;
	const struct device *dev = s_init_dev;
	const struct spd2010_config *config = dev->config;

	if (init_step == 0) {
		init_step++;
		gpio_pin_set_dt(&config->reset, 1);
		k_work_reschedule(&init_work, K_MSEC(100));
	} else if (init_step == 1) {
		init_step++;
		gpio_pin_set_dt(&config->reset, 0);
		k_work_reschedule(&init_work, K_MSEC(70));
	} else if (init_step == 2) {
		k_event_set(&event_group, INIT_BIT);
		init_step = 0;
		int ret = spd2010_lcd_init(dev);
		if (ret) {
			LOG_ERR("init failed");
			return;
		}
	}
}

#else
#define	wait_init_done()

__attribute__((section(".itcm"))) static int spd2010_transmit(const struct device *dev, uint8_t cmd,
							      const void *tx_data, size_t tx_len);
static void spd2010_reset_display(const struct device *dev)
{
	LOG_DBG("Resetting display");

	const struct spd2010_config *config = dev->config;
	if (config->reset.port != NULL) {
		k_sleep(K_MSEC(50));
		gpio_pin_set_dt(&config->reset, 1);
		k_sleep(K_MSEC(100));
		gpio_pin_set_dt(&config->reset, 0);
		k_sleep(K_MSEC(70));
	} else {
		spd2010_transmit(dev, SPD2010_SOFTRESET, NULL, 0);
		k_sleep(K_MSEC(120));
	}
}
#endif //ifdef CONFIG_DRIVER_NON_BLOCK_INIT

__attribute__((section(".dtcm_data"))) volatile uint8_t spi_done = 0;

__attribute__((section(".itcm"))) static int spd2010_transmit(const struct device *dev, uint8_t cmd,
							      const void *tx_data, size_t tx_len)
{
	const struct spd2010_config *config = dev->config;
	int r;
	struct spd2010_data *data = dev->data;

	gpio_pin_set_dt(&config->cmd_data, SPD2010_CMD);

	uint32_t ctrl = 0;

	ctrl |= CSK_SPI_MODE_MASTER;
	ctrl |= CSK_SPI_CPOL0_CPHA0;
	ctrl |= CSK_SPI_MSB_LSB;
	ctrl |= CSK_SPI_TXIO_DMA;
	ctrl |= CSK_SPI_DATA_BITS(8);

	r = SPI_Control(data->spi, ctrl, 50 * 1000 * 1000);
	if (r != 0) {
		LOG_ERR("111SPI_Control failed, error code:%d", r);
		return r;
	}

	/* cmd */
	r = SPI_Send(data->spi, &cmd, 1);
	if (r != 0) {
		LOG_ERR("SPI_Send failed, error code:%d", r);
		return r;
	}
	k_sem_take(&data->spi_done_sem, K_FOREVER);
	if ((tx_data != NULL) && (tx_len > 0)) {
		gpio_pin_set_dt(&config->cmd_data, SPD2010_DATA);
		/* data */
		r = SPI_Send(data->spi, tx_data, tx_len);
		if (r != 0) {
			LOG_ERR("SPI_Send failed, error code:%d", r);
			return r;
		}
		k_sem_take(&data->spi_done_sem, K_FOREVER);
	}

	return 0;
}

__attribute__((section(".itcm"))) int32_t SPI_Send_1(void *spi_dev, const void *data, uint32_t num,
						     uint32_t w)
{
	static uint8_t first = 1;
	const uint8_t ch = CONFIG_SPD2010_DMA_CHANNEL;
	SPI_DEV *spi = (spi_dev);
	int32_t stat;


	if ((data == NULL) || (num == 0U) || (num > MAX_TRANCNT))
		return CSK_DRIVER_ERROR_PARAMETER;

	if (!(spi->info->flags & SPI_FLAG_CONFIGURED))
		return CSK_DRIVER_ERROR;

	if (spi->info->status.bit.busy)
		return CSK_DRIVER_ERROR_BUSY;

	if (first) {
		int ret_ch = dma_channel_reserve(ch, spi->dma_tx.cb_event, spi->dma_tx.usr_param,
						 DMA_CACHE_SYNC_NOP);
		if (ret_ch != ch) {
			LOG_ERR("ch %d is not a valid", ch);
			k_panic();
		}
		first = 0;
	}
	spi->info->tx_dyn_dma_ch = ch;

	// set busy flag
	spi->info->status.bit.busy = 1U;
	spi->info->status.bit.data_unf = 0;
	spi->info->status.bit.tx_mode = NO_IO;
	spi->info->xfer.tx_buf = data;
	spi->info->xfer.tx_cnt = 0U;
	spi->info->xfer.req_tx_cnt = num;
	spi->info->xfer.cur_op = SPI_SEND;

	while ((spi->reg->STATUS) & 0x1);

	spi->reg->TRANSCTRL = (SPI_TRANSMODE_WRONLY | WR_TRANCNT(num) |
			       (spi->reg->TRANSCTRL & SPI_TRANSMODE_CMD_EN));

	spi->info->xfer.dma_tx_done = 0;
	uint32_t is_gather = 0;
	is_gather = (w - 1) | (1) << 20;
	const uint32_t control = DMA_CH_CTLL_DST_WIDTH(spi->info->dma_width_shift) |
				 DMA_CH_CTLL_SRC_WIDTH(spi->info->dma_width_shift) |
				 DMA_CH_CTLL_DST_BSIZE(spi->info->tx_bsize_shift) |
				 DMA_CH_CTLL_SRC_BSIZE(spi->info->tx_bsize_shift) |
				 DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_DEC | DMA_CH_CTLL_TTFC_M2P |
				 DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN |
				 DMA_CH_CTLL_S_GATH_EN;

	const uint32_t config_low = DMA_CH_CFGL_CH_PRIOR(1);
	const uint32_t config_high = DMA_CH_CFGH_DST_PER(spi->dma_tx.reqsel);

	stat = dma_channel_configure(spi->info->tx_dyn_dma_ch, (uint32_t)spi->info->xfer.tx_buf,
				     (uint32_t)(&(spi->reg->DATA)), num, control, config_low,
				     config_high, is_gather, 0);

	if (stat != 0) {
		LOG_ERR("dma ch %d config failed, ret:%d", ch, stat);
		return stat;
	}

	spi->reg->CTRL |= TXDMAEN;
	// enable interrupts
	spi->reg->INTREN = SPI_ENDINT;
	spi->info->status.bit.tx_mode = DMA_IO;
	while (SPI_TXFIFO_EMPTY(spi));
	spi->reg->CMD = 0;

	return CSK_DRIVER_OK;
}

__attribute__((section(".itcm"))) static int
spd2010_set_display_datas_with_dma_rotate_90(const struct device *dev, uint8_t *datas,
					     uint32_t datas_size, uint32_t w, uint32_t h)
{
	struct spd2010_data *data = dev->data;

	datas_size >>= 1;

	if (datas_size == 0) {
		return 0;
	}

	if (datas_size >= 512) {
		/* 不可能会出现,这里还是检查下 */
		k_panic();
	}

	uint32_t cnt = datas_size;

	int r = SPI_Send_1(data->spi, datas, cnt, w);
	if (r != 0) {
		LOG_ERR("SPI_Send_1 failed, error code:%d", r);
		return r;
	}

	return 0;
}

static int spd2010_blanking_on(const struct device *dev)
{
	wait_init_done();
	return spd2010_transmit(dev, SPD2010_DISPOFF, NULL, 0);
}

static int spd2010_blanking_off(const struct device *dev)
{
	wait_init_done();
	return spd2010_transmit(dev, SPD2010_DISPON, NULL, 0);
}

static int spd2010_te_on(const struct device *dev)
{
	/**
	 * @brief TEON
	 * 	cmd: 0x35, parameter: TEM[bit0:bit0]
	 * 	- When TEM ='0': The Tearing Effect output line consists of V-Blanking information
	 * only
	 * 	- When TEM =’1’: The Tearing Effect output Line consists of both V-Blanking and
	 * H-Blanking information
	 */

	uint8_t data = 0x00;
	return spd2010_transmit(dev, SPD2010_TEON, &data, 1);
}

typedef struct {
	uint8_t cmd;
	uint16_t delay;//delay ms
	uint8_t data_len;
	uint8_t data[4];
} spd2010_sleep_cmd_item_t;

const static spd2010_sleep_cmd_item_t sleep_out[] = {
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20 ,0x10 ,0x2D},},
	{.cmd = 0x02, .data_len = 1,  .data = {0x00},},
	// {.cmd = 0xFF, .data_len = 3,  .data = {0x20 ,0x10 ,0x12},},
	// {.cmd = 0x18, .data_len = 1,  .data = {0x02},},
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20 ,0x10 ,0x44},},
	{.cmd = 0x10, .data_len = 1,  .data = {0x55},},//Self recovery for 1.45',1.99',2.22'
	{.cmd = 0xFF, .data_len = 3,  .data = {0x20 ,0x10 ,0x00},},
	{.delay=5,},
	{.cmd = SPD2010_SLPOUT, .data_len = 1,  .data = {0x00},},
	{.delay=120,},
	{.cmd = 0x29, .data_len = 1,  .data = {0x00},},
};

static int spd2010_exit_sleep(const struct device *dev)
{
	int i;
	int ret = 0;
	for (i = 0; i < sizeof(sleep_out) / sizeof(sleep_out[0]); i++) {
		if (sleep_out[i].cmd != 0x0) {
			k_msleep(sleep_out[i].delay);
			continue;
		}
		ret = spd2010_transmit(dev, sleep_out[i].cmd, &sleep_out[i].data, sleep_out[i].data_len);
	}
	
	// spd2010_transmit(dev, SPD2010_SLPOUT, NULL, 0);
	return ret;
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

__attribute__((section(".itcm"))) static int
spd2010_write(const struct device *dev, const uint16_t x, const uint16_t y,
	      const struct display_buffer_descriptor *desc, const void *buf)
{
	wait_init_done();
	const struct spd2010_config *config = dev->config;
	struct spd2010_data *data = dev->data;

	/* 坐标转换旋转90度 */
	uint32_t new_h = desc->width;
	uint32_t new_w = desc->height;
	uint32_t new_y = x;
	uint32_t new_x = config->width - (y + desc->height);

	spd2010_set_mem_area(dev, new_x, new_y, new_w, new_h);

	if (device_is_ready(config->te.port)) {
		/* TODO:这里RESET后强制等待一个TE信号， 这里需要优化*/
		k_sem_reset(&data->te_sem);
		int r = k_sem_take(&data->te_sem, K_MSEC(60));
		if (r != 0) {
			LOG_ERR("te sem take timeout");
		}
	}

	const uint8_t *write_data_start = (uint8_t *)buf;
	uint16_t write_h;
	uint32_t start_offset = desc->width * (desc->height - 1) * SPD2010_PIXEL_SIZE;
	write_h = desc->height;
	spd2010_transmit(dev, SPD2010_WRMEMST, NULL, 0);

	SCB_CleanDCache_by_Addr((void *)write_data_start, desc->width * desc->height * SPD2010_PIXEL_SIZE);
	/* 发送数据 */
	gpio_pin_set_dt(&config->cmd_data, SPD2010_DATA);
	const uint32_t ctrl = CSK_SPI_MODE_MASTER | CSK_SPI_CPOL0_CPHA0 | CSK_SPI_MSB_LSB |
			      CSK_SPI_TXIO_DMA | CSK_SPI_DATA_BITS(16);
	const uint32_t speed = 50 * 1000 * 1000;
	int r = SPI_Control(data->spi, ctrl, speed);
	if (r != 0) {
		LOG_ERR("SPI_Control failed, error code:%d", r);
		return r;
	}

	data->display_ctx.buffer = write_data_start;
	data->display_ctx.h = desc->height;
	data->display_ctx.w = desc->width;
	data->display_ctx.curr_w = 1;
	data->display_ctx.pixel_size = SPD2010_PIXEL_SIZE;
	data->display_ctx.start_offset = start_offset;

	/* 剩下的数据将会在中断中发送 */
	spd2010_set_display_datas_with_dma_rotate_90(dev, (void *)(write_data_start + start_offset),
						     1 * SPD2010_PIXEL_SIZE * write_h, desc->width,
						     desc->height);
	k_sem_take(&data->spi_done_sem, K_FOREVER);

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
	const struct spd2010_config *config = dev->config;
	wait_init_done();
	if (!device_is_ready(config->bl_pwm.dev)) {
		LOG_ERR("%s, The current device does not support brightness adjustment.", dev->name);
		return -ENOTSUP;
	}
	if (brightness > 100) {
		LOG_ERR("Set brightness, brightness value error!");
		return -EINVAL;
	}
	if(pwm_set_pulse_dt(&config->bl_pwm, config->bl_pwm.period * (100-brightness) / 100)) {
		LOG_ERR("Set brightness, PWM device config error!");
		return -1;
	}
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

	// wait_init_done();
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
	uint8_t madctr_value;
	struct spd2010_data *data = dev->data;
	int r;

	wait_init_done();
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

	wait_init_done();
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

static int spd2010_lcd_init(const struct device *dev)
{
	uint16_t i;
	const struct spd2010_config *config = dev->config;
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
	LOG_INF("spd2010_set_pixel_format");
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

	const struct pwm_dt_spec *pwm_backlight = &config->bl_pwm;

	if (!device_is_ready(pwm_backlight->dev)) {
		LOG_ERR("%s: pwm device not ready", pwm_backlight->dev->name);
		return -ENODEV;
	}

	spd2010_set_brightness(dev, config->bl_brightness);

	return 0;
}

__attribute__((section(".itcm"))) static void csk_spi_isr_callback(uint32_t event,
								   uint32_t usr_param)
{
	if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
		const struct device *dev = (const struct device *)usr_param;
		struct spd2010_data *data = dev->data;

		if (data->display_ctx.buffer) {
			/* 表示发送的是显示数据 */
			if (data->display_ctx.curr_w < data->display_ctx.w) {
				data->display_ctx.start_offset += 2;
				spd2010_set_display_datas_with_dma_rotate_90(
					dev,
					(void *)(data->display_ctx.buffer +
						 data->display_ctx.start_offset),
					data->display_ctx.pixel_size * data->display_ctx.h,
					data->display_ctx.w, data->display_ctx.h);

				data->display_ctx.curr_w++;
			} else {
				k_sem_give(&data->spi_done_sem);
				data->display_ctx.buffer = NULL;
			}
		} else {
			/* 表示发送的是命令 */
			k_sem_give(&data->spi_done_sem);
		}
	}
}

static int spd2010_init(const struct device *dev)
{
	const struct spd2010_config *config = dev->config;
	struct spd2010_data *data = dev->data;
	int ret;

	memset(&data->display_ctx, 0, sizeof(struct spd2010_display_send_ctx));

	if (!spi_is_ready(&config->bus)) {
		LOG_ERR("SPI bus %s not ready", config->bus.bus->name);
		return -ENODEV;
	} else {
		/**
		 * SPI驱动已经初始化过了，为了直接调用hal驱动，这里需要改变SPI的事件回调地址
		 *
		 * 也就是说SPI驱动不能进行复用
		 * 
		 * 请确保SPI设备的名称为"SPI_0"和"SPI_1"
		 */
		SPI_DEV *hal_spi = NULL;
		if (strcmp(config->bus.bus->name, "SPI_0") == 0) {
			hal_spi = SPI0();
		} else if (strcmp(config->bus.bus->name, "SPI_1") == 0) {
			hal_spi = SPI1();
		} else {
			LOG_ERR("SPI device name: %s is not '%s' or '%s'!", config->bus.bus->name,
				"SPI_0", "SPI_1");
			k_panic();
		}

		hal_spi->info->cb_event = csk_spi_isr_callback;
		hal_spi->info->usr_param = (uint32_t)dev;
		data->spi = hal_spi;
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

#ifdef CONFIG_DRIVER_NON_BLOCK_INIT
	s_init_dev = dev;
	k_work_init_delayable(&init_work, init_work_handler);
	k_work_schedule(&init_work, K_MSEC(50));
#else
	spd2010_reset_display(dev);
	ret = spd2010_lcd_init(dev);
	if (ret) {
		return ret;
	}
#endif

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
		.bl_pwm = PWM_DT_SPEC_INST_GET_OR(inst, pwms),                             	\
		.bl_brightness = DT_INST_PROP(inst, bl_default_brightness),                          \
		.width = DT_INST_PROP(inst, width),                                                \
		.height = DT_INST_PROP(inst, height),                                              \
		.inversion_on = DT_INST_PROP(inst, inversion_on),                                  \
	};                                                                                         \
	static struct spd2010_data spd2010_data_##inst = {                                         \
		.x_offset = DT_INST_PROP(inst, x_offset),                                          \
		.y_offset = DT_INST_PROP(inst, y_offset),                                          \
		.te_sem = Z_SEM_INITIALIZER(spd2010_data_##inst.te_sem, 0, 1),                     \
		.busy_sem = Z_SEM_INITIALIZER(spd2010_data_##inst.busy_sem, 1, 1),                     \
		.spi_done_sem = Z_SEM_INITIALIZER(spd2010_data_##inst.spi_done_sem, 0, 1),                     \
		.pixel_format = PIXEL_FORMAT_RGB_565,                                              \
		.orientation = DISPLAY_ORIENTATION_NORMAL,                                         \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, spd2010_init, NULL, &spd2010_data_##inst,                      \
			      &spd2010_config_##inst, POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,   \
			      &spd2010_api);

DT_INST_FOREACH_STATUS_OKAY(SPD2010_INIT);
