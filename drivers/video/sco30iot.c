/*
 * Copyright (c) 2022, listenai
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT smartsens_sco30iot

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <drivers/video.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(sco30iot, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief  SCO30IOT SENSOR ID
 */
// 数据手册中的 CHIP ID 为 0x8A46，实测为 0x9A46
#define SCO30IOT_SENSOR_ID		0x9A46
#define SC301IOT_CHIP_ID_REG_H		0xF7
#define SC301IOT_CHIP_ID_REG_L		0xF8

#define SC301IOT_REG_VALUE_08BIT	1
#define SC301IOT_REG_VALUE_16BIT	2
#define SC301IOT_REG_VALUE_24BIT	3

/**
 * @brief  SCO30IOT SENSOR REG
 */
#define PAGE_BANK_SEL			0xF0

// 画面镜像和翻转
#define PAGE_BANK_REG_CID_MIRROR		0x3221	// default mirror off
#define PAGE_BANK_REG_CID_FLIP			0x3221	// default flip off

#define REG_CID_MIRROR_OFF_VALUE		0x00	// 0x3221,bit[2:1],2'b00 mirror off,2'b11 mirror on
#define REG_CID_MIRROR_ON_VALUE			0x06	// 0x3221,bit[2:1],2'b00 mirror off,2'b11 mirror on
#define REG_CID_FLIP_OFF_VALUE			0x00	// 0x3221,bit[6:5],2'b00 flip off,2'b11 flip off
#define REG_CID_FLIP_ON_VALUE			0x60	// 0x3221,bit[6:5],2'b00 flip off,2'b11 flip off

// Color bar 模式
#define PAGE_BANK_REG_COLOR_BAR			0x0100	// color bar
#define PAGE_BANK_REG_COLOR_BAR_EN		0x80	// 0x0100, bit[7] color bar enable, 1: enable, 0: disable
#define PAGE_BANK_REG_COLOR_BAR_R_VALUE		0x0101	// define color bar R value
#define PAGE_BANK_REG_COLOR_BAR_G_VALUE		0x0102	// define color bar G value
#define PAGE_BANK_REG_COLOR_BAR_B_VALUE		0x0103	// define color bar B value

// GAMMA 配置
#define PAGE_BANK_REG_GAMMA_EN			0x00F5	// gamma enable, 0: enable, 1: disable
#define PAGE_BANK_REG_GAMMA_EP1			0x0041	//  EP1: gamma coefficient1
#define PAGE_BANK_REG_GAMMA_EP2			0x0042	//  EP2: gamma coefficient2
#define PAGE_BANK_REG_GAMMA_EP3			0x0043	//  EP3: gamma coefficient3
#define PAGE_BANK_REG_GAMMA_EP4			0x0044	//  EP4: gamma coefficient4
#define PAGE_BANK_REG_GAMMA_EP5			0x0045	//  EP5: gamma coefficient5
#define PAGE_BANK_REG_GAMMA_EP6			0x0046	//  EP6: gamma coefficient6
#define PAGE_BANK_REG_GAMMA_EP7			0x0047	//  EP7: gamma coefficient7
#define PAGE_BANK_REG_GAMMA_EP8			0x0048	//  EP8: gamma coefficient8
#define PAGE_BANK_REG_GAMMA_EP9			0x0049	//  EP9: gamma coefficient9
#define PAGE_BANK_REG_GAMMA_EP10		0x004A	//  EP10: gamma coefficient10
#define PAGE_BANK_REG_GAMMA_EP11		0x004B	//  EP11: gamma coefficient11
#define PAGE_BANK_REG_GAMMA_EP12		0x004C	//  EP12: gamma coefficient12
#define PAGE_BANK_REG_GAMMA_EP13		0x004D	//  EP13: gamma coefficient13
#define PAGE_BANK_REG_GAMMA_EP14		0x004E	//  EP14: gamma coefficient14
#define PAGE_BANK_REG_GAMMA_EP15		0x004F	//  EP15: gamma coefficient15

