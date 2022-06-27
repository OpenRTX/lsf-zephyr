/*
 * Copyright (c) 2017 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2019 Marc Reilly
 * Copyright (c) 2019 PHYTEC Messtechnik GmbH
 * Copyright (c) 2020 Endian Technologies AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT newvision_nv3030b

#include "display_nv3030b.h"

#include <device.h>
#include <drivers/spi.h>
#include <drivers/gpio.h>
#include <sys/byteorder.h>
#include <drivers/display.h>

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(display_nv3030b);

#define NV3030B_CS_PIN		DT_INST_SPI_DEV_CS_GPIOS_PIN(0)
#define NV3030B_CMD_DATA_PIN	DT_INST_GPIO_PIN(0, cmd_data_gpios)
#define NV3030B_CMD_DATA_FLAGS	DT_INST_GPIO_FLAGS(0, cmd_data_gpios)
#define NV3030B_RESET_PIN	DT_INST_GPIO_PIN(0, reset_gpios)
#define NV3030B_RESET_FLAGS	DT_INST_GPIO_FLAGS(0, reset_gpios)

static uint8_t nv3030b_porch_param[] = DT_INST_PROP(0, porch_param);
static uint8_t nv3030b_cmd2en_param[] = DT_INST_PROP(0, cmd2en_param);
static uint8_t nv3030b_pwctrl1_param[] = DT_INST_PROP(0, pwctrl1_param);
static uint8_t nv3030b_pvgam_param[] = DT_INST_PROP(0, pvgam_param);
static uint8_t nv3030b_nvgam_param[] = DT_INST_PROP(0, nvgam_param);
static uint8_t nv3030b_ram_param[] = DT_INST_PROP(0, ram_param);
static uint8_t nv3030b_rgb_param[] = DT_INST_PROP(0, rgb_param);

#define ST7789	0

struct nv3030b_data {
	const struct device *spi_dev;
	struct spi_config spi_config;
#if DT_INST_SPI_DEV_HAS_CS_GPIOS(0)
	struct spi_cs_control cs_ctrl;
#endif

#if DT_INST_NODE_HAS_PROP(0, reset_gpios)
	const struct device *reset_gpio;
#endif
	const struct device *cmd_data_gpio;

	uint16_t height;
	uint16_t width;
	uint16_t x_offset;
	uint16_t y_offset;
};

#ifdef CONFIG_NV3030B_RGB565
#define NV3030B_PIXEL_SIZE 2u
#else
#define NV3030B_PIXEL_SIZE 3u
#endif

static void nv3030b_set_lcd_margins(struct nv3030b_data *data,
			     uint16_t x_offset, uint16_t y_offset)
{
	data->x_offset = x_offset;
	data->y_offset = y_offset;
}

static void nv3030b_set_cmd(struct nv3030b_data *data, int is_cmd)
{
	gpio_pin_set(data->cmd_data_gpio, NV3030B_CMD_DATA_PIN, is_cmd);
}

static void nv3030b_transmit(struct nv3030b_data *data, uint8_t cmd,
		uint8_t *tx_data, size_t tx_count)
{
	struct spi_buf tx_buf = { .buf = &cmd, .len = 1 };
	struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

	nv3030b_set_cmd(data, 1);
	spi_write(data->spi_dev, &data->spi_config, &tx_bufs);

	if (tx_data != NULL) {
		tx_buf.buf = tx_data;
		tx_buf.len = tx_count;
		nv3030b_set_cmd(data, 0);
		spi_write(data->spi_dev, &data->spi_config, &tx_bufs);
	}
}

static void nv3030b_exit_sleep(struct nv3030b_data *data)
{
	nv3030b_transmit(data, NV3030B_CMD_SLEEP_OUT, NULL, 0);
	k_sleep(K_MSEC(200));
}


static void nv3030b_reset_display(struct nv3030b_data *data)
{
	printk("--------display reset------------\n");

	gpio_pin_set(data->reset_gpio, NV3030B_RESET_PIN, 0);
	k_sleep(K_MSEC(10));
	gpio_pin_set(data->reset_gpio, NV3030B_RESET_PIN, 1);
	k_sleep(K_MSEC(200));
	gpio_pin_set(data->reset_gpio, NV3030B_RESET_PIN, 0);
	k_sleep(K_MSEC(200));

}

static int nv3030b_blanking_on(const struct device *dev)
{
	struct nv3030b_data *driver = (struct nv3030b_data *)dev->data;

	nv3030b_transmit(driver, NV3030B_CMD_DISP_OFF, NULL, 0);
	return 0;
}

static int nv3030b_blanking_off(const struct device *dev)
{
	struct nv3030b_data *driver = (struct nv3030b_data *)dev->data;

	nv3030b_transmit(driver, NV3030B_CMD_DISP_ON, NULL, 0);
	return 0;
}

static int nv3030b_read(const struct device *dev,
			const uint16_t x,
			const uint16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	return -ENOTSUP;
}

static void nv3030b_set_mem_area(struct nv3030b_data *data, const uint16_t x,
				 const uint16_t y, const uint16_t w, const uint16_t h)
{
	uint16_t spi_data[2];

	uint16_t ram_x = x + data->x_offset;
	uint16_t ram_y = y + data->y_offset;

	spi_data[0] = sys_cpu_to_be16(ram_x);
	spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	nv3030b_transmit(data, NV3030B_CMD_CASET, (uint8_t *)&spi_data[0], 4);

	spi_data[0] = sys_cpu_to_be16(ram_y);
	spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	nv3030b_transmit(data, NV3030B_CMD_RASET, (uint8_t *)&spi_data[0], 4);
}

static int nv3030b_write(const struct device *dev,
			 const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	struct nv3030b_data *data = (struct nv3030b_data *)dev->data;
	const uint8_t *write_data_start = (uint8_t *) buf;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
	uint16_t write_cnt;
	uint16_t nbr_of_writes;
	uint16_t write_h;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
	__ASSERT((desc->pitch * NV3030B_PIXEL_SIZE * desc->height) <= desc->buf_size,
			"Input buffer to small");

	LOG_DBG("Writing %dx%d (w,h) @ %dx%d (x,y)",
			desc->width, desc->height, x, y);
	nv3030b_set_mem_area(data, x, y, desc->width, desc->height);

	if (desc->pitch > desc->width) {
		write_h = 1U;
		nbr_of_writes = desc->height;
	} else {
		write_h = desc->height;
		nbr_of_writes = 1U;
	}

	nv3030b_transmit(data, NV3030B_CMD_RAMWR,
			 (void *) write_data_start,
			 desc->width * NV3030B_PIXEL_SIZE * write_h);

	tx_bufs.buffers = &tx_buf;
	tx_bufs.count = 1;

	write_data_start += (desc->pitch * NV3030B_PIXEL_SIZE);
	for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
		tx_buf.buf = (void *)write_data_start;
		tx_buf.len = desc->width * NV3030B_PIXEL_SIZE * write_h;
		spi_write(data->spi_dev, &data->spi_config, &tx_bufs);
		write_data_start += (desc->pitch * NV3030B_PIXEL_SIZE);
	}

	return 0;
}

static void *nv3030b_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int nv3030b_set_brightness(const struct device *dev,
			   const uint8_t brightness)
{
	return -ENOTSUP;
}

static int nv3030b_set_contrast(const struct device *dev,
			 const uint8_t contrast)
{
	return -ENOTSUP;
}

static void nv3030b_get_capabilities(const struct device *dev,
			      struct display_capabilities *capabilities)
{
	struct nv3030b_data *data = (struct nv3030b_data *)dev->data;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = data->width;
	capabilities->y_resolution = data->height;

#ifdef CONFIG_NV3030B_RGB565
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
#else
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_888;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_888;
#endif
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int nv3030b_set_pixel_format(const struct device *dev,
			     const enum display_pixel_format pixel_format)
{
#ifdef CONFIG_NV3030B_RGB565
	if (pixel_format == PIXEL_FORMAT_RGB_565) {
#else
	if (pixel_format == PIXEL_FORMAT_RGB_888) {
#endif
		return 0;
	}
	LOG_ERR("Pixel format change not implemented");
	return -ENOTSUP;
}

static int nv3030b_set_orientation(const struct device *dev,
			    const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}
	LOG_ERR("Changing display orientation not implemented");
	return -ENOTSUP;
}


static void nv3030b_lcd_init(struct nv3030b_data *p_nv3030b)
{
	uint8_t tmp;
	nv3030b_set_lcd_margins(p_nv3030b, p_nv3030b->x_offset,
				p_nv3030b->y_offset);
#if ST7789

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_CMD2EN, nv3030b_cmd2en_param,
			 sizeof(nv3030b_cmd2en_param));

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_PORCTRL, nv3030b_porch_param,
			 sizeof(nv3030b_porch_param));

	/* Digital Gamma Enable, default disabled */
	tmp = 0x00;
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_DGMEN, &tmp, 1);

	/* Frame Rate Control in Normal Mode, default value */
	tmp = 0x0f;
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_FRCTRL2, &tmp, 1);

	tmp = DT_INST_PROP(0, gctrl);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_GCTRL, &tmp, 1);

	tmp = DT_INST_PROP(0, vcom);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_VCOMS, &tmp, 1);

