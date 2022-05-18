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

#include <device.h>
#include <drivers/spi.h>
#include <drivers/gpio.h>
#include <sys/byteorder.h>
#include <drivers/display.h>
#include <drivers/pinmux.h>
#include <drivers/pwm.h>


#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(display_st7789v);

// LOG_MODULE_REGISTER(display_st7789v, LOG_LEVEL_DBG);

static struct k_sem te_lock;
static int te_idle = 0;

#define ST7789V_CS_PIN		DT_INST_SPI_DEV_CS_GPIOS_PIN(0)
#define ST7789V_CMD_DATA_PIN	DT_INST_GPIO_PIN(0, cmd_data_gpios)
#define ST7789V_CMD_DATA_FLAGS	DT_INST_GPIO_FLAGS(0, cmd_data_gpios)
#define ST7789V_RESET_PIN	DT_INST_GPIO_PIN(0, reset_gpios)
#define ST7789V_RESET_FLAGS	DT_INST_GPIO_FLAGS(0, reset_gpios)


#define TE_NODE 		DT_NODELABEL(tearing)
#define GPIO_TE			DT_GPIO_CTLR(TE_NODE, gpios) 
#define PIN_TE			DT_GPIO_PIN(TE_NODE, gpios) 
#define FLAGS_TE		DT_GPIO_FLAGS(TE_NODE, gpios) 



// devicetree: LCD PWM
struct pwm_csk_config {
	uint8_t pwm_pin;
	uint8_t pwm_func;
	uint8_t clock_source;
	uint8_t clock_prescaler;
	uint8_t mode;
	const struct device *pinmux_drv;
};

// PWM SWDIO
#define PWM_LED0_NODE	DT_NODELABEL(pwmlcd)
#define PINMUXA_NODE 	DT_NODELABEL(pinmuxa)
#define SWDIO_PIN_NUM 	1
#define SWDIO_PIN_FUNC 	0
#define PWM1_PIN_FUNC 	11

// devicetree: power lock
#define PWR_LOCK_NODE 	DT_NODELABEL(pwrlock)
#define GPIO_PWR_LOCK	DT_GPIO_CTLR(PWR_LOCK_NODE, gpios) 
#define PIN_PWR_LOCK	DT_GPIO_PIN(PWR_LOCK_NODE, gpios) 
#define FLAGS_PWR_LOCK	DT_GPIO_FLAGS(PWR_LOCK_NODE, gpios) 




static uint8_t st7789v_porch_param[] = DT_INST_PROP(0, porch_param);
static uint8_t st7789v_cmd2en_param[] = DT_INST_PROP(0, cmd2en_param);
static uint8_t st7789v_pwctrl1_param[] = DT_INST_PROP(0, pwctrl1_param);
static uint8_t st7789v_pvgam_param[] = DT_INST_PROP(0, pvgam_param);
static uint8_t st7789v_nvgam_param[] = DT_INST_PROP(0, nvgam_param);
static uint8_t st7789v_ram_param[] = DT_INST_PROP(0, ram_param);
static uint8_t st7789v_rgb_param[] = DT_INST_PROP(0, rgb_param);
static uint8_t st7789v_ste_param[] = DT_INST_PROP(0, ste_param);

struct st7789v_data {
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
	uint16_t frctrl2;
	uint16_t teon;
	uint16_t mdac;
	uint16_t vdvvrhen;
	uint16_t orientation;

};

#ifdef CONFIG_ST7789V_RGB565
#define ST7789V_PIXEL_SIZE 2u
#else
#define ST7789V_PIXEL_SIZE 3u
#endif



enum PIN_FUNC{
	PWM1_FUNC,
	SWD_FUNC
};
void pin_set_func(enum PIN_FUNC func){
	const struct device *pinmuxa = DEVICE_DT_GET(PINMUXA_NODE);
	if (!device_is_ready(pinmuxa)) {
		LOG_ERR("pinmuxa device is not ready");
		return;
	}
	switch (func)
	{
	case PWM1_FUNC:
		pinmux_pin_set(pinmuxa, SWDIO_PIN_NUM, PWM1_PIN_FUNC);
		break;
	case SWD_FUNC:
		pinmux_pin_set(pinmuxa, SWDIO_PIN_NUM, SWDIO_PIN_FUNC);
		break;
	default:
		break;
	}
}



