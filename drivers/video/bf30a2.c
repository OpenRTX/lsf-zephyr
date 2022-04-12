/*
 * Copyright (c) 2022, listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT byd_bf30a2

#include <zephyr.h>
#include <device.h>

#include <sys/byteorder.h>

#include <drivers/video.h>
#include <drivers/i2c.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <drivers/pwm.h>

#include <stdbool.h>
#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>

LOG_MODULE_REGISTER(bf30a2);

#if CONFIG_VIDEO_BUFFER_POOL_NUM_MAX < 1
#error "CONFIG_VIDEO_BUFFER_POOL_NUM_MAX < 1"
#endif

#define BF30A2_FRAME_WIDTH (128)
#define BF30A2_FRAME_HEIGHT (180)

#define BF30A2_DEPTH (1)
#define BF30A2_RX_BUF_CNT (2)

#define BF30A2_FRAME_START_PACKET_LEN (9)
#define BF30A2_LINE_START_PACKET_LEN (6)
#define BF30A2_DATA_PACKET_HEADA_LEN (6)

#define BF30A2_DATA_PACKET_LEN                                                                     \
	((BF30A2_LINE_START_PACKET_LEN + BF30A2_DATA_PACKET_HEADA_LEN +                            \
	  BF30A2_FRAME_WIDTH * BF30A2_DEPTH))

#define BF30A2_FRAME_END_PACKET_LEN (4)

#define BF30A2_FRAME_SIZE                                                                          \
	(BF30A2_FRAME_START_PACKET_LEN + BF30A2_DATA_PACKET_LEN * BF30A2_FRAME_HEIGHT +            \
	 BF30A2_LINE_START_PACKET_LEN + BF30A2_FRAME_END_PACKET_LEN)

struct pwm_dt_spec {
	const struct device *pwm;
	const int pin;
	const int flag;
};

struct bf20a2_chip_data {
	const struct gpio_dt_spec pd_gpios;
	const struct gpio_dt_spec pwr_gpios;
	const struct i2c_dt_spec i2c_bus;
	const struct pwm_dt_spec pwm_bus;
	const struct spi_dt_spec spi_bus;
	const uint16_t product_id;
	const int xclk_freq;
};

struct bf30a2_dev_data {
	struct bf20a2_chip_data chip_data;
	struct k_work_delayable dwork;
	struct k_fifo fifo;
	struct video_buffer *vbuf[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX];
	uint32_t vbuf_index;

	int buf_index;
};

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)

#define X_AXIS 0x3b
#define V_START 0x46
#define V_STOP 0x7d

struct bf30a2_reg_config {
	uint8_t reg;
	uint8_t value;
};

static const struct bf30a2_reg_config bf30a2_regs_configs[] = {
	{ 0xf2, 0x01 },
	{ 0x2F, X_AXIS },
	{ 0x2e, 0x57 }, //
	{ 0x07, 0x52 }, //
	{ 0x2c, 0x51 }, //
	{ 0x12, 0xc3 }, //
	{ 0x15, 0x80 }, //
	{ 0x6b, 0x71 }, //
	{ 0x04, 0x00 }, //
	{ 0x06, 0x26 }, //
	{ 0x08, 0x07 }, //
	{ 0x1c, 0x12 }, //
	{ 0x1e, 0xae }, //
	{ 0x1f, 0x00 }, //
	{ 0x20, 0x20 }, //
	{ 0x21, 0x20 }, //
	{ 0x34, 0x02 }, //
	{ 0x35, 0x02 }, //
	{ 0x36, 0x21 }, //
	{ 0x37, 0x13 }, //
	{ 0xca, 0x07 }, //
	{ 0xcb, 0x22 }, //
	{ 0xcc, 0x89 }, //
	{ 0xcd, 0x4c }, //
	{ 0xce, 0x6b }, //
	{ 0xcf, 0xf0 }, //
	{ 0xa0, 0x8e }, //
	{ 0x01, 0x1b }, //
	{ 0x02, 0x1d }, //
	{ 0x13, 0x08 }, //
	{ 0x87, 0x13 },
	{ 0x8a, 0x1a },
	{ 0x8b, 0x08 },
	{ 0x70, 0x1d },
	{ 0x71, 0x40 },
	{ 0x72, 0x02 },
	{ 0x73, 0x62 },
	{ 0x74, 0x22 },
	{ 0x75, 0xbf },
	{ 0x76, 0x01 },
	{ 0x77, 0xcc },
	{ 0x40, 0x32 },
	{ 0x41, 0x28 },
	{ 0x42, 0x26 },
	{ 0x43, 0x1d },
	{ 0x44, 0x1a },
	{ 0x45, 0x14 },
	{ 0x46, 0x11 },
	{ 0x47, 0x0f },
	{ 0x48, 0x0e },
	{ 0x49, 0x0d },
	{ 0x4B, 0x0c },
	{ 0x4C, 0x0b },
	{ 0x4E, 0x0a },
	{ 0x4F, 0x09 },
	{ 0x50, 0x09 },
	{ 0x24, 0x50 },
	{ 0x25, 0x36 },
	{ 0x80, 0x00 },
	{ 0x81, 0x20 },
	{ 0x82, 0x40 },
	{ 0x83, 0x30 },
	{ 0x84, 0x50 },
	{ 0x85, 0x30 },
	{ 0x86, 0x30 },
	{ 0x89, 0x45 },
	{ 0x8f, 0x81 },
	{ 0x91, 0xff },
	{ 0x92, 0x08 },
	{ 0x94, 0x82 },
	{ 0x95, 0xfd },
	{ 0x9a, 0x20 },
	{ 0x9e, 0xbc },
	{ 0xf0, 0x81 },
	{ 0x51, 0x06 },
	{ 0x52, 0x25 },
	{ 0x53, 0x2b },
	{ 0x54, 0x0F },
	{ 0x57, 0x2A },
	{ 0x58, 0x22 },
	{ 0x59, 0x2c },
	{ 0x23, 0x33 },
	{ 0xa0, 0x8f },
	{ 0xa1, 0x93 },
	{ 0xa2, 0x0f },
	{ 0xa3, 0x2a },
	{ 0xa4, 0x08 },
	{ 0xa5, 0x26 },
	{ 0xa7, 0x80 },
	{ 0xa8, 0x80 },
	{ 0xa9, 0x1e },
	{ 0xaa, 0x19 },
	{ 0xab, 0x18 },
	{ 0xae, 0x50 },
	{ 0xaf, 0x04 },
	{ 0xc8, 0x10 },
	{ 0xc9, 0x15 },
	{ 0xd3, 0x0c },
	{ 0xd4, 0x16 },
	{ 0xee, 0x06 },
	{ 0xef, 0x04 },
	{ 0x55, 0x34 },
	{ 0x56, 0x9c },
	{ 0xb1, 0x98 },
	{ 0xb2, 0x98 },
	{ 0xb3, 0xc4 },
	{ 0xb4, 0x0C },
	{ 0x00, 0x42 },
	{ 0x13, 0x07 },
	{ 0xf1, 0x09 },
	{ 0xa0, 0x8E },
	{ 0xa1, 0x93 },
	{ 0x23, 0x22 },
	{ 0x01, 0x0a },
	{ 0x02, 0x0a },
	{ 0x17, 0x00 },
	{ 0x18, 0x80 },
	{ 0x19, V_START },
	{ 0x1a, V_STOP },
	/*  Soft sleep mode */
	{ 0xcf, 0xf1 },
};