#if (DT_INST_NODE_HAS_PROP(0, vrhs) && \
	DT_INST_NODE_HAS_PROP(0, vdvs))
	tmp = 0x01;
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_VDVVRHEN, &tmp, 1);

	tmp = DT_INST_PROP(0, vrhs);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_VRH, &tmp, 1);

	tmp = DT_INST_PROP(0, vdvs);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_VDS, &tmp, 1);
#endif

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_PWCTRL1, nv3030b_pwctrl1_param,
			 sizeof(nv3030b_pwctrl1_param));

	/* Memory Data Access Control */
	tmp = DT_INST_PROP(0, mdac);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_MADCTL, &tmp, 1);

	/* Interface Pixel Format */
	tmp = DT_INST_PROP(0, colmod);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_COLMOD, &tmp, 1);

	tmp = DT_INST_PROP(0, lcm);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_LCMCTRL, &tmp, 1);

	tmp = DT_INST_PROP(0, gamma);
	nv3030b_transmit(p_nv3030b, NV3030B_CMD_GAMSET, &tmp, 1);

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_INV_ON, NULL, 0);

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_PVGAMCTRL, nv3030b_pvgam_param,
			 sizeof(nv3030b_pvgam_param));

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_NVGAMCTRL, nv3030b_nvgam_param,
			 sizeof(nv3030b_nvgam_param));

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_RAMCTRL, nv3030b_ram_param,
			 sizeof(nv3030b_ram_param));

	nv3030b_transmit(p_nv3030b, NV3030B_CMD_RGBCTRL, nv3030b_rgb_param,
			 sizeof(nv3030b_rgb_param));
