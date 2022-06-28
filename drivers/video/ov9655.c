/*
 * Copyright (c) 2022, listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ovti_ov9655

#include <zephyr.h>
#include <device.h>

#include <drivers/video.h>
#include <drivers/i2c.h>
#include <drivers/gpio.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ov9655);

/** 
  * @brief  OV9655 ID  
  */  
#define  OV9655_ID    0x96

/* OV9655 Registers definition */
#define OV9655_SENSOR_PIDH              0x0A
#define OV9655_SENSOR_PIDL              0x0B
#define OV9655_SENSOR_COM7              0x12
#define OV9655_SENSOR_TSLB              0x3A
#define OV9655_SENSOR_MTX1              0x4F
#define OV9655_SENSOR_MTX2              0x50
#define OV9655_SENSOR_MTX3              0x51
#define OV9655_SENSOR_MTX4              0x52
#define OV9655_SENSOR_MTX5              0x53
#define OV9655_SENSOR_MTX6              0x54
#define OV9655_SENSOR_BRTN              0x55
#define OV9655_SENSOR_CNST1             0x56
#define OV9655_SENSOR_CNST2             0x57

struct ov9655_data {
    const struct i2c_dt_spec i2c_bus;
    const struct gpio_dt_spec reset_gpios;
	const struct gpio_dt_spec pwdn_gpios;
	struct video_format fmt;
};

struct ov9655_reg {
	uint8_t addr;
	uint8_t value;
};

/* Initialization sequence for QVGA resolution (320x240) in YUV format */
static const struct ov9655_reg ov9655_init_reg_tb[] = {
	{ 0x12, 0x80 },{ 0x00, 0x00 },{ 0x01, 0x80 },{ 0x02, 0x80 },{ 0x03, 0x02 },{ 0x04, 0x03 },{ 0x0e, 0x61 },
	{ 0x0f, 0x42 },{ 0x11, 0x01 },{ 0x12, 0x62 },{ 0x13, 0xe7 },{ 0x14, 0x3a },{ 0x15, 0x00 },{ 0x16, 0x24 },{ 0x17, 0x18 },
	{ 0x18, 0x04 },{ 0x19, 0x01 },{ 0x1a, 0x81 },{ 0x1e, 0x04 },{ 0x24, 0x3c },{ 0x25, 0x36 },{ 0x26, 0x72 },
	{ 0x27, 0x08 },{ 0x28, 0x08 },{ 0x29, 0x15 },{ 0x2a, 0x00 },{ 0x2b, 0x00 },{ 0x2c, 0x08 },{ 0x32, 0x24 },
	{ 0x33, 0x00 },{ 0x34, 0x3f },{ 0x35, 0x00 },{ 0x36, 0x3a },{ 0x38, 0x72 },{ 0x39, 0x57 },{ 0x3a, 0x08},
	{ 0x3b, 0x04 },{ 0x3d, 0x99 },{ 0x3e, 0x0e },{ 0x3f, 0xc1 },{ 0x40, 0xc0 },{ 0x41, 0x01 },{ 0x42, 0xc0 },
	{ 0x43, 0x0a },{ 0x44, 0xf0 },{ 0x45, 0x46 },{ 0x46, 0x62 },{ 0x47, 0x2a },{ 0x48, 0x3c },{ 0x4a, 0xfc },
	{ 0x4b, 0xfc },{ 0x4c, 0x7f },{ 0x4d, 0x7f },{ 0x4e, 0x7f },{ 0x52, 0x28 },{ 0x53, 0x88 },{ 0x54, 0xb0 },
	{ 0x4f, 0x98 },{ 0x50, 0x98 },{ 0x51, 0x00 },{ 0x58, 0x1a },{ 0x59, 0x85 },{ 0x5a, 0xa9 },{ 0x5b, 0x64 },
	{ 0x5c, 0x84 },{ 0x5d, 0x53 },{ 0x5e, 0x0e },{ 0x5f, 0xf0 },{ 0x60, 0xf0 },{ 0x61, 0xf0 },{ 0x62, 0x00 },
	{ 0x63, 0x00 },{ 0x64, 0x02 },{ 0x65, 0x20 },{ 0x66, 0x00 },{ 0x69, 0x0a },{ 0x6b, 0x5a },{ 0x6c, 0x04 },
	{ 0x6d, 0x55 },{ 0x6e, 0x00 },{ 0x6f, 0x9d },{ 0x70, 0x21 },{ 0x71, 0x78 },{ 0x72, 0x11 },{ 0x73, 0x01 },
	{ 0x74, 0x10 },{ 0x75, 0x10 },{ 0x76, 0x01 },{ 0x77, 0x02 },{ 0x7a, 0x12 },{ 0x7b, 0x08 },{ 0x7c, 0x15 },
	{ 0x7d, 0x24 },{ 0x7e, 0x45 },{ 0x7f, 0x55 },{ 0x80, 0x6a },{ 0x81, 0x78 },{ 0x82, 0x87 },{ 0x83, 0x96 },
	{ 0x84, 0xa3 },{ 0x85, 0xb4 },{ 0x86, 0xc3 },{ 0x87, 0xd6 },{ 0x88, 0xe6 },{ 0x89, 0xf2 },{ 0x8a, 0x24 },
	{ 0x8c, 0x80 },{ 0x90, 0x7d },{ 0x91, 0x7b },{ 0x9d, 0x02 },{ 0x9e, 0x02 },{ 0x9f, 0x7a },{ 0xa0, 0x79 },
	{ 0xa1, 0x40 },{ 0xa4, 0x50 },{ 0xa5, 0x68 },{ 0xa6, 0x4a },{ 0xa8, 0xc1 },{ 0xa9, 0xef },{ 0xaa, 0x92 },
	{ 0xab, 0x04 },{ 0xac, 0x80 },{ 0xad, 0x80 },{ 0xae, 0x80 },{ 0xaf, 0x80 },{ 0xb2, 0xf2 },{ 0xb3, 0x20 },
	{ 0xb4, 0x20 },{ 0xb5, 0x00 },{ 0xb6, 0xaf },{ 0xbb, 0xae },{ 0xbc, 0x7f },{ 0xbd, 0x7f },{ 0xbe, 0x7f },
	{ 0xbf, 0x7f },{ 0xc0, 0xaa },{ 0xc1, 0xc0 },{ 0xc2, 0x01 },{ 0xc3, 0x4e },{ 0xc6, 0x05 },{ 0xc7, 0x81 },
	{ 0xc9, 0xe0 },{ 0xca, 0xe8 },{ 0xcb, 0xf0 },{ 0xcc, 0xd8 },{ 0xcd, 0x93 },{ 0xcd, 0x93 },{ 0xFF, 0xFF }
};