static int bf30a2_chip_xclk_init(const struct device *dev)
{
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	if (!device_is_ready(data->chip_data.pwm_bus.pwm)) {
		LOG_ERR("PWM device is not ready!");
		return -1;
	}

	/* period unit: nanoseconds, data->chip_data.xclk_freq unit: Khz */
	uint32_t period = 1000000 / data->chip_data.xclk_freq;

	if (pwm_pin_set_nsec(data->chip_data.pwm_bus.pwm, data->chip_data.pwm_bus.pin, period,
			     period / 2, data->chip_data.pwm_bus.flag)) {
		LOG_ERR("PWM device config error!");
		return -1;
	}

	return 0;
}

static int bf30a2_chip_power_enable(const struct device *dev, uint8_t status)
{
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	return gpio_pin_configure(data->chip_data.pwr_gpios.port, data->chip_data.pwr_gpios.pin,
				  (status ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE) |
					  data->chip_data.pwr_gpios.dt_flags);
}

static int bf30a2_chip_power_down_enable(const struct device *dev, uint8_t status)
{
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	return gpio_pin_configure(data->chip_data.pd_gpios.port, data->chip_data.pd_gpios.pin,
				  (status ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE) |
					  data->chip_data.pd_gpios.dt_flags);
}

static int bf30a2_chip_write_register(const struct device *dev, uint8_t addr, uint8_t value)
{
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;
	uint8_t wr_addr[2];
	struct i2c_msg msgs;

	wr_addr[0] = addr;
	wr_addr[1] = value;

	msgs.buf = wr_addr;
	msgs.len = 2;
	msgs.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer(data->chip_data.i2c_bus.bus, &msgs, 1, data->chip_data.i2c_bus.addr);
}

