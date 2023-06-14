/*
 * Copyright (c) 2017 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2019 Marc Reilly
 * Copyright (c) 2019 PHYTEC Messtechnik GmbH
 * Copyright (c) 2020 Endian Technologies AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sitronix_st7789v

#include "display_st7789v.h"

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/display.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_st7789v, 4);

#include "csk6_cm33/include/spi.h"

struct st7789v_config {
	struct spi_dt_spec bus;
	struct gpio_dt_spec cmd_data_gpio;
	struct gpio_dt_spec reset_gpio;
	uint8_t vcom;
	uint8_t gctrl;
	bool vdv_vrh_enable;
	uint8_t vrh_value;
	uint8_t vdv_value;
	uint8_t mdac;
	uint8_t gamma;
	uint8_t colmod;
	uint8_t lcm;
	uint8_t porch_param[5];
	uint8_t cmd2en_param[4];
	uint8_t pwctrl1_param[2];
	uint8_t pvgam_param[14];
	uint8_t nvgam_param[14];
	uint8_t ram_param[2];
	uint8_t rgb_param[3];
	uint16_t height;
	uint16_t width;
};

struct st7789v_display_send_ctx {
	const uint8_t *buffer;
	uint32_t h;
	uint32_t w;

	uint32_t line_size;
	uint32_t line_cnt;
	uint32_t curr_cnt;
	uint32_t step_offset;
	uint8_t rotate_90;
};

struct st7789v_data {
	uint16_t x_offset;
	uint16_t y_offset;
	struct k_sem spi_done_sem;
	SPI_DEV *spi;
	struct st7789v_display_send_ctx display_ctx;
};

#ifdef CONFIG_ST7789V_RGB565
#define ST7789V_PIXEL_SIZE 2u
#else
#define ST7789V_PIXEL_SIZE 3u
#endif

__attribute__((section(".itcm"))) static int
spd2010_set_display_datas_with_dma(const struct device *dev, uint8_t *datas,
					     uint32_t datas_size, uint32_t w, uint32_t h, uint8_t rotate_90);

__attribute__((section(".itcm"))) static void csk_spi_isr_callback(uint32_t event,
								   uint32_t usr_param)
{
	if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
		const struct device *dev = (const struct device *)usr_param;
		struct st7789v_data *data = dev->data;

		if (data->display_ctx.buffer) {
			/* 表示发送的是显示数据 */
			if (data->display_ctx.curr_cnt < data->display_ctx.line_cnt) {
				data->display_ctx.buffer += data->display_ctx.step_offset;

				spd2010_set_display_datas_with_dma(
					dev, (void *)data->display_ctx.buffer,
					data->display_ctx.line_size, data->display_ctx.w,
					data->display_ctx.h, data->display_ctx.rotate_90);

				data->display_ctx.curr_cnt++;
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

__attribute__((section(".itcm"))) int32_t SPI_Send_1(void *spi_dev, const void *data, uint32_t num,
						     uint32_t gather_interval)
{
	static uint8_t first = 1;
	const uint8_t ch = CONFIG_ST7789V_CSK6_DMA_CHANNEL;
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
	uint32_t control = DMA_CH_CTLL_DST_WIDTH(spi->info->dma_width_shift) |
				 DMA_CH_CTLL_SRC_WIDTH(spi->info->dma_width_shift) |
				 DMA_CH_CTLL_DST_BSIZE(spi->info->tx_bsize_shift) |
				 DMA_CH_CTLL_SRC_BSIZE(spi->info->tx_bsize_shift) |
				 DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_TTFC_M2P |
				 DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN;

	if (gather_interval) {
		is_gather = (gather_interval - 1) | (1) << 20;
		control |= DMA_CH_CTLL_S_GATH_EN | DMA_CH_CTLL_SRC_DEC;
	} else {
		control |= DMA_CH_CTLL_SRC_INC;
	}

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
spd2010_set_display_datas_with_dma(const struct device *dev, uint8_t *datas,
					     uint32_t datas_size, uint32_t w, uint32_t h, uint8_t rotate_90)
{
	struct st7789v_data *data = dev->data;

	datas_size >>= 1;

	if (datas_size == 0) {
		return 0;
	}

	if (datas_size >= 512) {
		/* 不可能会出现,这里还是检查下 */
		k_panic();
	}

	uint32_t cnt = datas_size;

	int r = SPI_Send_1(data->spi, datas, cnt, rotate_90 ? w : 0);
	if (r != 0) {
		LOG_ERR("SPI_Send_1 failed, error code:%d", r);
		return r;
	}

	return 0;
}

static void st7789v_set_lcd_margins(const struct device *dev,
				    uint16_t x_offset, uint16_t y_offset)
{
	struct st7789v_data *data = dev->data;

	data->x_offset = x_offset;
	data->y_offset = y_offset;
}

__attribute__((section(".itcm"))) static int32_t st7789v_transmit(const struct device *dev, uint8_t cmd,
			     uint8_t *tx_data, size_t tx_len)
{
	const struct st7789v_config *config = dev->config;
	int r;
	struct st7789v_data *data = dev->data;

	gpio_pin_set_dt(&config->cmd_data_gpio, 1);

	uint32_t ctrl = 0;
	ctrl |= CSK_SPI_MODE_MASTER;
	ctrl |= CSK_SPI_CPOL0_CPHA0;
	ctrl |= CSK_SPI_MSB_LSB;
	ctrl |= CSK_SPI_TXIO_DMA;
	ctrl |= CSK_SPI_DATA_BITS(8);

	r = SPI_Control(data->spi, ctrl, 50 * 1000 * 1000);
	if (r != 0) {
		LOG_ERR("SPI_Control failed, error code:%d", r);
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
		gpio_pin_set_dt(&config->cmd_data_gpio, 0);
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

static void st7789v_exit_sleep(const struct device *dev)
{
	st7789v_transmit(dev, ST7789V_CMD_SLEEP_OUT, NULL, 0);
	k_sleep(K_MSEC(120));
}

static void st7789v_reset_display(const struct device *dev)
{
	LOG_DBG("Resetting display");

	const struct st7789v_config *config = dev->config;
	if (config->reset_gpio.port != NULL) {
		k_sleep(K_MSEC(1));
		gpio_pin_set_dt(&config->reset_gpio, 1);
		k_sleep(K_MSEC(6));
		gpio_pin_set_dt(&config->reset_gpio, 0);
		k_sleep(K_MSEC(20));
	} else {
		st7789v_transmit(dev, ST7789V_CMD_SW_RESET, NULL, 0);
		k_sleep(K_MSEC(5));
	}
}

static int st7789v_blanking_on(const struct device *dev)
{
	st7789v_transmit(dev, ST7789V_CMD_DISP_OFF, NULL, 0);
	return 0;
}

static int st7789v_blanking_off(const struct device *dev)
{
	st7789v_transmit(dev, ST7789V_CMD_DISP_ON, NULL, 0);
	return 0;
}

static int st7789v_read(const struct device *dev,
			const uint16_t x,
			const uint16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	return -ENOTSUP;
}

static void st7789v_set_mem_area(const struct device *dev, const uint16_t x,
				 const uint16_t y, const uint16_t w, const uint16_t h)
{
	struct st7789v_data *data = dev->data;
	uint16_t spi_data[2];

	uint16_t ram_x = x + data->x_offset;
	uint16_t ram_y = y + data->y_offset;

	spi_data[0] = sys_cpu_to_be16(ram_x);
	spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	st7789v_transmit(dev, ST7789V_CMD_CASET, (uint8_t *)&spi_data[0], 4);

	spi_data[0] = sys_cpu_to_be16(ram_y);
	spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	st7789v_transmit(dev, ST7789V_CMD_RASET, (uint8_t *)&spi_data[0], 4);
}

__attribute__((section(".itcm"))) static int
st7789v_write(const struct device *dev, const uint16_t x, const uint16_t y,
	      const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct st7789v_config *config = dev->config;
	struct st7789v_data *data = dev->data;
	uint8_t rotate_90 = 0;
	const uint8_t *write_data_start = (uint8_t *)buf;
	uint32_t start_offset = 0;

	uint32_t new_h = desc->height;
	uint32_t new_w = desc->width;
	uint32_t new_x = x;
	uint32_t new_y = y;

#if CONFIG_ST7789V_CSK6_ROTATE_90
	rotate_90 = 1;
#endif

	if (rotate_90) {
		/* 坐标转换旋转90度 */
		new_h = desc->width;
		new_w = desc->height;
		new_y = x;
		new_x = config->width - (y + desc->height);
		start_offset = desc->width * (desc->height - 1) * 2;
	}

	st7789v_set_mem_area(dev, new_x, new_y, new_w, new_h);
	st7789v_transmit(dev, ST7789V_CMD_RAMWR, NULL, 0);

	SCB_CleanDCache_by_Addr((void *)write_data_start, desc->width * desc->height * 2);
	gpio_pin_set_dt(&config->cmd_data_gpio, 0);
	const uint32_t ctrl = CSK_SPI_MODE_MASTER | CSK_SPI_CPOL0_CPHA0 | CSK_SPI_MSB_LSB |
			      CSK_SPI_TXIO_DMA | CSK_SPI_DATA_BITS(16);
	const uint32_t speed = 50 * 1000 * 1000;
	int r = SPI_Control(data->spi, ctrl, speed);
	if (r != 0) {
		LOG_ERR("SPI_Control failed, error code:%d", r);
		return r;
	}

	data->display_ctx.h = desc->height;
	data->display_ctx.w = desc->width;
	data->display_ctx.curr_cnt = 1;
	data->display_ctx.buffer = write_data_start + start_offset;
	data->display_ctx.rotate_90 = rotate_90;

	if (data->display_ctx.rotate_90) {
		/**
		 * ROTATE 90
		 * **********************MEM*********************
		 * Y                                         [end]
		 * A  ^    ^    ^    ^    ^    ^    ^    ^    ^
		 * L  |    |    |    |    |    |    |    |    |
		 * P  |    |    |    |    |    |    |    |    |
		 * S  |    |    |    |    |    |    |    |    |
		 * I  |    |    |    |    |    |    |    |    |
		 * D  |    |    |    |    |    |    |    |    |
		 * *  |    |    |    |    |    |    |    |    |
		 * *[start]
		 * *********************************************
		 *
		 */

		data->display_ctx.line_cnt = desc->width;
		data->display_ctx.line_size = desc->height * 2;
		data->display_ctx.step_offset = 1 * 2;
	} else {
		/**
		 * NORMAL
		 *
		 * *********************DISPALY & MEM*************************
		 * [start]------------------------------------------->
		 *        ------------------------------------------->
		 *        ------------------------------------------->
		 *        ------------------------------------------->
		 *        -------------------------------------------> [end]
		 * *********************************************************
		 */
		data->display_ctx.line_cnt = desc->height;
		data->display_ctx.line_size = desc->width * 2;
		data->display_ctx.step_offset = desc->width * 2;
	}

	/* 剩下的数据将会在中断中发送 */
	spd2010_set_display_datas_with_dma(dev, (void *)data->display_ctx.buffer,
						    data->display_ctx.line_size, data->display_ctx.w,
						     data->display_ctx.h, data->display_ctx.rotate_90);
	k_sem_take(&data->spi_done_sem, K_FOREVER);

	return 0;
}

static void *st7789v_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int st7789v_set_brightness(const struct device *dev,
			   const uint8_t brightness)
{
	return -ENOTSUP;
}

static int st7789v_set_contrast(const struct device *dev,
			 const uint8_t contrast)
{
	return -ENOTSUP;
}

static void st7789v_get_capabilities(const struct device *dev,
			      struct display_capabilities *capabilities)
{
	const struct st7789v_config *config = dev->config;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->width;
	capabilities->y_resolution = config->height;

#ifdef CONFIG_ST7789V_RGB565
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
#else
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_888;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_888;
#endif
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int st7789v_set_pixel_format(const struct device *dev,
			     const enum display_pixel_format pixel_format)
{
#ifdef CONFIG_ST7789V_RGB565
	if (pixel_format == PIXEL_FORMAT_RGB_565) {
#else
	if (pixel_format == PIXEL_FORMAT_RGB_888) {
#endif
		return 0;
	}
	LOG_ERR("Pixel format change not implemented");
	return -ENOTSUP;
}

static int st7789v_set_orientation(const struct device *dev,
			    const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}
	LOG_ERR("Changing display orientation not implemented");
	return -ENOTSUP;
}

static void st7789v_lcd_init(const struct device *dev)
{
	struct st7789v_data *data = dev->data;
	const struct st7789v_config *config = dev->config;
	uint8_t tmp;

	st7789v_set_lcd_margins(dev, data->x_offset,
				data->y_offset);

	st7789v_transmit(dev, ST7789V_CMD_CMD2EN,
			 (uint8_t *)config->cmd2en_param,
			 sizeof(config->cmd2en_param));

	st7789v_transmit(dev, ST7789V_CMD_PORCTRL,
			 (uint8_t *)config->porch_param,
			 sizeof(config->porch_param));

	/* Digital Gamma Enable, default disabled */
	tmp = 0x00;
	st7789v_transmit(dev, ST7789V_CMD_DGMEN, &tmp, 1);

	/* Frame Rate Control in Normal Mode, default value */
	tmp = 0x0f;
	st7789v_transmit(dev, ST7789V_CMD_FRCTRL2, &tmp, 1);

	tmp = config->gctrl;
	st7789v_transmit(dev, ST7789V_CMD_GCTRL, &tmp, 1);

	tmp = config->vcom;
	st7789v_transmit(dev, ST7789V_CMD_VCOMS, &tmp, 1);

	if (config->vdv_vrh_enable) {
		tmp = 0x01;
		st7789v_transmit(dev, ST7789V_CMD_VDVVRHEN, &tmp, 1);

		tmp = config->vrh_value;
		st7789v_transmit(dev, ST7789V_CMD_VRH, &tmp, 1);

		tmp = config->vdv_value;
		st7789v_transmit(dev, ST7789V_CMD_VDS, &tmp, 1);
	}

	st7789v_transmit(dev, ST7789V_CMD_PWCTRL1,
			 (uint8_t *)config->pwctrl1_param,
			 sizeof(config->pwctrl1_param));

	/* Memory Data Access Control */
	tmp = config->mdac;
	st7789v_transmit(dev, ST7789V_CMD_MADCTL, &tmp, 1);

	/* Interface Pixel Format */
	tmp = config->colmod;
	st7789v_transmit(dev, ST7789V_CMD_COLMOD, &tmp, 1);

	tmp = config->lcm;
	st7789v_transmit(dev, ST7789V_CMD_LCMCTRL, &tmp, 1);

	tmp = config->gamma;
	st7789v_transmit(dev, ST7789V_CMD_GAMSET, &tmp, 1);

	st7789v_transmit(dev, ST7789V_CMD_INV_ON, NULL, 0);

	st7789v_transmit(dev, ST7789V_CMD_PVGAMCTRL,
			 (uint8_t *)config->pvgam_param,
			 sizeof(config->pvgam_param));

	st7789v_transmit(dev, ST7789V_CMD_NVGAMCTRL,
			 (uint8_t *)config->nvgam_param,
			 sizeof(config->nvgam_param));

	st7789v_transmit(dev, ST7789V_CMD_RAMCTRL,
			 (uint8_t *)config->ram_param,
			 sizeof(config->ram_param));

	st7789v_transmit(dev, ST7789V_CMD_RGBCTRL,
			 (uint8_t *)config->rgb_param,
			 sizeof(config->rgb_param));
}

static int st7789v_init(const struct device *dev)
{
	const struct st7789v_config *config = dev->config;
	struct st7789v_data *data = dev->data;

	memset(&data->display_ctx, 0, sizeof(struct st7789v_display_send_ctx));
	LOG_INF("st7789v_init");
	if (!spi_is_ready(&config->bus)) {
		LOG_ERR("SPI device not ready");
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

	if (config->reset_gpio.port != NULL) {
		if (!device_is_ready(config->reset_gpio.port)) {
			LOG_ERR("Reset GPIO device not ready");
			return -ENODEV;
		}

		if (gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE)) {
			LOG_ERR("Couldn't configure reset pin");
			return -EIO;
		}
	}

	if (!device_is_ready(config->cmd_data_gpio.port)) {
		LOG_ERR("Reset GPIO device not ready");
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&config->cmd_data_gpio, GPIO_OUTPUT)) {
		LOG_ERR("Couldn't configure cmd/DATA pin");
		return -EIO;
	}

	st7789v_reset_display(dev);

	st7789v_blanking_on(dev);

	st7789v_lcd_init(dev);

	st7789v_exit_sleep(dev);
	LOG_INF("st7789v_init done");
	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int st7789v_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	int ret = 0;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		st7789v_exit_sleep(dev);
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		st7789v_transmit(dev, ST7789V_CMD_SLEEP_IN, NULL, 0);
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}
#endif /* CONFIG_PM_DEVICE */

static const struct display_driver_api st7789v_api = {
	.blanking_on = st7789v_blanking_on,
	.blanking_off = st7789v_blanking_off,
	.write = st7789v_write,
	.read = st7789v_read,
	.get_framebuffer = st7789v_get_framebuffer,
	.set_brightness = st7789v_set_brightness,
	.set_contrast = st7789v_set_contrast,
	.get_capabilities = st7789v_get_capabilities,
	.set_pixel_format = st7789v_set_pixel_format,
	.set_orientation = st7789v_set_orientation,
};

#define ST7789V_INIT(inst)							\
	static const struct st7789v_config st7789v_config_ ## inst = {		\
		.bus = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER |          \
							  SPI_WORD_SET(8), 0),	\
		.cmd_data_gpio = GPIO_DT_SPEC_INST_GET(inst, cmd_data_gpios),	\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {}),	\
		.vcom = DT_INST_PROP(inst, vcom),				\
		.gctrl = DT_INST_PROP(inst, gctrl),				\
		.vdv_vrh_enable = (DT_INST_NODE_HAS_PROP(inst, vrhs)            \
					&& DT_INST_NODE_HAS_PROP(inst, vdvs)),	\
		.vrh_value = DT_INST_PROP_OR(inst, vrhs, 0),			\
		.vdv_value = DT_INST_PROP_OR(inst, vdvs, 0),			\
		.mdac = DT_INST_PROP(inst, mdac),				\
		.gamma = DT_INST_PROP(inst, gamma),				\
		.colmod = DT_INST_PROP(inst, colmod),				\
		.lcm = DT_INST_PROP(inst, lcm),					\
		.porch_param = DT_INST_PROP(inst, porch_param),			\
		.cmd2en_param = DT_INST_PROP(inst, cmd2en_param),		\
		.pwctrl1_param = DT_INST_PROP(inst, pwctrl1_param),		\
		.pvgam_param = DT_INST_PROP(inst, pvgam_param),			\
		.nvgam_param = DT_INST_PROP(inst, nvgam_param),			\
		.ram_param = DT_INST_PROP(inst, ram_param),			\
		.rgb_param = DT_INST_PROP(inst, rgb_param),			\
		.width = DT_INST_PROP(inst, width),				\
		.height = DT_INST_PROP(inst, height),				\
	};									\
										\
	static struct st7789v_data st7789v_data_ ## inst = {			\
		.x_offset = DT_INST_PROP(inst, x_offset),			\
		.y_offset = DT_INST_PROP(inst, y_offset),			\
		.spi_done_sem = Z_SEM_INITIALIZER(st7789v_data_##inst.spi_done_sem, 0, 1),                     \
	};									\
										\
	PM_DEVICE_DT_INST_DEFINE(inst, st7789v_pm_action);			\
										\
	DEVICE_DT_INST_DEFINE(inst, &st7789v_init, PM_DEVICE_DT_INST_GET(inst), \
			&st7789v_data_ ## inst, &st7789v_config_ ## inst,	\
			POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,		\
			&st7789v_api);

DT_INST_FOREACH_STATUS_OKAY(ST7789V_INIT)