// 坏点去除
#define PAGE_BANK_REG_BAD_PIXEL_CANCELLATION_SIGNLE		0x00E0
#define REG_BAD_PIXEL_CANCELLATION_SIGNLE_EN			0x04	// 0x00E0,bit[2] single bad pixel cancellation enable, 1: enable, 0: disable
#define REG_BAD_PIXEL_CANCELLATION_BRIGHT_COUPLET_EN		0x08	// 0x00E0,bit[3] bright couplet bad pixel cancellation enable, 1: enable, 0: disable
#define REG_BAD_PIXEL_CANCELLATION_DARK_COUPLET_EN		0x20	// 0x00E0,bit[5] dark couplet bad pixel cancellation enable, 1: enable, 0: disable

#define PAGE_BANK_REG_BAD_PIXEL_CANCELLATION_BIG		0x00E2
#define REG_BAD_PIXEL_CANCELLATION_BIG_CLUSTER_EN		0x08	// 0x00E2,bit[3] big cluster bad pixel cancellation enable, 1: enable, 0: disable

// 手动曝光&全局增益
#define PAGE_BANK_REG_EXPOSURE_GAIN				0x0070	// AEC/AGC enable
#define REG_EXPOSURE_GAIN_AEC_REG_EN				0x01	// 0x0070,bit[0] mannual exposure AEC enable, 1: auto, 0: mannual
#define REG_EXPOSURE_GAIN_AGC_REG_EN				0x02	// 0x0070,bit[1] mannual exposure value

#define PAGE_BANK_REG_EXPOSURE_GLOBAL_GAIN			0x0072	// Exposure global gain
#define REG_EXPOSURE_GLOBAL_GAIN_AGC_EN				0x80	// 0x0072,bit[7] Global gain and digital gain control 1: independent; 0: non independent

// 以半行为单位，最小值为 1，最大值 为 2*{16'h320e,16'h320f} - 'd8，调节 步长为 1
#define REG_EXPOSURE_REG_EXPOSURE_TIME_L			0x008D	// {16'h008d}
#define REG_EXPOSURE_REG_EXPOSURE_TIME_H			0x008E	// {16'h008e}

#define REG_EXPOSURE_GLOBAL_GAIN_TIME_L				0x008B	// {16'h008b[3:0]}
#define REG_EXPOSURE_GLOBAL_GAIN_TIME_H				0x008C	// {16'h008c}

#define PAGE_BANK_REG_EXPOSURE_DIGITAL_GAIN			0x009E	// Exposure digital gain

// 设置输出窗口大小
#define PAGE_BANK_REG_WINDOW_WIDTH_L				0x3206	// Output window width low 8 bits
#define PAGE_BANK_REG_WINDOW_WIDTH_H				0x3207	// Output window width high 8 bits
#define PAGE_BANK_REG_WINDOW_HEIGHT_L				0x3208	// Output window height low 8 bits
#define PAGE_BANK_REG_WINDOW_HEIGHT_H				0x3209	// Output window height high 8 bits


struct sco30iot_data {
	const struct i2c_dt_spec i2c_bus;
	const struct gpio_dt_spec reset_gpios;
	const struct gpio_dt_spec pwdn_gpios;
	struct video_format fmt;
};

struct sco30iot_reg {
	uint8_t addr;
	uint8_t value;
};