static int bf30a2_chip_read_regsiter(const struct device *dev, uint8_t addr, uint8_t *buf,
				     uint32_t buf_size)
{
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;
	uint8_t wr_addr[2];
	struct i2c_msg msgs[2];

	wr_addr[0] = addr;

	msgs[0].buf = wr_addr;
	msgs[0].len = 1U;
	msgs[0].flags = I2C_MSG_WRITE;

	msgs[1].buf = buf;
	msgs[1].len = buf_size;
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_STOP;

	return i2c_transfer(data->chip_data.i2c_bus.bus, &msgs[0], 2, data->chip_data.i2c_bus.addr);
}

static int bf30a2_chip_i2c_init(const struct device *dev)
{
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	if (!device_is_ready(data->chip_data.i2c_bus.bus)) {
		LOG_ERR("I2C is not ready!");
		return -1;
	}

	if (i2c_configure(data->chip_data.i2c_bus.bus,
			  I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER)) {
		LOG_ERR("I2C config failed!");
		return -1;
	}

	/* Try to read the product ID */
	uint8_t id[2] = { 0 };
	int32_t r;

	r = bf30a2_chip_read_regsiter(dev, 0xfc, &id[0], 1);
	if (r != 0) {
		return r;
	}
	r = bf30a2_chip_read_regsiter(dev, 0xfd, &id[1], 1);
	if (r != 0) {
		return r;
	}

	if ((id[0] != (data->chip_data.product_id >> 8)) ||
	    (id[1] != (data->chip_data.product_id & 0xFF))) {
		LOG_ERR("An incorrect product ID(%02X%02X) was read, target product ID(%04X).",
			id[0], id[1], data->chip_data.product_id);
		return -1;
	}

	return 0;
}

static int bf30a2_chip_config(const struct device *dev)
{
	int32_t r;
	uint32_t c = sizeof(bf30a2_regs_configs) / sizeof(struct bf30a2_reg_config);
	const struct bf30a2_reg_config *config = &bf30a2_regs_configs[0];

	while (c--) {
		r = bf30a2_chip_write_register(dev, config->reg, config->value);
		if (r != 0) {
			LOG_ERR("Config register %X error!", config->reg);
			return r;
		}
		config++;
	}

	return 0;
}
#endif

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
static int bf30a2_chip_spi_init(const struct device *dev)
{
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	if (!device_is_ready(data->chip_data.spi_bus.bus)) {
		LOG_ERR("SPI bus is not ready!");
		return -1;
	}

	return spi_transceive_dt(&data->chip_data.spi_bus, NULL, NULL);
}
#endif

