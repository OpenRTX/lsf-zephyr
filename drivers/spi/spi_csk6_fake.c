/*
 * Copyright(C) 2022 listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_spifake

#include <assert.h>
#include <kernel.h>
#include <errno.h>
#include <drivers/spi.h>
#include <drivers/clock_control.h>
#include <drivers/pinmux.h>
#include <soc_dt.h>

#include "Driver_I2S.h"
#include "i2s_venus.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_spi_fake, CONFIG_SPI_LOG_LEVEL);

#include "spi_context.h"
#include "kernel.h"

#include "apc_inner_venus.h"

struct spi_csk6_fake_data {
	void *hal_dev;
};

struct spi_csk6_fake_config {
	const struct csk6_pinctrl_cfg *pinctrl_list;
	uint8_t pinctrl_list_size;
};

K_SEM_DEFINE(g_spi_fake_sem, 0, 1);
K_MUTEX_DEFINE(g_spi_fake_mutex);

static int32_t csk_spi_trans_hal_3(void *hal_dev, uint8_t *datas, uint32_t len)
{
	int32_t r;
	uint8_t temp;
	uint8_t buf[4] = { 0 };

	IOMuxManager_PinConfigure(1, 7, 14);
	IOMuxManager_PinConfigure(1, 8, 14);

	memcpy(buf + 4 - len, datas, len);

	temp = buf[0];
	buf[0] = buf[3];
	buf[3] = temp;
	temp = buf[1];
	buf[1] = buf[2];
	buf[2] = temp;

	uint32_t ctrl = CSK_I2S_MODE_MASTER | CSK_I2S_TDM_CHS(0) | CSK_I2S_TXCH_STEREO_SRC_MONO |
			CSK_I2S_DATA_FORMAT_32BIT | CSK_I2S_PROTO_LEFT;

	r = I2S_Control(hal_dev, ctrl, 16000);

	if (r != 0) {
		printk("SPI Fake driver init error, %s:%d.\n", __FILE__, __LINE__);
		return -1;
	}
	I2S_DEV *i2s = (I2S_DEV *)hal_dev;
	i2s->reg->REG_I2S_CFG0.bit.LRCK_POL = 1;
	i2s->reg->REG_I2S_CFG1.bit.BCK_DIV = 1;
	i2s->reg->REG_I2S_CFG1.bit.BCK_DIV_LD = 1;

	r = I2S_Send(hal_dev, buf, 1, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);
	int k = arch_irq_lock();
	i2s->reg->REG_I2S_CFG0.bit.ENABLE = 1;
	while (!CSK_APC->REG_APC_INTR_TX_IRSR.bit.TX_CH0_L_FIFO_EMP_IRSR)
		;
	IOMuxManager_PinConfigure(1, 8, 0);
	IOMuxManager_PinConfigure(1, 7, 0);

	GPIO_SetDir(GPIOB(), 1 << 7, 1);
	GPIO_PinWrite(GPIOB(), 1 << 7, 1);

	GPIO_SetDir(GPIOB(), 1 << 8, 1);
	GPIO_PinWrite(GPIOB(), 1 << 8, 0);
	arch_irq_unlock(k);
	if (r != 0) {
		printk("I2S_Send error,r:%d\n", r);
		return r;
	}

	return 0;
}

static int32_t csk_spi_trans_hal_2(void *hal_dev, uint8_t *datas, uint32_t len)
{
	int32_t r;
	uint8_t temp;
	uint8_t buf[4] = { 0 };

	IOMuxManager_PinConfigure(1, 7, 14);
	IOMuxManager_PinConfigure(1, 8, 14);

	memcpy(buf, datas, len);

	temp = buf[0];
	buf[0] = buf[3];
	buf[3] = temp;
	temp = buf[1];
	buf[1] = buf[2];
	buf[2] = temp;

	uint32_t ctrl = CSK_I2S_MODE_MASTER | CSK_I2S_TDM_CHS(0) | CSK_I2S_TXCH_STEREO_SRC_MONO |
			CSK_I2S_DATA_FORMAT_32BIT | CSK_I2S_PROTO_LEFT;

	r = I2S_Control(hal_dev, ctrl, 16000);

	if (r != 0) {
		printk("SPI Fake driver init error, %s:%d.\n", __FILE__, __LINE__);
		return -1;
	}
	I2S_DEV *i2s = (I2S_DEV *)hal_dev;
	i2s->reg->REG_I2S_CFG0.bit.LRCK_POL = 1;
	i2s->reg->REG_I2S_CFG1.bit.BCK_DIV = 2;
	i2s->reg->REG_I2S_CFG1.bit.BCK_DIV_LD = 1;

	r = I2S_Send(hal_dev, buf, 1, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);
	int k = arch_irq_lock();
	i2s->reg->REG_I2S_CFG0.bit.ENABLE = 1;
	k_busy_wait(len);
	IOMuxManager_PinConfigure(1, 8, 0);
	IOMuxManager_PinConfigure(1, 7, 0);
	GPIO_SetDir(GPIOB(), 1 << 7, 1);
	GPIO_PinWrite(GPIOB(), 1 << 7, 1);
	GPIO_SetDir(GPIOB(), 1 << 8, 1);
	GPIO_PinWrite(GPIOB(), 1 << 8, 0);
	while (!CSK_APC->REG_APC_INTR_TX_IRSR.bit.TX_CH0_L_FIFO_EMP_IRSR);
	arch_irq_unlock(k);
	if (r != 0) {
		printk("I2S_Send error,r:%d\n", r);
		return r;
	}

	return 0;
}

static int32_t csk_spi_trans_hal(void *hal_dev, uint8_t *datas, uint32_t len)
{
	int32_t r;
	uint32_t n;
	uint8_t temp;
	uint32_t l;
	I2S_DEV *i2s = (I2S_DEV *)hal_dev;
	uint8_t *buf = NULL;

	printk("spi send %d datas.\n", len);

	if (len == 1 && (*datas == 0x2a || *datas == 0x2b || *datas == 0x2c)) {
		return csk_spi_trans_hal_3(hal_dev, datas, len);
	}

	if (len < 4) {
		return csk_spi_trans_hal_2(hal_dev, datas, len);
	}

	if (len == 4) {
		return csk_spi_trans_hal_3(hal_dev, datas, len);
	}
	l = ROUND_UP(len, 8) + 128;
	buf = k_malloc(l);
	if (buf == NULL) {
		printk("malloc buffer faild.\n");
		return -1;
	}
	memcpy(buf, datas, len);
	memcpy(buf + len, datas, l - len);

	for (n = 0; n < l;) {
		temp = buf[n + 0];
		buf[n + 0] = buf[n + 3];
		buf[n + 3] = temp;

		temp = buf[n + 1];
		buf[n + 1] = buf[n + 2];
		buf[n + 2] = temp;
		n += 4;
	}

	uint32_t ctrl = CSK_I2S_MODE_MASTER | CSK_I2S_TDM_CHS(4) | CSK_I2S_TXCH_STEREO_SRC_STEREO |
			CSK_I2S_DATA_FORMAT_32BIT;

	ctrl |= (CSK_I2S_PROTO_PCMMODE_B << CSK_I2S_PROTO_Pos);
	r = I2S_Control(hal_dev, ctrl, 16000);
	if (r != 0) {
		printk("I2S Control return error,r:%d\n", r);
		return r;
	}
	i2s->reg->REG_I2S_CFG1.bit.SLOTNUM = 180;
	i2s->reg->REG_I2S_CFG1.bit.BCK_DIV = 1;
	i2s->reg->REG_I2S_CFG1.bit.BCK_DIV_LD = 1;
	IOMuxManager_PinConfigure(1, 7, 14);
	IOMuxManager_PinConfigure(1, 8, 14);
	r = I2S_Send(hal_dev, buf, l / 4, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);
	i2s->reg->REG_I2S_CFG0.bit.ENABLE = 1;
	k_sem_reset(&g_spi_fake_sem);
	r = k_sem_take(&g_spi_fake_sem, K_MSEC(5000));

	if (r != 0) {
		k_free(buf);
		printk("I2S_Send return error,r:%d\n", r);
		return r;
	}

	int k = arch_irq_lock();
	while (!CSK_APC->REG_APC_INTR_TX_IRSR.bit.TX_CH0_L_FIFO_EMP_IRSR)
		;
	IOMuxManager_PinConfigure(1, 7, 0);
	GPIO_SetDir(GPIOB(), 1 << 7, 1);
	GPIO_PinWrite(GPIOB(), 1 << 7, 1);
	arch_irq_unlock(k);

	IOMuxManager_PinConfigure(1, 8, 0);
	GPIO_SetDir(GPIOB(), 1 << 8, 1);
	GPIO_PinWrite(GPIOB(), 1 << 8, 0);

	if (r != 0) {
		k_free(buf);
		printk("I2S_Send return error,r:%d\n", r);
		return r;
	}

	k_free(buf);

	return r;
}

static int csk_spi_transceive(const struct device *dev, const struct spi_config *spi_cfg,
			      const struct spi_buf_set *tx_bufs, const struct spi_buf_set *rx_bufs)
{
	uint32_t i;
	int32_t r;
	const struct spi_buf *buf = tx_bufs->buffers;
	struct spi_csk6_fake_data *data = (struct spi_csk6_fake_data *)dev->data;

	k_mutex_lock(&g_spi_fake_mutex, K_FOREVER);
	for (i = 0; i < tx_bufs->count; i++, buf++) {
		r = csk_spi_trans_hal(data->hal_dev, buf->buf, buf->len);
		if (r != 0) {
			return r;
		}
	}
	k_mutex_unlock(&g_spi_fake_mutex);

	return 0;
}

static void cb_i2s_oneshot_play_event(uint32_t event_info, uint32_t usr_param)
{
	const struct device *dev = (const struct device *)usr_param;
	struct spi_csk6_fake_data *data = (struct spi_csk6_fake_data *)dev->data;
	uint8_t event, ch_no;

	event = event_info & 0xFF;
	ch_no = (event_info >> 8) & 0xFF;

	if (event & CSK_I2S_EVENT_TRANSMIT_COMPLETE) {
		k_sem_give(&g_spi_fake_sem);
	}

	if (event & CSK_I2S_EVENT_TX_FIFO_UNDERRUN) {
		I2S_DEV *i2s = (I2S_DEV *)data->hal_dev;
		i2s->reg->REG_I2S_CFG0.bit.ENABLE = 0;
	}

	if (event & CSK_I2S_EVENT_TX_FIFO_EMPTY) {
	}
}

static const struct spi_driver_api csk_spi_driver_api = {
	.transceive = csk_spi_transceive,
};

static int spi_csk_fake_init(const struct device *dev)
{
	int32_t r;
	struct spi_csk6_fake_data *data = (struct spi_csk6_fake_data *)dev->data;
	struct spi_csk6_fake_config *config = (struct spu_csk6_fake_config *)dev->config;


	// PINCTRL_CONFIG_PINS(config->pinctrl_list, config->pinctrl_list_size);
	/* LRCK */
	IOMuxManager_PinConfigure(1, 7, 14);
	/* BCK */
	IOMuxManager_PinConfigure(1, 8, 14);
	/* DOUT */
	IOMuxManager_PinConfigure(1, 1, 14);
	/* DIN */
	IOMuxManager_PinConfigure(1, 2, 14);
	/* MCLK */
	IOMuxManager_PinConfigure(1, 0, 14);

	data->hal_dev = I2S1();

	r = I2S_Initialize(data->hal_dev, cb_i2s_oneshot_play_event, (uint32_t)dev, 0,
			   CH_BMP_STEREO, 0, 0);

	r |= I2S_PowerControl(data->hal_dev, CSK_POWER_FULL);
	if (r != 0) {
		LOG_ERR("Spu fake driver init failed.");
		return -1;
	}

	LOG_DEBUG("Spi fake driver init OK.");

	return 0;
}

// static const struct csk6_pinctrl_cfg g_spi_csk6_fake_pinctrl_list[DT_INST_NUM_PINCTRLS_BY_IDX(0, 0)] = CSK6_DT_ALT_ITEMS_LIST(0);

static struct spi_csk6_fake_data g_spi_csk6_fake_data = { 0 };
static struct spi_csk6_fake_config g_spi_csk6_fake_config = {
	// .pinctrl_list = &g_spi_csk6_fake_pinctrl_list,
	// .pinctrl_list_size = ARRAY_SIZE(g_spi_csk6_fake_pinctrl_list),
};

DEVICE_DT_INST_DEFINE(0, spi_csk_fake_init, NULL, &g_spi_csk6_fake_data, &g_spi_csk6_fake_config,
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &csk_spi_driver_api);