#else

	uint8_t nv3030b_private_param[] = {0x06, 0x08};
	uint8_t nv3030b_osc1_param[] = {0x06, 0x04};
	uint8_t nv3030b_bias_param[] = {0x00, 0x04, 0x40};
	uint8_t nv3030b_vgl_param[] = {0x41, 0x07, 0x12, 0x12};
	uint8_t nv3030b_vsp_param[] = {0x09, 0x17, 0x21};
	uint8_t nv3030b_vsn_param[] = {0x09, 0x17, 0x21};
	uint8_t nv3030b_neg_param[] = {0x20, 0x40};
	uint8_t nv3030b_gamma_param[] = {0x90, 0x30, 0x22, 0x26};
	uint8_t nv3030b_frame_param[] = {0x0f, 0x02, 0x03};
	uint8_t nv3030b_porch_param[] = {0x02, 0x02, 0x0a, 0x14};
	uint8_t nv3030b_porch1_param[] = {0x04, 0x01, 0x9f, 0x00, 0x02};
	uint8_t nv3030b_gammenew_param[] = {0x00, 0x15, 0x19, 0x37, 0x3b, 0x3f};
	uint8_t nv3030b_gammenew1_param[] = {0x3f, 0x3b, 0x38, 0x1b, 0x17, 0x00};
	uint8_t nv3030b_gammenew2_param[] = {0x2a, 0x72};
	uint8_t nv3030b_gammenew3_param[] = {0x71, 0x2b};
	uint8_t nv3030b_gammenew4_param[] = {0x07, 0x08, 0x0e, 0x11, 0x0f, 0x0a, 0x11, 0x12};
	uint8_t nv3030b_gammenew5_param[] = {0x19, 0x11, 0x15, 0x0f, 0x11, 0x0d, 0x07, 0x08};
	uint8_t nv3030b_gammasou_param[] = {0x00, 0xff};
	uint8_t nv3030b_gammasou1_param[] = {0x01, 0x04, 0x03, 0x03, 0x00, 0x12};
	uint8_t nv3030b_gammasou2_param[] = {0x00, 0x70, 0x00};
	uint8_t nv3030b_gate2_param[] = {0xfa, 0xfc};

	nv3030b_transmit(p_nv3030b, 0Xfd, nv3030b_private_param,
			 sizeof(nv3030b_private_param));

	nv3030b_transmit(p_nv3030b, 0x61, nv3030b_osc1_param,
			 sizeof(nv3030b_osc1_param));

	nv3030b_transmit(p_nv3030b, 0x62, nv3030b_bias_param,
			 sizeof(nv3030b_bias_param));

	nv3030b_transmit(p_nv3030b, 0x63, nv3030b_vgl_param,
			 sizeof(nv3030b_vgl_param));

	nv3030b_transmit(p_nv3030b, 0x65, nv3030b_vsp_param,
			 sizeof(nv3030b_vsp_param));

	nv3030b_transmit(p_nv3030b, 0x66, nv3030b_vsn_param,
			 sizeof(nv3030b_vsn_param));

	nv3030b_transmit(p_nv3030b, 0x67, nv3030b_neg_param,
			 sizeof(nv3030b_neg_param));
	
	nv3030b_transmit(p_nv3030b, 0x68, nv3030b_gamma_param,
			 sizeof(nv3030b_gamma_param));
	
	nv3030b_transmit(p_nv3030b, 0xb1, nv3030b_frame_param,
			 sizeof(nv3030b_frame_param));
	
	tmp = 0x01;
 	nv3030b_transmit(p_nv3030b, 0xb4, &tmp, 1);
	
	nv3030b_transmit(p_nv3030b, 0xb5, nv3030b_porch_param,
			 sizeof(nv3030b_porch_param));
	
	nv3030b_transmit(p_nv3030b, 0xb6, nv3030b_porch1_param,
			 sizeof(nv3030b_porch1_param));
	
	tmp = 0x11;
	nv3030b_transmit(p_nv3030b, 0xdf, &tmp, 1);

	nv3030b_transmit(p_nv3030b, 0xe2, nv3030b_gammenew_param,
			 sizeof(nv3030b_gammenew_param));
	
	nv3030b_transmit(p_nv3030b, 0xe5, nv3030b_gammenew1_param,
			 sizeof(nv3030b_gammenew1_param));
	
	nv3030b_transmit(p_nv3030b, 0xe1, nv3030b_gammenew2_param,
			 sizeof(nv3030b_gammenew2_param));
	
	nv3030b_transmit(p_nv3030b, 0xe4, nv3030b_gammenew3_param,
			 sizeof(nv3030b_gammenew3_param));
	
	nv3030b_transmit(p_nv3030b, 0xe0, nv3030b_gammenew4_param,
			 sizeof(nv3030b_gammenew4_param));
	
	nv3030b_transmit(p_nv3030b, 0xe3, nv3030b_gammenew5_param,
			 sizeof(nv3030b_gammenew5_param));

	nv3030b_transmit(p_nv3030b, 0xe6, nv3030b_gammasou_param,
			 sizeof(nv3030b_gammasou_param));
	
	nv3030b_transmit(p_nv3030b, 0xe7, nv3030b_gammasou1_param,
			 sizeof(nv3030b_gammasou1_param));
	
	nv3030b_transmit(p_nv3030b, 0xe8, nv3030b_gammasou2_param,
			 sizeof(nv3030b_gammasou2_param));

	tmp = 0x50;
	nv3030b_transmit(p_nv3030b, 0xec, &tmp, 1);
	
	tmp = 0x00;
	nv3030b_transmit(p_nv3030b, 0xf1, &tmp, 1);
	
	nv3030b_transmit(p_nv3030b, 0xfd, nv3030b_gate2_param,
			 sizeof(nv3030b_gate2_param));

	tmp = 0x55;
	nv3030b_transmit(p_nv3030b, 0x3a, &tmp, 1);
	
	tmp = 0x00;
	nv3030b_transmit(p_nv3030b, 0x35, &tmp, 1);
	
	tmp = 0xa0;
	nv3030b_transmit(p_nv3030b, 0x36, &tmp, 1);

	nv3030b_transmit(p_nv3030b, 0x21, NULL, 0);
	
	nv3030b_transmit(p_nv3030b, 0x11, NULL, 0);
	k_sleep(K_MSEC(200));
	
	nv3030b_transmit(p_nv3030b, 0x29, NULL, 0);
	k_sleep(K_MSEC(10));