/* 25M CLK & VGA & YUV(UYVY) & 15fps */
static const struct sco30iot_reg sco30iot_init_reg_tb[] = {
	//*24M//
	{PAGE_BANK_SEL, 0x30},
	{0x01, 0xff},
	{0x02, 0xff},
	{0x22, 0x07},
	{0x19, 0xff},
	{0x3f, 0x82},
	{0x30, 0x02},

	// 0xF0, 0x0177, DVP-PCLK 信号极性
	{PAGE_BANK_SEL, 0x01},
	{0x70, 0x00},
	{0x71, 0x80},
	{0x72, 0x20},
	{0x73, 0x00},
	{0x74, 0xe0},
	{0x75, 0x10},
	{0x76, 0x81},
#if defined(CONFIG_SCO30IOT_FPS_20)
	{0x77, 0xb8}, //  pclk极性 bit【2】   yuv bitbit[45] 8c之前的数值
#elif defined(CONFIG_SCO30IOT_FPS_15) || defined(CONFIG_SCO30IOT_FPS_30) 
	{0x77, 0x8c}, //  pclk极性 bit【2】
#endif
	{0x78, 0xe1},
	{0x79, 0x31},
	{0xf5, 0x01},
	{0xf4, 0x0a},
	// 0xF0, 0x3637
	{PAGE_BANK_SEL, 0x36},
	{0x37, 0x79},
	{0xea, 0x09},
	{0x31, 0x82},
	{0x3e, 0x60},
	{0x30, 0xf0},
	{0x33, 0x33},
	
	{PAGE_BANK_SEL, 0x32},
	{0x48, 0x02},

	{PAGE_BANK_SEL, 0x33},
	{0x02, 0x12},
	{0x7c, 0x02},
	{0x7d, 0x0e},
	{0xa2, 0x04},
	{0x5e, 0x06},
	{0x5f, 0x0a},
	{0x0b, 0x58},
	{0x06, 0x38},

	{PAGE_BANK_SEL, 0x32},
	{0x48, 0x02},
	{0xf0, 0x39},
	{0x02, 0x70},
	{0xf0, 0x45},
	{0x09, 0x1c},

	{0xf0, 0x37},
	{0x22, 0x0d},
	{PAGE_BANK_SEL, 0x33},
	{0x33, 0x10},
	{0xb1, 0x80},
	{0x34, 0x40},
	{0x0b, 0x54},
	{0xb2, 0x78},
	{0xf0, 0x36},
	{0x11, 0x80},
	{0xf0, 0x30},
	{0x38, 0x44},
	{0xf0, 0x33},
	{0xb3, 0x51},
	{0x01, 0x10},
	{0x0b, 0x6c},
	{0x06, 0x24},
	{0xf0, 0x36},
	{0x31, 0x82},
	{0x3e, 0x60},
	{0x30, 0xf0},
	{0x33, 0x33},
	{0xf0, 0x34},
	{0x9f, 0x02},
	{0xa6, 0x40},
	{0xa7, 0x47},
	{0xe8, 0x5f},
	{0xa8, 0x51},
	{0xa9, 0x44},
	{0xe9, 0x36},
	{PAGE_BANK_SEL, 0x33},
	{0xb3, 0x51},
	{0x64, 0x17},
	{0x90, 0x01},
	{0x91, 0x03},
	{0x92, 0x07},
	{0x01, 0x10},
	{0x93, 0x10},
	{0x94, 0x10},
	{0x95, 0x10},
	{0x96, 0x01},
	{0x97, 0x07},
	{0x98, 0x1f},
	{0x99, 0x10},
	{0x9a, 0x20},
	{0x9b, 0x28},
	{0x9c, 0x28},
	{PAGE_BANK_SEL, 0x36},
	{0x70, 0x54},
	{0xb6, 0x40},
	{0xb7, 0x41},
	{0xb8, 0x43},
	{0xb9, 0x47},
	{0xba, 0x4f},
	{0xb0, 0x8b},
	{0xb1, 0x8b},
	{0xb2, 0x8b},
	{0xb3, 0x9b},
	{0xb4, 0xb8},
	{0xb5, 0xf0},
	{0x7e, 0x41},
	{0x7f, 0x47},
	{0x77, 0x80},
	{0x78, 0x84},
	{0x79, 0x8a},
	{0xa0, 0x47},
	{0xa1, 0x5f},
	{0x96, 0x43},
	{0x97, 0x44},
	{0x98, 0x54},
	{PAGE_BANK_SEL, 0x00},
	{0xf0, 0x01},
	{0x73, 0x00},
	{0x74, 0xe0},
	{0x70, 0x00},
	{0x71, 0x80},
	{PAGE_BANK_SEL, 0x36},
	{0x37, 0x74},
	{0xf0, 0x3f},
	{0x03, 0x91},
	{PAGE_BANK_SEL, 0x36}, // cvbs_off
	{0x11, 0x80},
	{0xf0, 0x01},
	{0x79, 0xc1},
	{0xf0, 0x37},
	{0x24, 0x21},
	
	{0xf0, 0x36},//PAD 驱动能力 16’h3641 8’h00 Bit[1:0]: adjust PAD driver capability
	{0x41, 0x61},

	{0xf0, 0x32},
	{0x0e, 0x04},
	{0x0f, 0x18},
	{0xf0, 0x00},
	{0x72, 0x38},
	{0x7a, 0x80},
	{0x7c, 0x04},
	{0x7e, 0x25},
	{0x85, 0x18},
	{0x9b, 0x35},
	{0x9e, 0x20},
	{0xd0, 0x66},
	{0xd1, 0x34},
	{0Xd3, 0x44},
	{0xd6, 0x44},
	{0xb0, 0x41},
	{0xb2, 0x48},
	{0xb3, 0xf4},
	{0xb4, 0x0b},
	{0xb5, 0x78},
	{0xba, 0xff},
	{0xbb, 0xc0},
	{0xbc, 0x90},
	{0xbd, 0x3a},
	{0xc1, 0x67},
	{0xf0, 0x01},
	{0x20, 0x11},
	{0x23, 0x90},
	{0x24, 0x15},
	{0x25, 0x87},
	{0xbc, 0x9f},
	{0xbd, 0x3a},
	{0x48, 0xe6},
	{0x49, 0xc0},
	{0x4a, 0xd0},
	{0x4b, 0x48},
	{PAGE_BANK_SEL, 0x00},
	{0x71, 0x92},
	{0x7c, 0x03},
	{0x84, 0xb4},
	{PAGE_BANK_SEL, 0x33},
	{0x14, 0x95},

	//[cvbs_on}
	// {0xf0,0x36},
	// {0x11,0x00},
	// {0xf0,0x01},
	// {0x79,0xf1},

#if defined(CONFIG_SCO30IOT_FPS_30)
	{PAGE_BANK_SEL, 0x36},
	{0x11, 0x80},
	{0x41, 0x03}, // 驱动能力
	{0xf0, 0x01},
	{0x79, 0xc1},
	{0xf0, 0x32},
	{0x0c, 0x03},
	{0x0d, 0x59}, // 30fps
//  {0xf0,0x01},
//  {0x00,0x80},  //pattere test   bit{7}
#elif defined(CONFIG_SCO30IOT_FPS_20)
	{PAGE_BANK_SEL, 0x36},
	// {0x41,0x03},  //驱动能力
	{0x11, 0x80},
	{PAGE_BANK_SEL, 0x01},
	{0x79, 0xc1},
	// {0x7a,0x36},  //hsy 极性 bit【4】

	// HTS: 0x320C.  0x320D
	{PAGE_BANK_SEL, 0x32},
	{0x0C, 0x04},
	{0x0D, 0x78}, // 25fps //359-30fps  478-25fps   6b2-15fps

	// {0xf0,0x01},
	// {0x00,0x80}, //pattere test   bit{7}
#elif defined(CONFIG_SCO30IOT_FPS_15)
	{PAGE_BANK_SEL, 0x36},
	{0x11, 0x80},
	{0xf0, 0x01},
	{0x79, 0xc1}, //  yuvbit【4,5】,

	// HTS: 0x320C.  0x320D
	{PAGE_BANK_SEL, 0x32},
	{0x0c, 0x06},
	{0x0d, 0xb2}, // 15fps
//  {PAGE_BANK_SEL,0x01},
//  {0x00,0x80},  //pattere test   bit{7}
#endif

};

