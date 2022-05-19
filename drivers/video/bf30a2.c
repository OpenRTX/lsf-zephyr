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
#include <drivers/uart.h>

#include <hal/spi.h>
#include <hal/dma.h>

#include <stdbool.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(bf30a2);

#if CONFIG_VIDEO_BUFFER_POOL_NUM_MAX < 1
#error "CONFIG_VIDEO_BUFFER_POOL_NUM_MAX < 1"
#endif

#ifdef CONFIG_SOC_LS_ADSP_CSK6
#define BF30A2_FAST_CODE 
#else
#define BF30A2_FAST_CODE __attribute__ ((section(".itcm")))
#include "Driver_DVP.h"
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
    void *spi_hal;
    const struct device *dev;
    struct bf20a2_chip_data chip_data;
	struct k_work_delayable dwork;
	struct k_fifo fifo_free;
	struct k_fifo fifo_out;
	struct k_fifo fifo_vbuf_in;
	struct k_fifo fifo_vbuf_out;
    struct video_buffer *curr_vbuf;
};

extern struct bf30a2_dev_data bf30a2_dev_data;

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)

#ifdef CONFIG_SOC_LS_ADSP_CSK6
#include "hal/gpio_venus.h"
#else
#include "hal/gpio.h"
#endif
#include "hal/IOMuxManager.h"

#define SPI0_CS_PIN             16
#define SPI1_CS_PIN             9

void lower_spi_cs(uint8_t index)
{
    volatile uint32_t i = 1000;
    if (index == 0) {
        IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI0_CS_PIN, HAL_IOMUX_NONE_MODE);
        while(i > 0)    i--;
        IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI0_CS_PIN, HAL_IOMUX_PULLDOWN_MODE);
    } else if (index == 1) {
        IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI1_CS_PIN, HAL_IOMUX_PULLDOWN_MODE);
    }
}

void raise_spi_cs(uint8_t index)
{
    volatile uint32_t i = 1000;
    if (index == 0) {
        IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI0_CS_PIN, HAL_IOMUX_PULLUP_MODE);
        while(i > 0)    i--;
        IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI0_CS_PIN, HAL_IOMUX_NONE_MODE);
    } else if (index == 1) {
        IOMuxManager_ModeConfigure(CSK_IOMUX_PAD_A, SPI1_CS_PIN, HAL_IOMUX_PULLUP_MODE);
    }
}

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
	{ 0x2e, 0x57 },
	{ 0x07, 0x52 },
	{ 0x2c, 0x51 },
	{ 0x12, 0xc3 },
	{ 0x15, 0x80 },
	{ 0x6b, 0x71 },
	{ 0x04, 0x00 },
	{ 0x06, 0x26 },
	{ 0x08, 0x07 },
	{ 0x1c, 0x12 },
	{ 0x1e, 0xae },
	{ 0x1f, 0x00 },
	{ 0x20, 0x20 },
	{ 0x21, 0x20 },
	{ 0x34, 0x02 },
	{ 0x35, 0x02 },
	{ 0x36, 0x21 },
	{ 0x37, 0x13 },
	{ 0xca, 0x07 },
	{ 0xcb, 0x22 },
	{ 0xcc, 0x89 },
	{ 0xcd, 0x4c },
	{ 0xce, 0x6b },
	{ 0xcf, 0xf0 },
	{ 0xa0, 0x8e },
	{ 0x01, 0x1b },
	{ 0x02, 0x1d },
	{ 0x13, 0x08 },
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

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), pwms)
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
#endif

#define SPI1_CS_PIN 9
#include "hal/gpio.h"
#include "hal/IOMuxManager.h"

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
    return 0;
}
#endif

static int bf30a2_chip_init(const struct device *dev)
{
	int r;

	/** 
     * NOTE THAT: bf30a2 must aways be on bus i2c if the csk6_spec property is not defined.
     */
#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), pwms)
	r = bf30a2_chip_xclk_init(dev);
	if (r != 0) {
		LOG_ERR("XCLK init error.");
		return r;
	}
#endif

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

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)

/* TODO: Remove it */
#define RECV_BUFS_COUNT 1
static __attribute__((aligned(4))) uint8_t recv_bufs[RECV_BUFS_COUNT][25219 + 1] = {0};
static SPI_DEV *spi_hal;

static int bf30a2_decode_XX(char *in, char *out, uint32_t l, uint16_t h, uint16_t w);