static int bf30a2_chip_init(const struct device *dev)
{
	int r;

	/** 
     * NOTE THAT: bf30a2 must aways be on bus i2c if the csk6_spec property is not defined.
     */
#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)
	r = bf30a2_chip_xclk_init(dev);
	if (r != 0) {
		LOG_ERR("XCLK init error.");
		return r;
	}
	r = bf30a2_chip_power_enable(dev, 1);
	if (r != 0) {
		LOG_ERR("power enable error.");
		return r;
	}
	r = bf30a2_chip_power_down_enable(dev, 0);
	if (r != 0) {
		LOG_ERR("power dowm mode disable error.");
		return r;
	}
	r = bf30a2_chip_i2c_init(dev);
	if (r != 0) {
		LOG_ERR("i2c init error.");
		return r;
	}
	r = bf30a2_chip_config(dev);
	if (r != 0) {
		LOG_ERR("chip config error.");
		return r;
	}
#endif

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
	r = bf30a2_chip_spi_init(dev);
	if (r != 0) {
		LOG_ERR("spi init error.");
		return r;
	}
#endif

	return r;
}

static int bf30a2_driv_set_format(const struct device *dev, enum video_endpoint_id ep,
				  struct video_format *fmt)
{
	return 0;
}

static int bf30a2_driv_get_format(const struct device *dev, enum video_endpoint_id ep,
				  struct video_format *fmt)
{
	if (fmt == NULL) {
		return -1;
	}

	fmt->pixelformat = VIDEO_PIX_FMT_BGGR8;
	fmt->width = 128;
	fmt->height = 180;
	fmt->pitch = 180;

	return 0;
}

static int bf30a2_driv_get_caps(const struct device *dev, enum video_endpoint_id ep,
				struct video_caps *caps)
{
	return 0;
}

static int bf30a2_driv_stream_start(const struct device *dev)
{
#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)
	/* TODO: active CS pin */
	bf30a2_chip_write_register(dev, 0xCF, 0xF0);

	bf30a2_chip_power_enable(dev, 0);
#endif

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
	extern const k_tid_t bf30a2_thread_id;
	k_thread_resume(bf30a2_thread_id);
#endif

	return 0;
}

static int bf30a2_driv_stream_stop(const struct device *dev)
{
#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)
	bf30a2_chip_write_register(dev, 0xCF, 0xF1);
	bf30a2_chip_power_enable(dev, 1);
#endif

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
	extern const k_tid_t bf30a2_thread_id;
	k_thread_suspend(bf30a2_thread_id);
#endif
	return 0;
}

static int bf30a2_driv_dequeue(const struct device *dev, enum video_endpoint_id ep,
			       struct video_buffer **buf, k_timeout_t timeout)
{
#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	*buf = k_fifo_get(&data->fifo, timeout);
	if (*buf == NULL) {
		return -EAGAIN;
	}

	return 0;
#else
	return -1;
#endif
}

static int bf30a2_set_ctrl(const struct device *dev, unsigned int cid, void *value)
{
	return 0;
}

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)

enum { BF30A2_PACKET_TYPE_FRAME_START = 0x01,
       BF30A2_PACKET_TYPE_LINE_START = 0x02,
       BF30A2_PACKET_TYPE_FRAME_DATA = 0x40,
       BF30A2_PACKET_TYPE_FRAME_END = 0x00,
} bf30a2_packet_type_t;

static inline int bf30a2_frame_check(char *in, uint32_t l, uint8_t packetId)
{
	if (in == NULL || l < 4) {
		return -1;
	}

	if (in[0] != 0xFF || in[1] != 0xFF || in[2] != 0xFF || in[3] != packetId) {
		return -1;
	}

	return 0;
}