static void st7789v_set_lcd_margins(struct st7789v_data *data,
			     uint16_t x_offset, uint16_t y_offset)
{
	data->x_offset = x_offset;
	data->y_offset = y_offset;
}

static void st7789v_set_cmd(struct st7789v_data *data, int is_cmd)
{
	gpio_pin_set(data->cmd_data_gpio, ST7789V_CMD_DATA_PIN, is_cmd);
}

static void st7789v_transmit(struct st7789v_data *data, uint8_t cmd,
		uint8_t *tx_data, size_t tx_count)
{
	struct spi_buf tx_buf = { .buf = &cmd, .len = 1 };
	struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

	st7789v_set_cmd(data, 1);
	spi_write(data->spi_dev, &data->spi_config, &tx_bufs);

	if (tx_data != NULL) {
		tx_buf.buf = tx_data;
		tx_buf.len = tx_count;
		st7789v_set_cmd(data, 0);
		spi_write(data->spi_dev, &data->spi_config, &tx_bufs);
	}
}

static void st7789v_exit_sleep(struct st7789v_data *data)
{
	st7789v_transmit(data, ST7789V_CMD_SLEEP_OUT, NULL, 0);
	k_sleep(K_MSEC(120));
}

static void st7789v_reset_display(struct st7789v_data *data)
{
	LOG_DBG("Resetting display");
#if DT_INST_NODE_HAS_PROP(0, reset_gpios)
	k_sleep(K_MSEC(1));
	gpio_pin_set(data->reset_gpio, ST7789V_RESET_PIN, 1);
	k_sleep(K_MSEC(6));
	gpio_pin_set(data->reset_gpio, ST7789V_RESET_PIN, 0);
	k_sleep(K_MSEC(20));
#else
	st7789v_transmit(p_st7789v, ST7789V_CMD_SW_RESET, NULL, 0);
	k_sleep(K_MSEC(5));
#endif
}

static int st7789v_blanking_on(const struct device *dev)
{
	struct st7789v_data *driver = (struct st7789v_data *)dev->data;

	st7789v_transmit(driver, ST7789V_CMD_DISP_OFF, NULL, 0);
	return 0;
}

static int st7789v_blanking_off(const struct device *dev)
{
	struct st7789v_data *driver = (struct st7789v_data *)dev->data;

	st7789v_transmit(driver, ST7789V_CMD_DISP_ON, NULL, 0);
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

static void st7789v_set_mem_area(struct st7789v_data *data, const uint16_t x,
				 const uint16_t y, const uint16_t w, const uint16_t h)
{
	uint16_t spi_data[2];

	uint16_t ram_x = x + data->x_offset;
	uint16_t ram_y = y + data->y_offset;

	spi_data[0] = sys_cpu_to_be16(ram_x);
	spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	st7789v_transmit(data, ST7789V_CMD_CASET, (uint8_t *)&spi_data[0], 4);

	spi_data[0] = sys_cpu_to_be16(ram_y);
	spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	st7789v_transmit(data, ST7789V_CMD_RASET, (uint8_t *)&spi_data[0], 4);
}

static int _st7789v_write(const struct device *dev,
			 const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	struct st7789v_data *data = (struct st7789v_data *)dev->data;
	const uint8_t *write_data_start = (uint8_t *) buf;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
	uint16_t write_cnt;
	uint16_t nbr_of_writes;
	uint16_t write_h;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
	__ASSERT((desc->pitch * ST7789V_PIXEL_SIZE * desc->height) <= desc->buf_size,
			"Input buffer to small");

	LOG_DBG("Writing %dx%d (w,h) @ %dx%d (x,y)",
			desc->width, desc->height, x, y);
	st7789v_set_mem_area(data, x, y, desc->width, desc->height);

	if (desc->pitch > desc->width) {
		write_h = 1U;
		nbr_of_writes = desc->height;
	} else {
		write_h = desc->height;
		nbr_of_writes = 1U;
	}

	st7789v_transmit(data, ST7789V_CMD_RAMWR,
			 (void *) write_data_start,
			 desc->width * ST7789V_PIXEL_SIZE * write_h);

	tx_bufs.buffers = &tx_buf;
	tx_bufs.count = 1;

	write_data_start += (desc->pitch * ST7789V_PIXEL_SIZE);
	for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
		tx_buf.buf = (void *)write_data_start;
		tx_buf.len = desc->width * ST7789V_PIXEL_SIZE * write_h;
		spi_write(data->spi_dev, &data->spi_config, &tx_bufs);
		write_data_start += (desc->pitch * ST7789V_PIXEL_SIZE);
	}

	return 0;
}