static int sco30iot_write_reg(const struct device *dev, uint8_t reg_addr, uint8_t value)
{
	struct sco30iot_data *drv_data = dev->data;
	struct i2c_msg msgs;
	uint8_t tx_buf[2];

	tx_buf[0] = reg_addr;
	tx_buf[1] = value;

	msgs.buf = tx_buf;
	msgs.len = 2;
	msgs.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer(drv_data->i2c_bus.bus, &msgs, 1, drv_data->i2c_bus.addr);
}


static int sco30iot_read_reg(const struct device *dev, uint8_t reg_addr, uint8_t *value)
{
	struct sco30iot_data *drv_data = dev->data;
	struct i2c_msg msgs[2];

	msgs[0].buf = (uint8_t *)&reg_addr;
	msgs[0].len = 1;
	/*
	 * When using I2C to read the registers of the SCCB device,
	 * a stop bit is required after writing the register address
	 */
	msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	msgs[1].buf = (uint8_t *)value;
	msgs[1].len = 1;
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_STOP | I2C_MSG_RESTART;

	return i2c_transfer(drv_data->i2c_bus.bus, msgs, 2, drv_data->i2c_bus.addr);
}

int sco30iot_modify_reg(const struct device *dev, uint8_t reg_addr, uint8_t clear_mask,
			uint8_t value)
{
	int ret;
	uint8_t set_value;

	ret = sco30iot_read_reg(dev, reg_addr, &set_value);

	if (ret == 0) {
		set_value = (set_value & (~clear_mask)) | (set_value & clear_mask);
		ret = sco30iot_write_reg(dev, reg_addr, set_value);
	}

	return ret;
}