static int bf30a2_decode(char *in, char *out, uint32_t l, uint16_t h, uint16_t w)
{
	uint16_t i = 0;
	uint32_t offset = 0;
	uint32_t len = 0;

	if (bf30a2_frame_check(in + offset, l, BF30A2_PACKET_TYPE_FRAME_START) != 0) {
		return -1;
	}

	offset += BF30A2_FRAME_START_PACKET_LEN;

	for (i = 0; i < h; i++) {
		if (bf30a2_frame_check(in + offset, l - offset, BF30A2_PACKET_TYPE_LINE_START) !=
		    0) {
			int j = 1;
			while (j < 128) {
				if (bf30a2_frame_check(in + offset - j, l - offset,
						       BF30A2_PACKET_TYPE_LINE_START) == 0) {
					offset -= j;
					break;
				}
				j++;
			}
			if (j >= 128) {
				return -1;
			}
		}

		offset += BF30A2_LINE_START_PACKET_LEN;

		/* data packet */
		if (bf30a2_frame_check(in + offset, l - offset, BF30A2_PACKET_TYPE_FRAME_DATA) !=
		    0) {
			printk("Decode error: data packet, h=%d. offset=0x%x\n", i, offset);
			return -1;
		} else {
			/* TODO: Is need to check data size for the data packet? */
		}
		uint32_t _copy_size = w;
		/* copy raw data to out buffer. */
		char *dst = out + i * w;
		char *src = in + offset + BF30A2_DATA_PACKET_HEADA_LEN;
		uint32_t _size = _copy_size;
		while (_size--) {
			*dst++ = *src++;
		}
		offset += BF30A2_DATA_PACKET_HEADA_LEN + _copy_size;
		len += w;
	}

	return len;
}

static int bf30a2_decode_x(char *in, char *out, uint32_t l, uint16_t h, uint16_t w)
{
	for (int i = 0; i < l; i++) {
		if ((l - i) < BF30A2_FRAME_SIZE) {
			break;
		}

		if (bf30a2_frame_check(in + i, l - i, BF30A2_PACKET_TYPE_FRAME_START) == 0) {
			int r = bf30a2_decode(in + i, out, l - i, h, w);
			if (r > 0) {
				return r;
			}
		}
	}

	return -1;
}

static void bf30a2_thread(void *p1, void *p2, void *p3)
{
	static uint8_t frame_buffer[BF30A2_FRAME_SIZE * 2] = { 0 };
	static uint8_t frame_index = 0;

	int r = 0;
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)p1;
	struct video_buffer *vbuf;
	struct spi_buf spi_buf;
	struct spi_buf_set rx_buf;

	__ASSERT(p1 != NULL, "Param is null.");

	/** 
	 * FIXME:
	 * There may be problems with receiving data from SPI,
	 * so here use a bad way to receive spi datas:
	 * Decode one frame of data by receiving two frames of SPI datas.
	 */
	spi_buf.buf = frame_buffer;
	spi_buf.len = sizeof(frame_buffer);
	rx_buf.buffers = &spi_buf;
	rx_buf.count = 1;

	extern const k_tid_t bf30a2_thread_id;
	k_thread_suspend(bf30a2_thread_id);

	while (1) {
		frame_index %= CONFIG_VIDEO_BUFFER_POOL_NUM_MAX;
		vbuf = data->vbuf[frame_index];
		frame_index++;

		r = spi_transceive_dt(&data->chip_data.spi_bus, NULL, &rx_buf);
		if (r == spi_buf.len) {
			r = bf30a2_decode_x(spi_buf.buf, vbuf->buffer, r, 180, 128);
			if (r > 0) {
				vbuf->bytesused = r;
				vbuf->timestamp = k_uptime_get_32();
				k_fifo_put(&data->fifo, vbuf);
			}
		}
	}
}
#endif

static const struct video_driver_api bf30a2_driv_api = {
	.set_format = bf30a2_driv_set_format,
	.get_format = bf30a2_driv_get_format,
	.get_caps = bf30a2_driv_get_caps,
	.stream_start = bf30a2_driv_stream_start,
	.stream_stop = bf30a2_driv_stream_stop,
	.dequeue = bf30a2_driv_dequeue,
	.set_ctrl = bf30a2_set_ctrl,
};

static int bf30a2_dev_init(const struct device *dev)
{
#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)

	/* FIXME: Use power pin temporary instead of cs pin at here. */
	bf30a2_chip_power_enable(dev, 1);

#endif

	bf30a2_chip_init(dev);

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	k_fifo_init(&data->fifo);

	uint32_t i;
	for (i = 0; i < ARRAY_SIZE(data->vbuf); i++) {
		data->vbuf[i] = video_buffer_alloc(BF30A2_FRAME_SIZE);
		if (data->vbuf[i] == NULL) {
			LOG_ERR("Unable to alloc video buffer");
			return -1;
		}
	}

#endif

	return 0;
}