static int ov9655_write_reg(const struct device *dev, uint8_t reg_addr,
			    uint8_t value)
{
	struct ov9655_data *drv_data = dev->data;
	struct i2c_msg msgs;
	uint8_t tx_buf[2];

	tx_buf[0] = reg_addr;
	tx_buf[1] = value;

	msgs.buf = tx_buf;
	msgs.len = 2;
	msgs.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer(drv_data->i2c_bus.bus, &msgs, 1, drv_data->i2c_bus.addr);
}

static int ov9655_read_reg(const struct device *dev, uint8_t reg_addr,
			   uint8_t *value)
{
	struct ov9655_data *drv_data = dev->data;
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

int ov9655_modify_reg(const struct device *dev,
		      uint8_t reg_addr,
		      uint8_t clear_mask,
		      uint8_t value)
{
	int ret;
	uint8_t set_value;

	ret = ov9655_read_reg(dev, reg_addr, &set_value);

	if (ret == 0) {
		set_value = (set_value & (~clear_mask)) |
				(set_value & clear_mask);
		ret = ov9655_write_reg(dev, reg_addr, set_value);
	}


	return ret;
}

static int ov9655_write_all(const struct device *dev,
			    const struct ov9655_reg *regs,
			    uint16_t reg_num)
{
	uint16_t i = 0;
	int err;

	for (i = 0; i < reg_num; i++) {
		err = ov9655_write_reg(dev, regs[i].addr, regs[i].value);
		k_sleep(K_MSEC(1));
		if (err) {
			return err;
		}
	}

	return 0;
}

static int ov9655_set_fmt(const struct device *dev,
			  enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	/* we only support one format for now (QVGA VYUY) */
	if (fmt->pixelformat != VIDEO_PIX_FMT_VYVU || fmt->height != 240 ||
	    fmt->width != 320) {
		LOG_WRN("ov9655 only support one format for now (QVGA VYUY) \n ");
		return -ENOTSUP;
	}

    return 0;
}

static int ov9655_get_fmt(const struct device *dev,
			  enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	return 0;
}

static int ov9655_stream_start(const struct device *dev)
{
	return 0;
}

static int ov9655_stream_stop(const struct device *dev)
{
	return 0;
}

static const struct video_format_cap fmts[] = {
	{
		.pixelformat = VIDEO_PIX_FMT_VYVU,
		.width_min = 320,
		.width_max = 320,
		.height_min = 240,
		.height_max = 240,
		.width_step = 0,
		.height_step = 0,
	},
	{ 0 }
};

static int ov9655_get_caps(const struct device *dev,
			   enum video_endpoint_id ep,
			   struct video_caps *caps)
{
	caps->format_caps = fmts;
	return 0;
}

#if CONFIG_VIDEO_OV9655_MANUAL_UV
static int ov9655_set_fixed_UV_value(const struct device *dev)
{
	uint8_t value = 0;
	ov9655_read_reg(dev, OV9655_SENSOR_TSLB, &value);
	ov9655_write_reg(dev, OV9655_SENSOR_TSLB, value | (1 << 4));
	ov9655_write_reg(dev, 0x67, CONFIG_VIDEO_OV9655_MANUAL_U_VALUE);
	ov9655_write_reg(dev, 0x68, CONFIG_VIDEO_OV9655_MANUAL_V_VALUE);

	//wait stable
	k_sleep(K_MSEC(1));
	return 0;
}
#endif

static int ov9655_init(const struct device *dev)
{
	struct ov9655_data *drv_data = dev->data;
	int ret = 0;
	uint8_t pid;

	if (i2c_configure(drv_data->i2c_bus.bus,
			I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER)) {
		LOG_ERR("I2C config failed!");
		return -EIO;
	}

	//check chip id
	ret = ov9655_read_reg(dev, OV9655_SENSOR_PIDH, &pid);
	if (ret) {
		LOG_ERR("Unable to read ov9655 pid");
		return -ENODEV;
	}

	if(pid != OV9655_ID){
		LOG_ERR("ov9655 read PID err 0x%x", pid);
		return -ENODEV;		
	}

	/* Prepare the camera to be configured by resetting all its registers */
	ov9655_write_reg(dev, OV9655_SENSOR_COM7, 0x80);
	k_sleep(K_MSEC(10));

	/* Configure Sensor */
	ret = ov9655_write_all(dev, ov9655_init_reg_tb,
				ARRAY_SIZE(ov9655_init_reg_tb));
	if (ret) {
		LOG_ERR("Unable to write ov9655 config");
		return ret;
	}

#if CONFIG_VIDEO_OV9655_MANUAL_UV
	ov9655_set_fixed_UV_value(dev);
#endif

	return 0;
}

static int ov9655_set_ctrl(const struct device *dev,
				unsigned int cid, void *value)
{
	int ret = 0;

	switch (cid) {	
	/*
		该sensor只有在mclk有输入时钟的情况下，i2c才能正常通信，所以并不能在内核设备初始化流程中进行sensor初始化。
		目前是通过set_ctrl来提供sensor初始化的api.
	*/
	case 0:
		ret = ov9655_init(dev);
		break;
	default:
		return -ENOTSUP;
	}

	return ret;
}

static const struct video_driver_api ov9655_driver_api = {
	.set_format = ov9655_set_fmt,
	.get_format = ov9655_get_fmt,
	.get_caps = ov9655_get_caps,
	.stream_start = ov9655_stream_start,
	.stream_stop = ov9655_stream_stop,
	.set_ctrl = ov9655_set_ctrl,
};

static int ov9655_init_0(const struct device *dev)
{
	struct ov9655_data *drv_data = dev->data;
	int ret = 0;

	if(drv_data->pwdn_gpios.port){
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

static struct ov9655_data ov9655_data_0 =
{
    .i2c_bus = I2C_DT_SPEC_GET(DT_DRV_INST(0)),

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), reset_gpios)
    .reset_gpios = GPIO_DT_SPEC_GET(DT_DRV_INST(0), reset_gpios),
#endif

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), pwdn_gpios)
	.pwdn_gpios = GPIO_DT_SPEC_GET(DT_DRV_INST(0), pwdn_gpios),
#endif
};

DEVICE_DT_INST_DEFINE(0, &ov9655_init_0, NULL,
		    &ov9655_data_0, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &ov9655_driver_api);