static int sco30iot_write_all(const struct device *dev, const struct sco30iot_reg *regs,
			      uint16_t reg_num)
{
	uint16_t i = 0;
	int err;

	for (i = 0; i < reg_num; i++) {
		err = sco30iot_write_reg(dev, regs[i].addr, regs[i].value);
		// k_sleep(K_MSEC(10));
		if (err) {
			return err;
		}
	}

	return 0;
}

static int sco30iot_write_page_reg_16bit(const struct device *dev, uint16_t reg_addr, uint8_t value)
{
	int ret = 0;

	uint16_t reg_addr_tmp = reg_addr;
	ret |= sco30iot_write_reg(dev, PAGE_BANK_SEL, (reg_addr_tmp>>8));
	ret |= sco30iot_write_reg(dev, (reg_addr_tmp&0xFF), value);
	return ret;
}

static int sco30iot_read_page_reg_16bit(const struct device *dev, uint16_t reg_addr, uint8_t *value)
{
	int ret = 0;

	uint16_t reg_addr_temp = reg_addr;
	ret |= sco30iot_write_reg(dev, PAGE_BANK_SEL, (reg_addr_temp>>8));
	ret |= sco30iot_read_reg(dev, (reg_addr_temp & 0xFF), value);
	return 0;
}

static int sco30iot_set_fmt(const struct device *dev, enum video_endpoint_id ep,
			    struct video_format *fmt)
{
	int ret = 0;

	if (fmt->pixelformat == VIDEO_PIX_FMT_VYUY && fmt->height == 480 && fmt->width == 640) {
		ret = sco30iot_write_all(dev, sco30iot_init_reg_tb,
					 ARRAY_SIZE(sco30iot_init_reg_tb));
		if (ret) {
			LOG_ERR("Unable to write sco30iot config");
			return ret;
		}

		return 0;
	}
	LOG_ERR("video format unsupported \n");
	return -ENOTSUP;
}

static int sco30iot_get_fmt(const struct device *dev, enum video_endpoint_id ep,
			    struct video_format *fmt)
{
	return 0;
}

static int sco30iot_stream_start(const struct device *dev)
{
	return 0;
}

static int sco30iot_stream_stop(const struct device *dev)
{
	return 0;
}

static const struct video_format_cap fmts[] = {
	{
		.pixelformat = VIDEO_PIX_FMT_VYUY,
		.width_min = 160,
		.width_max = 640,
		.height_min = 120,
		.height_max = 480,
		.width_step = 0,
		.height_step = 0,
	},
	{0}
};

static int sco30iot_get_caps(const struct device *dev, enum video_endpoint_id ep,
			     struct video_caps *caps)
{
	caps->format_caps = fmts;
	return 0;
}

static int sco30iot_ctrl_init(const struct device *dev)
{
	int ret = 0;
	struct sco30iot_data *drv_data = dev->data;

	if (!device_is_ready(drv_data->i2c_bus.bus))
	{
		LOG_ERR("I2C bus %s is not ready.", drv_data->i2c_bus.bus->name);
		return -ENODEV;
	}
	LOG_DBG("I2C bus %s is ready.", drv_data->i2c_bus.bus->name);

	if (i2c_configure(drv_data->i2c_bus.bus,
			I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER)) {
		LOG_ERR("I2C config failed!");
		return -EIO;
	}
	uint32_t sensor_id = 0;
	uint8_t id_h = 0;
	uint8_t id_l = 0;

	ret = sco30iot_read_reg(dev, SC301IOT_CHIP_ID_REG_H, &id_h);
	ret = sco30iot_read_reg(dev, SC301IOT_CHIP_ID_REG_L, &id_l);
	sensor_id = (id_h << 8) | id_l;
	if (ret) {
		LOG_ERR("Unable to read sco30iot pid, ret=%d", ret);
		return -ENODEV;
	}
	if(sensor_id != SCO30IOT_SENSOR_ID){
		LOG_ERR("sco30iot read sensor id err 0x%x", sensor_id);
		return -ENODEV;
	}

	LOG_INF("Sensor id: 0x%X", sensor_id);
	/* Configure Sensor */

	return 0;
}