enum {
	BF30A2_PACKET_TYPE_FRAME_START = 0x01,
	BF30A2_PACKET_TYPE_LINE_START = 0x02,
	BF30A2_PACKET_TYPE_FRAME_DATA = 0x40,
	BF30A2_PACKET_TYPE_FRAME_END = 0x00,
	BF30A2_PACKET_TYPE_FRAME_UNKNOWN = 0xFF,
} bf30a2_packet_type_t;

enum {
    MACHINE_STATE_INIT = 0,
    MACHINE_STATE_LINE_PACKET_RECEIVED,
    MACHINE_STATE_DATA_AND_LINE_PACKET_RECEIVED,
    MACHINE_STATE_DATA_AND_LINE_PACKET_RECEIVED_V2,
    MACHINE_STATE_END_AND_START_PACKET_RECEIVED,
	MACHINE_STATE_FIND_HEADING,
};

void bf30a2_spi_evt_callback(unsigned int  event_info, unsigned int  xfer_bytes, unsigned int  usr_param);

static int32_t spi_recv_fixed_dma_ch_install(SPI_DEV *spi, uint8_t ch)
{
    spi->info->rx_dyn_dma_ch =  dma_channel_reserve(ch, bf30a2_spi_evt_callback, 0, DMA_CACHE_SYNC_DST);
    if (spi->info->rx_dyn_dma_ch == DMA_CHANNEL_ANY) {
        LOG_ERR("%s: NO free DMA channel!!\r\n", __func__);
        return -1;
    }

    return 0;
}