void te_interrupt_cb(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins){
	te_idle = gpio_pin_get(dev, PIN_TE);
	if (te_idle) {
		k_sem_give(&te_lock);
	}
}

int init_te(void){
    static struct gpio_callback te_cb_data;

	k_sem_init(&te_lock, 0, 1);

	const struct device *te = DEVICE_DT_GET(GPIO_TE);
	if (!device_is_ready(te)) {
		printk("Error: te device %s is not ready", te->name);
		return -1;
	}
	int ret = gpio_pin_configure(te, PIN_TE, GPIO_INPUT | 0);
   	if (ret < 0) {
		printk("Error: power_lock configure %d", ret);
		return ret;
	}
    ret = gpio_pin_interrupt_configure(te, PIN_TE, GPIO_INT_EDGE_BOTH);
	if (ret) {
		return ret;
	}

	gpio_init_callback(&te_cb_data, te_interrupt_cb, BIT(PIN_TE));
	gpio_add_callback(te, &te_cb_data);
    return 1;
}

static int st7789v_write(const struct device *dev,
			 const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	if (!te_idle) {
		k_sem_take(&te_lock, K_FOREVER);
	}
	_st7789v_write(dev, x, y, desc, buf);
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

static int st7789v_set_orientation(struct st7789v_data *p_st7789v,
				   const enum display_orientation orientation)
{
	// struct st7789v_data *data = dev->data;

	uint8_t tx_data = 0;
	printk("orientation:%d", orientation);
	// uint8_t tx_data = ST7789V_MADCTL_BGR;

	// if (orientation == DISPLAY_ORIENTATION_NORMAL) {
	// 	tx_data |= ST7789V_MADCTL_MX;
	// } else if (orientation == DISPLAY_ORIENTATION_ROTATED_90) {
	// 	tx_data |= ST7789V_MADCTL_MV;
	// } else if (orientation == DISPLAY_ORIENTATION_ROTATED_180) {
	// 	tx_data |= ST7789V_MADCTL_MY;
	// } else if (orientation == DISPLAY_ORIENTATION_ROTATED_270) {
	// 	tx_data |= ST7789V_MADCTL_MV | ST7789V_MADCTL_MX |
	// 		   ST7789V_MADCTL_MY;
	// }

	// tx_data |= ST7789V_MADCTL_MY | ST7789V_MADCTL_MV ;
	// tx_data |= ST7789V_MADCTL_MX | ST7789V_MADCTL_MY | ST7789V_MADCTL_MV | ST7789V_MADCTL_ML;
	// tx_data |= ST7789V_MADCTL_MX | ST7789V_MADCTL_MV;
	// tx_data |= ST7789V_MADCTL_MV;

	// st7789v_transmit(p_st7789v, ST7789V_CMD_MADCTL, &tx_data, 1U);

	// p_st7789v->orientation = orientation;

	return 0;
}

static void st7789v_get_capabilities(const struct device *dev,
			      struct display_capabilities *capabilities)
{
	struct st7789v_data *data = (struct st7789v_data *)dev->data;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = data->width;
	capabilities->y_resolution = data->height;

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

static void st7789v_lcd_init(struct st7789v_data *p_st7789v)
{
	uint8_t tmp;

	st7789v_set_lcd_margins(p_st7789v, p_st7789v->x_offset,
				p_st7789v->y_offset);

	st7789v_transmit(p_st7789v, ST7789V_CMD_CMD2EN, st7789v_cmd2en_param,
			 sizeof(st7789v_cmd2en_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_PORCTRL, st7789v_porch_param,
			 sizeof(st7789v_porch_param));

	/* Digital Gamma Enable, default disabled */
	tmp = 0x00;
	st7789v_transmit(p_st7789v, ST7789V_CMD_DGMEN, &tmp, 1);

	/* Frame Rate Control in Normal Mode, default value */
	// tmp = 0x0f;
	// st7789v_transmit(p_st7789v, ST7789V_CMD_FRCTRL2, &tmp, 1);

	/* FRCTRL2 (C6h): Frame Rate Control in Normal Mode */
	// static uint8_t hh= 0x1E;
	// tmp = DT_INST_PROP(0, frctrl2);
	st7789v_transmit(p_st7789v, ST7789V_CMD_FRCTRL2, &(p_st7789v->frctrl2), 1);

	tmp = DT_INST_PROP(0, gctrl);
	st7789v_transmit(p_st7789v, ST7789V_CMD_GCTRL, &tmp, 1);

	tmp = DT_INST_PROP(0, vcom);
	st7789v_transmit(p_st7789v, ST7789V_CMD_VCOMS, &tmp, 1);

#if (DT_INST_NODE_HAS_PROP(0, vrhs) && \
	DT_INST_NODE_HAS_PROP(0, vdvs))
	tmp = 0x01;
	st7789v_transmit(p_st7789v, ST7789V_CMD_VDVVRHEN, &tmp, 1);

	tmp = DT_INST_PROP(0, vrhs);
	st7789v_transmit(p_st7789v, ST7789V_CMD_VRH, &tmp, 1);

	tmp = DT_INST_PROP(0, vdvs);
	st7789v_transmit(p_st7789v, ST7789V_CMD_VDS, &tmp, 1);
#endif

	// st7789v_transmit(p_st7789v, ST7789V_CMD_PWCTRL1, st7789v_pwctrl1_param,
	// 		 sizeof(st7789v_pwctrl1_param));

	/* Memory Data Access Control */
	// tmp = DT_INST_PROP(0, mdac);
	st7789v_transmit(p_st7789v, ST7789V_CMD_MADCTL, &(p_st7789v->mdac), 1);

	st7789v_set_orientation(p_st7789v, p_st7789v->orientation);


	/* VDV and VRH Command Enable */
	// tmp = DT_INST_PROP(0, vdvvrhen);
	st7789v_transmit(p_st7789v, ST7789V_CMD_VDVVRHEN, &(p_st7789v->vdvvrhen), 1);

	

	/* TEON (35h): Tearing Effect Line On */
	// tmp = DT_INST_PROP(0, teon);
	st7789v_transmit(p_st7789v, ST7789V_CMD_TEON, &(p_st7789v->teon), 1);

	/* STE (44h): Set Tear Scanline */
	st7789v_transmit(p_st7789v, ST7789V_CMD_STE, st7789v_ste_param,
			 sizeof(st7789v_ste_param));

	/* Interface Pixel Format */
	tmp = DT_INST_PROP(0, colmod);
	st7789v_transmit(p_st7789v, ST7789V_CMD_COLMOD, &tmp, 1);

	tmp = DT_INST_PROP(0, lcm);
	st7789v_transmit(p_st7789v, ST7789V_CMD_LCMCTRL, &tmp, 1);

	tmp = DT_INST_PROP(0, gamma);
	st7789v_transmit(p_st7789v, ST7789V_CMD_GAMSET, &tmp, 1);

	st7789v_transmit(p_st7789v, ST7789V_CMD_INV_ON, NULL, 0);

	st7789v_transmit(p_st7789v, ST7789V_CMD_PVGAMCTRL, st7789v_pvgam_param,
			 sizeof(st7789v_pvgam_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_NVGAMCTRL, st7789v_nvgam_param,
			 sizeof(st7789v_nvgam_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_RAMCTRL, st7789v_ram_param,
			 sizeof(st7789v_ram_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_RGBCTRL, st7789v_rgb_param,
			 sizeof(st7789v_rgb_param));
}

static int st7789v_init(const struct device *dev)
{
	struct st7789v_data *data = (struct st7789v_data *)dev->data;
	const struct device *cmd_pinmux_drv, *reset_pinmux_drv;

	data->spi_dev = device_get_binding(DT_INST_BUS_LABEL(0));
	if (data->spi_dev == NULL) {
		LOG_ERR("Could not get SPI device for LCD");
		return -EPERM;
	}

	data->spi_config.frequency =
		DT_INST_PROP(0, spi_max_frequency);
	data->spi_config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
	data->spi_config.slave = DT_INST_REG_ADDR(0);

	// use hardware spi_cs
	data->spi_config.cs = NULL;


	pin_set_func(PWM1_FUNC);
	int ret;
	const struct device *pwm = DEVICE_DT_GET(PWM_LED0_NODE);
	ret = pwm_pin_set_nsec(pwm, 1, 20000,  200, 1);


#if DT_INST_NODE_HAS_PROP(0, pinmuxs)

	char *name = DT_LABEL(DT_INST_PHANDLE_BY_IDX(0, pinmuxs, 0));     
	cmd_pinmux_drv = device_get_binding(name);
	if (!cmd_pinmux_drv) {
		return -EINVAL;
	}
	uint8_t num = DT_PHA_BY_IDX(DT_DRV_INST(0), pinmuxs, 0, pin);
	uint8_t func = DT_PHA_BY_IDX(DT_DRV_INST(0), pinmuxs, 0, function);
	pinmux_pin_set(cmd_pinmux_drv, num, func);
	
	name = DT_LABEL(DT_INST_PHANDLE_BY_IDX(0, pinmuxs, 1));               
	reset_pinmux_drv = device_get_binding(name);
	if (!reset_pinmux_drv) {
		return -EINVAL;
	}
	num = DT_PHA_BY_IDX(DT_DRV_INST(0), pinmuxs, 1, pin);
	func = DT_PHA_BY_IDX(DT_DRV_INST(0), pinmuxs, 1, function);
	pinmux_pin_set(reset_pinmux_drv, num, func);
#endif

#if DT_INST_NODE_HAS_PROP(0, reset_gpios)
	data->reset_gpio = device_get_binding(
			DT_INST_GPIO_LABEL(0, reset_gpios));
	if (data->reset_gpio == NULL) {
		LOG_ERR("Could not get GPIO port for display reset");
		return -EPERM;
	}

	if (gpio_pin_configure(data->reset_gpio, ST7789V_RESET_PIN,
			       GPIO_OUTPUT_INACTIVE | ST7789V_RESET_FLAGS)) {
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
	if (gpio_pin_configure(data->cmd_data_gpio, ST7789V_CMD_DATA_PIN,
			       GPIO_OUTPUT | ST7789V_CMD_DATA_FLAGS)) {
		LOG_ERR("Couldn't configure cmd/DATA pin");
		return -EIO;
	}

	st7789v_reset_display(data);

	st7789v_blanking_on(dev);

	st7789v_lcd_init(data);

	st7789v_exit_sleep(data);

	init_te();

	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int st7789v_pm_control(const struct device *dev,
			      enum pm_device_action action)
{
	struct st7789v_data *data = (struct st7789v_data *)dev->data;
	int ret = 0;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		st7789v_exit_sleep(data);
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		ret = st7789v_transmit(data, ST7789V_CMD_SLEEP_IN, NULL, 0);
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

static struct st7789v_data st7789v_data = {
	.width = DT_INST_PROP(0, width),
	.height = DT_INST_PROP(0, height),
	.x_offset = DT_INST_PROP(0, x_offset),
	.y_offset = DT_INST_PROP(0, y_offset),
	.frctrl2 = DT_INST_PROP(0, frctrl2),
	.teon = DT_INST_PROP(0, teon),
	.vdvvrhen = DT_INST_PROP(0, vdvvrhen),
	.mdac = DT_INST_PROP(0, mdac),
	.orientation = DT_INST_PROP(0, orientation),
	
	
};

DEVICE_DT_INST_DEFINE(0, &st7789v_init,
	      st7789v_pm_control, &st7789v_data, NULL, POST_KERNEL,
	      CONFIG_DISPLAY_INIT_PRIORITY, &st7789v_api);