static int sco30iot_set_gain_ctrl(const struct device *dev, int enable)
{
	int ret = 0;
	uint8_t reg = 0;

	ret = sco30iot_read_page_reg_16bit(dev, PAGE_BANK_REG_EXPOSURE_GAIN, &reg);

	if (enable) {
		reg |= REG_EXPOSURE_GAIN_AEC_REG_EN;
	} else {
		reg &= ~REG_EXPOSURE_GAIN_AEC_REG_EN;
	}

	ret |= sco30iot_write_page_reg_16bit(dev, PAGE_BANK_REG_EXPOSURE_GAIN, reg);
	ret = sco30iot_read_page_reg_16bit(dev, PAGE_BANK_REG_EXPOSURE_GAIN, &reg);

	return ret;
}

static int sco30iot_set_horizontal_mirror(const struct device *dev, int enable)
{
	int ret = 0;
	uint8_t reg_mirror = 0;

	sco30iot_read_page_reg_16bit(dev, PAGE_BANK_REG_CID_MIRROR, &reg_mirror);
	if (enable) {
		reg_mirror |= REG_CID_MIRROR_ON_VALUE;
	} else {
		reg_mirror &= ~REG_CID_MIRROR_ON_VALUE;
	}
	ret = sco30iot_write_page_reg_16bit(dev, PAGE_BANK_REG_CID_MIRROR, reg_mirror);

	if (ret) {
		LOG_WRN("Unable to write sco30iot mirror ret=%d", ret);
		return ret;
	}
	return ret;
}

static int sco30iot_set_vertical_flip(const struct device *dev, int enable)
{
	int ret = 0;
	uint8_t reg_value = 0;

	sco30iot_read_page_reg_16bit(dev, PAGE_BANK_REG_CID_FLIP, &reg_value);
	// LOG_INF("sco30iot_set_horizontal_mirror reg = 0x%x\n", reg_value);
	if (enable) {
		reg_value |= REG_CID_FLIP_ON_VALUE;
	} else {
		reg_value &= ~REG_CID_FLIP_ON_VALUE;
	}
	ret = sco30iot_write_page_reg_16bit(dev, PAGE_BANK_REG_CID_FLIP, reg_value);

	if (ret) {
		LOG_WRN("Unable to write sco30iot mirror ret=%d", ret);
		return ret;
	}
	return ret;
}

static int sco30iot_set_exposure_time(const struct device *dev, int value)
{
	int ret = 0;
	uint8_t reg_value = 0;

	// disable auto exposure, disable auto gain
	reg_value &= ~(REG_EXPOSURE_GAIN_AEC_REG_EN | REG_EXPOSURE_GAIN_AGC_REG_EN);
	sco30iot_write_page_reg_16bit(dev, PAGE_BANK_REG_EXPOSURE_GAIN, reg_value);
	
	// set exposure time
	uint16_t exposure_time = value;
	#define EXPOSURE_TIME_MAX 0x3FFF
	if (exposure_time > EXPOSURE_TIME_MAX) {
		exposure_time = EXPOSURE_TIME_MAX;
	}
	ret = sco30iot_write_page_reg_16bit(dev, REG_EXPOSURE_REG_EXPOSURE_TIME_H, (exposure_time >> 8) & 0xFF);
	ret = sco30iot_write_page_reg_16bit(dev, REG_EXPOSURE_REG_EXPOSURE_TIME_L, exposure_time & 0xFF);

	if (ret) {
		LOG_WRN("Unable to set sco30iot exposure ret=%d", ret);
		return ret;
	}
	return ret;
}