#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)

#define BF30A2_SPI_CONFIG_DT(node_id, operation_, delay_)                                          \
	{                                                                                          \
		.frequency = DT_PROP(node_id, spi_max_frequency), .operation = (operation_),       \
		.slave = 0,                                                                        \
		.cs = COND_CODE_1(DT_SPI_DEV_HAS_CS_GPIOS(node_id),                                \
				  (SPI_CS_CONTROL_PTR_DT(node_id, delay_)), (NULL)),               \
	}

#define BF30A2_SPI_DT_SPEC_GET(node_id, operation_, delay_)                                        \
	{                                                                                          \
		.bus = DEVICE_DT_GET(DT_PROP(node_id, spi)),                                       \
		.config = BF30A2_SPI_CONFIG_DT(DT_PROP(node_id, spi), operation_, delay_),         \
	}

#define BF30A2_PWM_DT_SPEC_GET(node_id)                                                            \
	{                                                                                          \
		.pwm = DEVICE_DT_GET(DT_PWMS_CTLR(node_id)),                                  \
		.pin = DT_PWMS_CHANNEL(node_id),                 \
		.flag = DT_PWMS_FLAGS(node_id),                                 \
	}

#define BF30A2_CHIP_DATA(inst)                                                                     \
	{                                                                                          \
		.pd_gpios = GPIO_DT_SPEC_GET(DT_DRV_INST(inst), pd_gpios),                         \
		.pwr_gpios = GPIO_DT_SPEC_GET(DT_DRV_INST(inst), pwr_gpios),                       \
		.i2c_bus = I2C_DT_SPEC_GET(DT_DRV_INST(inst)),                                     \
		.spi_bus = BF30A2_SPI_DT_SPEC_GET(                                                 \
			DT_DRV_INST(inst), SPI_WORD_SET(8) | SPI_OP_MODE_SLAVE | SPI_TRANSFER_MSB, \
			0),                                                                        \
		.pwm_bus = BF30A2_PWM_DT_SPEC_GET(DT_DRV_INST(inst)),                              \
		.product_id = DT_PROP(DT_DRV_INST(inst), product_id),                              \
		.xclk_freq = DT_PROP(DT_DRV_INST(inst), xclk_freq),                                \
	}
#else

#define BF30A2_SPI_CONFIG_DT(node_id, operation_, delay_)                                          \
	{                                                                                          \
		.frequency = DT_PROP(node_id, spi_max_frequency), .operation = (operation_),       \
		.slave = 0,                                                                        \
		.cs = COND_CODE_1(DT_SPI_DEV_HAS_CS_GPIOS(node_id),                                \
				  (SPI_CS_CONTROL_PTR_DT(node_id, delay_)), (NULL)),               \
	}

#define BF30A2_SPI_DT_SPEC_GET(node_id, operation_, delay_)                                        \
	{                                                                                          \
		.bus = DEVICE_DT_GET(DT_BUS(node_id)),                                             \
		.config = BF30A2_SPI_CONFIG_DT(DT_BUS(node_id), operation_, delay_),               \
	}

#define BF30A2_CHIP_DATA(inst)                                                                     \
	{                                                                                          \
		.spi_bus = BF30A2_SPI_DT_SPEC_GET(                                                 \
			DT_DRV_INST(inst), SPI_WORD_SET(8) | SPI_OP_MODE_SLAVE | SPI_TRANSFER_MSB, \
			0)                                                                         \
	}
#endif /* end of DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c) */

static struct bf30a2_dev_data bf30a2_dev_data = {
	.chip_data = BF30A2_CHIP_DATA(0),
};

#if DT_NODE_HAS_STATUS(DT_DRV_INST(0), okay)
DEVICE_DT_INST_DEFINE(0, &bf30a2_dev_init, NULL, &bf30a2_dev_data, NULL, POST_KERNEL,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &bf30a2_driv_api);

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
K_THREAD_DEFINE(bf30a2_thread_id, 2048, bf30a2_thread, &bf30a2_dev_data, NULL, NULL,
		K_HIGHEST_THREAD_PRIO, 0, 0);
#endif

#endif