int32_t spi_first_recv(SPI_DEV *spi, void *data, uint32_t num)
{
    int32_t stat;
    if (spi == NULL) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((data == NULL) || (num == 0U) || (num > MAX_TRANCNT))
        return CSK_DRIVER_ERROR_PARAMETER;

    if (!(spi->info->flags & SPI_FLAG_CONFIGURED))
        return CSK_DRIVER_ERROR;

    if (spi->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    // set busy flag
    spi->info->status.bit.busy = 1U;
    spi->info->status.bit.data_ovf = 0U;
    spi->info->status.bit.rx_mode = NO_IO;

    spi->info->xfer.rx_buf = data;
    spi->info->xfer.rx_cnt = 0U;
    spi->info->xfer.req_rx_cnt = num;
    spi->info->xfer.cur_op = SPI_RECEIVE;

    spi->reg->TRANSCTRL = (SPI_TRANSMODE_RDONLY | RD_TRANCNT(num) |
                (spi->reg->TRANSCTRL & SPI_TRANSMODE_CMD_EN));

    bool xfer_started = false;
    while ( RX_DMA(spi->info->txrx_mode) ) { // DMA RX
        bool dma_only = RX_DMA_ONLY(spi->info->txrx_mode);
        // initial the dma done flag
        spi->info->xfer.dma_rx_done = 0;
        // enable RX DMA
        spi->reg->CTRL |= RXDMAEN;
        if (spi->info->rx_dyn_dma_ch == DMA_CHANNEL_ANY) {
            LOG_ERR("%s: NO free DMA channel!!\r\n", __func__);
            if (dma_only)   return CSK_DRIVER_ERROR;
            break; // quit DMA RX
        }

        uint32_t control, config_low, config_high;
        control = DMA_CH_CTLL_DST_WIDTH(spi->info->dma_width_shift) | DMA_CH_CTLL_SRC_WIDTH(spi->info->dma_width_shift) |
                DMA_CH_CTLL_DST_BSIZE(spi->info->rx_bsize_shift) | DMA_CH_CTLL_SRC_BSIZE(spi->info->rx_bsize_shift) |
                DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |
                DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN;
        config_low = DMA_CH_CFGL_CH_PRIOR(1); // channel priority is higher than 0
        config_high = DMA_CH_CFGH_SRC_PER(spi->dma_rx.reqsel); // DMA_CH_CFGH_FIFO_MODE |
        
        stat = dma_channel_configure (spi->info->rx_dyn_dma_ch,
                            (uint32_t) (&(spi->reg->DATA)),
                            (uint32_t) spi->info->xfer.rx_buf,
                            num, control, config_low, config_high, 0, 0);
        if (stat == -1) {
            LOG_ERR("%s: Failed to call dma_channel_configure!!\r\n", __func__);
            dma_channel_disable(spi->info->rx_dyn_dma_ch, false);
            spi->info->rx_dyn_dma_ch = DMA_CHANNEL_ANY;
            if (dma_only)   return CSK_DRIVER_ERROR;
            break;
        }

        spi->reg->CTRL |= RXDMAEN;
        spi->reg->INTREN = SPI_ENDINT;
        spi->info->status.bit.rx_mode = DMA_IO;

        xfer_started = true;
        break;
    }

    if (!xfer_started && RX_PIO(spi->info->txrx_mode)) {
        spi->reg->INTREN = (SPI_RXFIFOINT | SPI_ENDINT);
        spi->info->status.bit.rx_mode = PIO_IO;
        xfer_started = true;
    }

    if (!xfer_started) {
        LOG_ERR("%s: Neither DMA nor PIO is started!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

	if((spi->info->txrx_mode & CSK_SPI_MODE_Msk) == CSK_SPI_MODE_SLAVE) {
	    CLR_SLV_RDCNT(spi); // clear RCnt on updated IP
	    spi->reg->INTREN |= SPI_RXFIFOORINT | SPI_SLVCMD;
	}

    if ((spi->info->txrx_mode & CSK_SPI_MODE_Msk) == CSK_SPI_MODE_MASTER) {
        spi->reg->CMD = 0;
    }

    return CSK_DRIVER_OK;
}

BF30A2_FAST_CODE static inline int32_t spi_recv(SPI_DEV *spi, void *data, uint32_t num)
{
    extern int32_t dma_fast_update_only_for_fix_channel(uint8_t ch,
        uint32_t src, uint32_t dst, uint32_t size);
    dma_fast_update_only_for_fix_channel(spi->info->rx_dyn_dma_ch,
        (uint32_t)(&(spi->reg->DATA)), (uint32_t)data, num);

    /* spi registers */
    spi->reg->TRANSCTRL = (SPI_TRANSMODE_RDONLY | RD_TRANCNT(num)
        | (spi->reg->TRANSCTRL & SPI_TRANSMODE_CMD_EN));
    spi->reg->CTRL |= RXDMAEN;
    spi->reg->INTREN = SPI_ENDINT;
    CLR_SLV_RDCNT(spi); // clear RCnt on updated IP
	spi->reg->INTREN |= SPI_RXFIFOORINT | SPI_SLVCMD;

	return CSK_DRIVER_OK;
}

BF30A2_FAST_CODE static ALWAYS_INLINE int bf30a2_recv_next_packet(char *buf, uint32_t buf_size)
{
    spi_recv(spi_hal, buf, buf_size);
    return 0;
}

static uint8_t next_state = MACHINE_STATE_INIT;
static uint16_t recv_lines = 0;
static uint32_t offset = 0;

BF30A2_FAST_CODE static ALWAYS_INLINE int bf30a2_recv_packet_process_v2(char *buf, uint8_t reset, uint8_t recv)
{
	int packet_received = 0;

	if (reset) {
		next_state = MACHINE_STATE_INIT;
		recv_lines = 0;
		offset = 0;
	}

    if (!recv) {
        return 0;
    }

	switch (next_state) {
	case MACHINE_STATE_INIT:
		next_state = MACHINE_STATE_DATA_AND_LINE_PACKET_RECEIVED;
        offset = 0;
		bf30a2_recv_next_packet(buf + offset, 6 + 29 * 140);
        offset += (6 + 29 * 140);
        recv_lines = 0;
		break;
	case MACHINE_STATE_DATA_AND_LINE_PACKET_RECEIVED:
        if (recv_lines >= 5) {
			next_state = MACHINE_STATE_DATA_AND_LINE_PACKET_RECEIVED_V2;
            bf30a2_recv_next_packet(buf + offset, (140 * 6));
            offset += 140 * 6;
        } else {
            next_state = MACHINE_STATE_DATA_AND_LINE_PACKET_RECEIVED;
            bf30a2_recv_next_packet(buf + offset, (140 * 29));
            offset += 140 * 29;
            recv_lines++;
        }
		break;
    case MACHINE_STATE_DATA_AND_LINE_PACKET_RECEIVED_V2:
        next_state = MACHINE_STATE_END_AND_START_PACKET_RECEIVED;
        bf30a2_recv_next_packet(buf + offset, (13));
        if (bf30a2_dev_data.curr_vbuf != NULL) {
            bf30a2_dev_data.curr_vbuf->bytesused = 25219;
            bf30a2_dev_data.curr_vbuf->timestamp = k_uptime_get_32();
            k_fifo_put(&bf30a2_dev_data.fifo_out, bf30a2_dev_data.curr_vbuf);
        } else {
            /* use static buffer. */
        }
        offset += 13;
        break;
    case MACHINE_STATE_END_AND_START_PACKET_RECEIVED:
        offset = 0;
        next_state = MACHINE_STATE_INIT;
        packet_received = 1;
        break;
	case MACHINE_STATE_FIND_HEADING:
	default:
		break;
	}

	return packet_received;
}

BF30A2_FAST_CODE static int32_t spi_wait_head(void *spi_dev, uint32_t timeout)
{
	SPI_DEV *spi = (SPI_DEV *)spi_dev;

	uint32_t start_time = k_uptime_get_32();

	int i = SPI_RXFIFO_ENTRIES(spi);

	uint8_t head_buf[5] = {0};
	uint8_t curr_idx = 0;

	while (1) {

		if(i<=0) {
			i = SPI_RXFIFO_ENTRIES(spi);
			continue;
		}

		head_buf[curr_idx++] = spi->reg->DATA;
		
		curr_idx = curr_idx % 5;
		uint8_t m_cnt = 0;

		for (size_t tmp = 0; tmp < 5; tmp++)
		{
			switch (tmp) {
			case 0:
				if(head_buf[(curr_idx + tmp)%5] == 0xFF) m_cnt++;
				break;
			case 1:
				if(head_buf[(curr_idx + tmp)%5] == 0xFF) m_cnt++;
				break;
			case 2:
				if(head_buf[(curr_idx + tmp)%5] == 0xFF) m_cnt++;
				break;
			case 3:
				if(head_buf[(curr_idx + tmp)%5] == 0x01) m_cnt++;
				break;
			case 4:
				if(head_buf[(curr_idx + tmp)%5] == 0x02) m_cnt++;
				break;
			default:
				break;
			}
		}

		if(m_cnt == 5) {
			break;
		}
		if(m_cnt%5){
			if(k_uptime_get_32() - start_time > timeout) return CSK_DRIVER_ERROR;
		}
		
		// auto decrease, re-evaluate i if down to 0
		if (--i == 0)
			i = SPI_RXFIFO_ENTRIES(spi);
	} // end while i

	return CSK_DRIVER_OK;
}

BF30A2_FAST_CODE void bf30a2_spi_evt_callback(unsigned int event_info, unsigned int  xfer_bytes, unsigned int  usr_param)
{
    char *buff = NULL;
    event_info &= 0xFF;

	if (event_info & DMA_EVENT_TRANSFER_COMPLETE) {
        spi_hal->reg->CTRL &= ~(TXDMAEN | RXDMAEN);
        spi_hal->reg->CTRL |= (TXFIFORST | RXFIFORST);

        if (bf30a2_dev_data.curr_vbuf != NULL && bf30a2_dev_data.curr_vbuf->buffer != NULL) {
            buff = bf30a2_dev_data.curr_vbuf->buffer;
        } else {
            buff = &recv_bufs[0][0];
        }

        /* A state-machine for receiving frames line by line */
        if (bf30a2_recv_packet_process_v2(buff, 0, 1)) {
            struct video_buffer *next_vbuf = k_fifo_get(&bf30a2_dev_data.fifo_free, K_NO_WAIT);
            if (next_vbuf != NULL && next_vbuf->buffer != NULL) {
                bf30a2_dev_data.curr_vbuf = next_vbuf;
                buff = next_vbuf->buffer;
            } else {
                buff = &recv_bufs[0][0];
            }
            
            /* restart state-machine to receive next frame */
		    bf30a2_recv_packet_process_v2(buff, 1, 1);
        }
	}
}

static int bf30a2_stream_start_internal(const struct device *dev)
{
    /* reset state-machine */
    bf30a2_recv_packet_process_v2(NULL, 1, 0);

    /* Skip first frame start packet. */
	return spi_first_recv(spi_hal, &recv_bufs[0][0], 9);
}

#endif

#define CSK_SPI1             ((CSK_SPI_RegDef *)  SPI1_BASE)

static int32_t bf30a2_restart();
static int bf30a2_driv_stream_start(const struct device *dev)
{
#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)
    int r = bf30a2_chip_config(dev);
	if (r != 0) {
		LOG_ERR("chip config error.");
		return r;
	}
    lower_spi_cs(1);
	bf30a2_chip_write_register(dev, 0xCF, 0xF0);
#endif

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
    bf30a2_stream_start_internal(dev);
    bf30a2_restart();
#endif

	return 0;
}

static int bf30a2_driv_stream_stop(const struct device *dev)
{
#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)
	raise_spi_cs(1);
	bf30a2_chip_write_register(dev, 0xCF, 0xF1);
#endif

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
    SPI_Control(spi_hal, CSK_SPI_ABORT_TRANSFER, 0);
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

	*buf = k_fifo_get(&data->fifo_vbuf_out, timeout);
	if (*buf == NULL) {
		return -EAGAIN;
	}

	return 0;
#else
	return -1;
#endif
}

static int bf30a2_driv_enqueue(const struct device *dev, enum video_endpoint_id ep,
			       struct video_buffer *buf)
{
#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	k_fifo_put(&data->fifo_vbuf_in, buf);

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

BF30A2_FAST_CODE
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

static int bf30a2_decode_XX(char *in, char *out, uint32_t l, uint16_t h, uint16_t w)
{
	uint16_t i = 0;
	uint32_t offset = 0;
	uint32_t len = 0;

	offset += 0;

    if (bf30a2_frame_check(in + offset, l - offset, BF30A2_PACKET_TYPE_LINE_START) != 0) {
        return -1;
    }
    offset += 6;
	for (i = 0; i < h; i++) {
		/* data packet */
		if (bf30a2_frame_check(in + offset, l - offset, BF30A2_PACKET_TYPE_FRAME_DATA) != 0) {
			return -1;
		}

		/* copy raw data to out buffer. */
        uint32_t _copy_size = w;
		char *dst = out + i * w;
		char *src = in + offset + BF30A2_DATA_PACKET_HEADA_LEN;
		uint32_t _size = _copy_size;
		while(_size--) {
			*dst++ = *src++;
		}
		offset += BF30A2_DATA_PACKET_HEADA_LEN + w;
		len += w;

        if (bf30a2_frame_check(in + offset, l - offset, BF30A2_PACKET_TYPE_LINE_START) != 0) {
			return -1;
		}
        offset += 6;
	}

	return len;
}

#if CONFIG_BF30A2_UNUSED_DEQUEUE
typedef struct {
    volatile char *addr;
    volatile uint32_t frame_size;
    volatile uint32_t frame_cnt;
    volatile uint32_t w_pos;
    volatile uint32_t r_pos;
    volatile uint32_t tag;
} __packed frames_info_t;

Z_GENERIC_SECTION(.csk6_psram_section1) __attribute__((aligned(32))) static char swp_frames[3][180 * 128] = {0};

static inline char *bf30a2_frames_info_get_free(frames_info_t *info)
{
    if (info == NULL || info->addr == NULL || info->frame_cnt <= 0) {
        return NULL;
    }
#ifndef CONFIG_BF30A2_DECODE_TEST
SCB_InvalidateDCache_by_Addr(info,sizeof(frames_info_t));
    if (info->w_pos < (info->frame_cnt - 1) || info->w_pos >= info->r_pos) {
        info->w_pos %= info->frame_cnt;
		info->r_pos %= info->frame_cnt;
    } else if ((info->w_pos == (info->frame_cnt - 1)) && (info->r_pos == 0)) {
        /* No more free space. */
        return NULL;
    } else if (info->r_pos  - info->w_pos == 1) {
        /* No more free space. */
        return NULL;
    }

#else
    info->w_pos = 0;
#endif
    info->w_pos %= info->frame_cnt;
    int offset = info->w_pos * info->frame_size;
    return info->addr + offset;
}

static inline int32_t bf30a2_frames_info_update_w_pos(frames_info_t *info)
{
    if (info == NULL || info->addr == NULL || info->frame_cnt <= 0) {
        return -1;
    }

    info->w_pos++;
    info->w_pos %= info->frame_cnt;
    SCB_CleanDCache_by_Addr(info, sizeof(frames_info_t));
    return info->w_pos;
}

#endif

/**
 * retval < 0  : decode error.
 * retval == 0 : no free buffer to decode.
 * retval >= 0 : decode success.
 */
static int32_t bf30a2_rda_frame_decode_proc(struct video_buffer *vbuf, void *p)
{
    int r = -1;

    cache_dma_fast_inv_stage1(vbuf->buffer, vbuf->buffer + vbuf->size);

#if CONFIG_BF30A2_UNUSED_DEQUEUE
    frames_info_t *frams_info = (frames_info_t *)p;
    char *free_buff = bf30a2_frames_info_get_free(frams_info);

    if (free_buff != NULL) {
        r = bf30a2_decode_XX(vbuf->buffer, free_buff, 25219, 179, 128);
        if (r > 0) {
            SCB_CleanDCache_by_Addr(free_buff, r);
            bf30a2_frames_info_update_w_pos(frams_info);
        }
    } else {
        LOG_ERR("no free space, info->w_pos:%u, info->r_pos:%u.", frams_info->w_pos,
            frams_info->r_pos);
        r = 0;
    }
#else
    struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)p;
    struct video_buffer *vbuf_out = k_fifo_get(&data->fifo_vbuf_in, K_MSEC(16));

    if (vbuf_out == NULL) {
        r = 0;
    }
    r = bf30a2_decode_XX(vbuf->buffer, vbuf_out->buffer, 25219, 180, 128);

    if (r > 0) {
        vbuf_out->bytesused = 128 * 180;
        vbuf_out->timestamp = vbuf->timestamp;
        k_fifo_put(&data->fifo_vbuf_out, vbuf_out);
    }
#endif

    return r;
}

static int32_t bf30a2_restart()
{
#if CONFIG_BF30A2_UNUSED_DEQUEUE
    frames_info_t *frams_info = (frames_info_t *)0x30704000;
    frams_info->w_pos = 0;
    frams_info->r_pos = 0;
    SCB_CleanDCache_by_Addr(frams_info, sizeof(frames_info_t));
#endif
    return 0;
}

static void bf30a2_thread(void *p1, void *p2, void *p3)
{
	__ASSERT(p1 != NULL, "Param is null.");
	struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)p1;
	struct video_buffer *vbuf;
	uint32_t cnt = 0;
	uint32_t last_time = 0;
	uint32_t error_cnt = 0;
	uint32_t ok_cnt = 0;
    uint32_t discard_cnt = 0;
	int fps = 0;
    int r;
    void *p;

#if CONFIG_BF30A2_UNUSED_DEQUEUE
    frames_info_t *frams_info = (frames_info_t *)0x30704000;
    memset(frams_info, 0, sizeof(frames_info_t));
    frams_info->addr = &swp_frames[0][0];
    frams_info->frame_size = 180 * 128;
    frams_info->frame_cnt = 3;
    frams_info->w_pos = 0;
    frams_info->r_pos = 0;
    frams_info->tag = 0x12345678;
    p = frams_info;

    LOG_INF("##########Frames info##########", frams_info);
    LOG_INF("info       :%p.", frams_info);
    LOG_INF("buffer addr:%p.", frams_info->addr);
    LOG_INF("frame  size:%u.", frams_info->frame_size);
    LOG_INF("frame count:%d.", frams_info->frame_cnt);
    LOG_INF("write   pos:%d.", frams_info->w_pos);
    LOG_INF("read    pos:%d.", frams_info->r_pos);
#else
    p = data;
#endif

	while (1) {
		vbuf = k_fifo_get(&data->fifo_out, K_FOREVER);
		if (vbuf != NULL && vbuf->buffer != NULL) {
			cnt++;
            r = bf30a2_rda_frame_decode_proc(vbuf, p);
			if (r > 0) {
                ok_cnt++;
			} else if (r < 0) {
				error_cnt++;
                /* TODO: to be optimized */
                next_state = MACHINE_STATE_FIND_HEADING;
				spi_wait_head(spi_hal, 3000);
				bf30a2_recv_packet_process_v2(NULL, 1, 0);
				spi_recv(spi_hal, &recv_bufs[0][0] + 5, 4);
			} else {
                discard_cnt++;
                LOG_ERR("No free space to decode frame.");
            }
            uint32_t frame_timestamp = vbuf->timestamp;
            /* free current vbuf */
            k_fifo_put(&data->fifo_free, vbuf);
            if (cnt % 30 == 0) {
                fps = frame_timestamp - last_time ? 1000 / (frame_timestamp - last_time) : 0;
                LOG_INF("FPS:%d, T:%d, O:%d, E:%d, D:%d.",
                    fps, cnt, ok_cnt, error_cnt, discard_cnt);
            }
			last_time = frame_timestamp;
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
    .enqueue = bf30a2_driv_enqueue,
	.set_ctrl = bf30a2_set_ctrl,
};

static int bf30a2_dev_init(const struct device *dev)
{
    struct bf30a2_dev_data *data = (struct bf30a2_dev_data *)dev->data;
    data->dev = dev;

#if DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, i2c)
    /* TODO: to be optimized */
    raise_spi_cs(1);
    IOMuxManager_PinConfigure(0, 7, 20);
    #define CP_DVP_BASE        (0x4A050000)
    DVP_TypeDef *hDvp = (DVP_TypeDef *)(CP_DVP_BASE);
    // Enable clockout with a divider of 4 (clockout = hclk/2/(4+1))
    hDvp->PIXEL_CLK_DIV = 4;
    hDvp->PIXEL_CLK_EN = 1;
#endif

	bf30a2_chip_init(dev);

#if !DT_PROP(DT_DRV_INST(0), csk6_spec) || DT_INST_ON_BUS(0, spi)

    /* TODO: to be optimized */
    spi_hal = SPI1();
    SPI_Uninitialize(spi_hal);	
    SPI_Initialize(spi_hal, NULL, (uint32_t)dev);
    SPI_PowerControl(spi_hal, CSK_POWER_FULL);
    SPI_Control(spi_hal, CSK_SPI_EXCL_OP_UNSET | CSK_SPI_MODE_SLAVE | CSK_SPI_RXIO_DMA |
                    CSK_SPI_CPOL0_CPHA0 | CSK_SPI_MSB_LSB | CSK_SPI_DATA_BITS(8), 0);
    /* Make sure dma channel 0 is always used for bf30a2. */
    spi_recv_fixed_dma_ch_install(spi_hal, 0);
    
    /* sr and thread */
	k_fifo_init(&data->fifo_free);
    k_fifo_init(&data->fifo_out);

	uint32_t i;
	for (i = 0; i < CONFIG_VIDEO_BUFFER_POOL_NUM_MAX / 2; i++) {
		struct video_buffer *vbuf = video_buffer_alloc(BF30A2_FRAME_SIZE);
		if (vbuf == NULL) {
			LOG_ERR("Unable to alloc video buffer, index:%d.", i);
			return -1;
		}

        k_fifo_put(&data->fifo_free, vbuf);
	}

    /* application and thread */
    k_fifo_init(&data->fifo_vbuf_in);
    k_fifo_init(&data->fifo_vbuf_out);

    for (i = 0; i < CONFIG_VIDEO_BUFFER_POOL_NUM_MAX / 2; i++) {
		struct video_buffer *vbuf = video_buffer_alloc(BF30A2_FRAME_SIZE);
		if (vbuf == NULL) {
			LOG_ERR("Unable to alloc video buffer, index:%d.", i);
			return -1;
		}

        k_fifo_put(&data->fifo_vbuf_in, vbuf);
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
	{      														\
		.bus = DEVICE_DT_GET(DT_PROP(node_id, spi)),			\
		.config = BF30A2_SPI_CONFIG_DT(DT_PROP(node_id, spi), operation_, delay_),   \
    }

#define BF30A2_PWM_DT_SPEC_GET(node_id)                                             \
	{                                                                                          \
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
		.xclk_freq = Z_COND_CODE_1(DT_NODE_HAS_PROP(DT_DRV_INST(inst), xclk_freq), (DT_PROP(DT_DRV_INST(inst), xclk_freq)),(0)),                                \
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

#if DT_NODE_HAS_STATUS(DT_DRV_INST(0), okay)

struct bf30a2_dev_data bf30a2_dev_data = {
	.chip_data = BF30A2_CHIP_DATA(0),
};

DEVICE_DT_INST_DEFINE(0, &bf30a2_dev_init, NULL, &bf30a2_dev_data, NULL, POST_KERNEL,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &bf30a2_driv_api);

K_THREAD_DEFINE(bf30a2_thread_id, 2048,                                \
			bf30a2_thread, &bf30a2_dev_data, NULL, NULL,               \
			K_HIGHEST_THREAD_PRIO, 0, 0);
#endif