static int sco30iot_set_colorbar(const struct device *dev, int enable)
{
	int ret = 0;
	uint8_t reg_value = 0;

	sco30iot_read_page_reg_16bit(dev, PAGE_BANK_REG_COLOR_BAR, &reg_value);
	if (enable) {
		reg_value |= PAGE_BANK_REG_COLOR_BAR_EN;
	} else {
		reg_value &= ~PAGE_BANK_REG_COLOR_BAR_EN;
	}

	ret = sco30iot_write_page_reg_16bit(dev, PAGE_BANK_REG_COLOR_BAR, reg_value);

	if (ret) {
		LOG_WRN("Unable to set sco30iot colorbar ret=%d", ret);
		return ret;
	}
	return ret;
}

static int sco30iot_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	int ret = 0;

	switch (cid) {
	case VIDEO_CID_HFLIP:
		ret = sco30iot_set_horizontal_mirror(dev, (int)value);
		break;
	case VIDEO_CID_VFLIP:
		ret = sco30iot_set_vertical_flip(dev, (int)value);
		break;
	case VIDEO_CID_INIT:
		/*
			该sensor只有在mclk有输入时钟的情况下，i2c才能正常通信，所以并不能在内核设备初始化流程中进行sensor初始化。
			目前是通过set_ctrl来提供sensor初始化的api.
		*/
		k_sleep(K_MSEC(50));// wait for mclk stable, otherwise i2c can't work, and sensor can't be initialized. (>=4ms)
		ret = sco30iot_ctrl_init(dev);
		break;
	case VIDEO_CID_CAMERA_GAIN:
		ret = sco30iot_set_gain_ctrl(dev, (int)value);
		break;
	case VIDEO_CID_CAMERA_EXPOSURE_TIME:
		ret = sco30iot_set_exposure_time(dev, (int)value);
		break;
	// case VIDEO_CID_EXPOSURE:
	// 	ret = sco30iot_set_exposure(dev, (int)value);
	// 	break;
	case VIDEO_CID_CAMERA_COLORBAR:
		ret |= sco30iot_set_colorbar(dev, (int)value);
		break;
	default:
		return -ENOTSUP;
	}

	return ret;
}

static const struct video_driver_api sco30iot_driver_api = {
	.set_format = sco30iot_set_fmt,
	.get_format = sco30iot_get_fmt,
	.get_caps = sco30iot_get_caps,
	.stream_start = sco30iot_stream_start,
	.stream_stop = sco30iot_stream_stop,
	.set_ctrl = sco30iot_set_ctrl,
};

static int sco30iot_init(const struct device *dev)
{
	struct sco30iot_data *drv_data = dev->data;
	int ret = 0;

	if (drv_data->pwdn_gpios.port) {
		gpio_pin_configure_dt(&drv_data->pwdn_gpios, GPIO_OUTPUT_ACTIVE);
		gpio_pin_set_dt(&drv_data->pwdn_gpios, 0);
	}

	if (drv_data->reset_gpios.port) {
		gpio_pin_configure_dt(&drv_data->reset_gpios, GPIO_OUTPUT_ACTIVE);
		gpio_pin_set_dt(&drv_data->reset_gpios, 0);
		k_sleep(K_MSEC(3));
		gpio_pin_set_dt(&drv_data->reset_gpios, 1);
		k_sleep(K_MSEC(1));
	}

	return ret;
}

#define SCO30IOT_DEVICE_INIT_CSK(index)						\
	static struct sco30iot_data sco30iot_data_##index = {			\
		.i2c_bus = I2C_DT_SPEC_INST_GET(index),				\
		.reset_gpios = GPIO_DT_SPEC_INST_GET_OR(index, reset_gpios, {}),\
		.pwdn_gpios = GPIO_DT_SPEC_INST_GET_OR(index, pwdn_gpios, {}),	\
	};									\
	DEVICE_DT_INST_DEFINE(index, &sco30iot_init, NULL, 			\
				&sco30iot_data_##index, NULL,			\
				POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,\
				&sco30iot_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCO30IOT_DEVICE_INIT_CSK)