#endif
	printk("----------init end-----------\n");

	
}

static int nv3030b_init(const struct device *dev)
{
	struct nv3030b_data *data = (struct nv3030b_data *)dev->data;

	data->spi_dev = device_get_binding(DT_INST_BUS_LABEL(0));
	if (data->spi_dev == NULL) {
		LOG_ERR("Could not get SPI device for LCD");
		return -EPERM;
	}

	data->spi_config.frequency =
		DT_INST_PROP(0, spi_max_frequency);
	data->spi_config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
	data->spi_config.slave = DT_INST_REG_ADDR(0);

#if DT_INST_SPI_DEV_HAS_CS_GPIOS(0)
	data->cs_ctrl.gpio_dev = device_get_binding(
			DT_INST_SPI_DEV_CS_GPIOS_LABEL(0));
	data->cs_ctrl.gpio_pin = DT_INST_SPI_DEV_CS_GPIOS_PIN(0);
	data->cs_ctrl.gpio_dt_flags = DT_INST_SPI_DEV_CS_GPIOS_FLAGS(0);
	data->cs_ctrl.delay = 0U;
	data->spi_config.cs = &(data->cs_ctrl);
#else
	data->spi_config.cs = NULL;
#endif

#if DT_INST_NODE_HAS_PROP(0, reset_gpios)
	data->reset_gpio = device_get_binding(
			DT_INST_GPIO_LABEL(0, reset_gpios));
	if (data->reset_gpio == NULL) {
		LOG_ERR("Could not get GPIO port for display reset");
		return -EPERM;
	}

	if (gpio_pin_configure(data->reset_gpio, NV3030B_RESET_PIN,
			       GPIO_OUTPUT_INACTIVE | NV3030B_RESET_FLAGS)) {
		LOG_ERR("Couldn't configure reset pin");
		return -EIO;
	}
#endif

	data->cmd_data_gpio = device_get_binding(
			DT_INST_GPIO_LABEL(0, cmd_data_gpios));
	if (data->cmd_data_gpio == NULL) {
		LOG_ERR("Could not get GPIO port for cmd/DATA port");
		return -EPERM;
	}
	if (gpio_pin_configure(data->cmd_data_gpio, NV3030B_CMD_DATA_PIN,
			       GPIO_OUTPUT | NV3030B_CMD_DATA_FLAGS)) {
		LOG_ERR("Couldn't configure cmd/DATA pin");
		return -EIO;
	}

	nv3030b_reset_display(data);

	#if ST7789
	printk("-------------ST7789--------------\n");
	nv3030b_blanking_on(dev);
	#endif
	
	nv3030b_lcd_init(data);

	#if ST7789
	nv3030b_exit_sleep(data);
	#endif
	
	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int nv3030b_pm_control(const struct device *dev,
			      enum pm_device_action action)
{
	struct nv3030b_data *data = (struct nv3030b_data *)dev->data;
	int ret = 0;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		nv3030b_exit_sleep(data);
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		ret = nv3030b_transmit(data, NV3030B_CMD_SLEEP_IN, NULL, 0);
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}
#endif /* CONFIG_PM_DEVICE */

static const struct display_driver_api nv3030b_api = {
	.blanking_on = nv3030b_blanking_on,
	.blanking_off = nv3030b_blanking_off,
	.write = nv3030b_write,
	.read = nv3030b_read,
	.get_framebuffer = nv3030b_get_framebuffer,
	.set_brightness = nv3030b_set_brightness,
	.set_contrast = nv3030b_set_contrast,
	.get_capabilities = nv3030b_get_capabilities,
	.set_pixel_format = nv3030b_set_pixel_format,
	.set_orientation = nv3030b_set_orientation,
};

static struct nv3030b_data nv3030b_data = {
	.width = DT_INST_PROP(0, width),
	.height = DT_INST_PROP(0, height),
	.x_offset = DT_INST_PROP(0, x_offset),
	.y_offset = DT_INST_PROP(0, y_offset),
};

DEVICE_DT_INST_DEFINE(0, &nv3030b_init,
	      nv3030b_pm_control, &nv3030b_data, NULL, POST_KERNEL,
	      CONFIG_DISPLAY_INIT_PRIORITY, &nv3030b_api);
