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

struct spi_csk6_fake_data {
	void *hal_dev;
};

static uint8_t g_buffer[230400] __attribute__((align(32))) = { 0 };

K_SEM_DEFINE(g_spi_fake_sem, 0, 1);

static int32_t csk_spi_trans_hal_2(void *hal_dev, uint8_t *datas, uint32_t len)
{
	int32_t r;
	uint32_t l = 0;
	uint32_t n;
	uint8_t temp;

	IOMuxManager_PinConfigure(1, 7, 14);
	IOMuxManager_PinConfigure(1, 8, 14);

	if (len <= 4) {
		l = ROUND_UP(len, 4);
		memset(g_buffer, 0, l);

		memcpy(g_buffer + l - len, datas, len);

		for (n = 0; n < l;) {
			temp = g_buffer[n + 0];
			g_buffer[n + 0] = g_buffer[n + 3];
			g_buffer[n + 3] = temp;

			temp = g_buffer[n + 1];
			g_buffer[n + 1] = g_buffer[n + 2];
			g_buffer[n + 2] = temp;

			n += 4;
		}

		// for (n = 720 * 20; n < l;) {
		// 	temp = g_buffer[n + 0];
		// 	g_buffer[n + 0] = g_buffer[n + 3];
		// 	g_buffer[n + 3] = temp;

		// 	temp = g_buffer[n + 1];
		// 	g_buffer[n + 1] = g_buffer[n + 2];
		// 	g_buffer[n + 2] = temp;

		// 	n += 4;
		// }

		uint32_t ctrl = CSK_I2S_MODE_MASTER | CSK_I2S_TDM_CHS(0) |
				CSK_I2S_TXCH_STEREO_SRC_MONO | CSK_I2S_DATA_FORMAT_32BIT;

		ctrl |= (CSK_I2S_PROTO_LEFT);
		r = I2S_Control(hal_dev, ctrl, 16000);

		if (r != 0) {
			printk("SPI Fake driver init error, %s:%d.\n", __FILE__, __LINE__);
			return -1;
		}
	}
	I2S_DEV *i2s = (I2S_DEV *)hal_dev;
	i2s->reg->REG_I2S_CFG0.bit.LRCK_POL = 1;
	i2s->reg->REG_I2S_CFG0.bit.BCKOUT_GATE = 1;
	i2s->reg->REG_I2S_CFG0.bit.EN_FORCE_ON = 0;
	i2s->reg->REG_I2S_CFG0.bit.BCK_FORCE_ON = 0;

	r = I2S_Send(hal_dev, g_buffer, 1, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);
	int k = arch_irq_lock();
	k_busy_wait(2);
	GPIO_PinWrite(GPIOA(), 1 << 9, 0);

	IOMuxManager_PinConfigure(1, 8, 0);
	IOMuxManager_PinConfigure(1, 7, 0);

	GPIO_SetDir(GPIOB(), 1 << 7, 1);
	GPIO_PinWrite(GPIOB(), 1 << 7, 1);

	GPIO_SetDir(GPIOB(), 1 << 8, 1);
	GPIO_PinWrite(GPIOB(), 1 << 8, 0);
	arch_irq_unlock(k);
	I2S_Abort_Channels(hal_dev, 0, CH_BMP_STEREO, 0);
	if (r != 0) {
		printk("I2S_Send error,r:%d\n", r);
		return r;
	}

	r = k_sem_take(&g_spi_fake_sem, K_MSEC(5000));
	if (r != 0) {
		printk("k_sem_take error,r:%d\n", r);
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

	printk("csk_spi_trans_hal, len:%d\n", len);
	if (len <= 4) {
		return csk_spi_trans_hal_2(hal_dev, datas, len);
	}

	I2S_DEV *i2s = (I2S_DEV *)hal_dev;
	l = ROUND_UP(len, 8);
	memset(g_buffer, 0, l);
	memcpy(g_buffer, datas, len);

	for (n = 0; n < l;) {
		temp = g_buffer[n + 0];
		g_buffer[n + 0] = g_buffer[n + 3];
		g_buffer[n + 3] = temp;

		temp = g_buffer[n + 1];
		g_buffer[n + 1] = g_buffer[n + 2];
		g_buffer[n + 2] = temp;

		n += 4;
	}
	IOMuxManager_PinConfigure(1, 7, 14);
	IOMuxManager_PinConfigure(1, 8, 14);
	r = I2S_Control(hal_dev, CSK_I2S_TXCH_STEREO_SRC_STEREO, 0);

	if (r != 0) {
		printk("I2S Control return error,r:%d\n", r);
		return r;
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

	if (0) {
		for (n = 0; n < 16; n++) {
			IOMuxManager_PinConfigure(1, 7, 14);
			IOMuxManager_PinConfigure(1, 8, 14);
			r = I2S_Send(hal_dev, g_buffer, 180 * 20, CH_BMP_STEREO,
				     I2S_TX_FLAG_START_NOW);

			int k = arch_irq_lock();
			k_busy_wait(180 * 20 * 4 / 2);
			GPIO_PinWrite(GPIOA(), 1 << 9, 0);

			IOMuxManager_PinConfigure(1, 8, 0);
			IOMuxManager_PinConfigure(1, 7, 0);

			GPIO_SetDir(GPIOB(), 1 << 7, 1);
			GPIO_PinWrite(GPIOB(), 1 << 7, 1);

			GPIO_SetDir(GPIOB(), 1 << 8, 1);
			GPIO_PinWrite(GPIOB(), 1 << 8, 0);
			arch_irq_unlock(k);
		}
	} else {
		IOMuxManager_PinConfigure(1, 7, 14);
		IOMuxManager_PinConfigure(1, 8, 14);
		r = I2S_Send(hal_dev, g_buffer, l / 4, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);

		// int k = arch_irq_lock();
		// k_busy_wait(l / 2);
		k_usleep(l / 2);
		GPIO_PinWrite(GPIOA(), 1 << 9, 0);
		IOMuxManager_PinConfigure(1, 8, 0);
		IOMuxManager_PinConfigure(1, 7, 0);

		GPIO_SetDir(GPIOB(), 1 << 7, 1);
		GPIO_PinWrite(GPIOB(), 1 << 7, 1);

		GPIO_SetDir(GPIOB(), 1 << 8, 1);
		GPIO_PinWrite(GPIOB(), 1 << 8, 0);
		// I2S_Abort_Channels(hal_dev, 0, CH_BMP_STEREO, 0);
		// arch_irq_unlock(k);
	}
	if (r != 0) {
		printk("I2S_Send return error,r:%d\n", r);
		return r;
	}
	r = k_sem_take(&g_spi_fake_sem, K_MSEC(5000));
	if (r != 0) {
		printk("k_sem_take error,r:%d\n", r);
		return r;
	}

	return r;
}

static int csk_spi_transceive(const struct device *dev, const struct spi_config *spi_cfg,
			      const struct spi_buf_set *tx_bufs, const struct spi_buf_set *rx_bufs)
{
	uint32_t i;
	int32_t r;
	const struct spi_buf *buf = tx_bufs->buffers;
	struct spi_csk6_fake_data *data = (struct spi_csk6_fake_data *)dev->data;

	for (i = 0; i < tx_bufs->count; i++, buf++) {
		r = csk_spi_trans_hal(data->hal_dev, buf->buf, buf->len);
		if (r != 0) {
			return r;
		}
	}

	return 0;
}

static const struct spi_driver_api csk_spi_driver_api = {
	.transceive = csk_spi_transceive,
};

static void cb_i2s_oneshot_play_event(uint32_t event_info, uint32_t usr_param)
{
	const struct device *dev = (const struct device *)usr_param;
	struct spi_csk6_fake_data *data = (struct spi_csk6_fake_data *)dev->data;
	uint8_t event, ch_no;

	event = event_info & 0xFF;
	ch_no = (event_info >> 8) & 0xFF;

	if (event & CSK_I2S_EVENT_TRANSMIT_COMPLETE) {
		GPIO_PinWrite(GPIOB(), 1 << 3, 1);
		GPIO_PinWrite(GPIOB(), 1 << 3, 0);
	}

	if (event & CSK_I2S_EVENT_TX_FIFO_UNDERRUN) {
		I2S_DEV *i2s = (I2S_DEV *)data->hal_dev;
		i2s->reg->REG_I2S_CFG0.bit.ENABLE = 0;
		k_sem_give(&g_spi_fake_sem);
	}

	if (event & CSK_I2S_EVENT_TX_FIFO_EMPTY) {
	}
}

static int spi_csk_fake_init(const struct device *dev)
{
	int32_t r;
	struct spi_csk6_fake_data *data = (struct spi_csk6_fake_data *)dev->data;

	IOMuxManager_PinConfigure(1, 3, 0);
	GPIO_SetDir(GPIOB(), 1 << 3, 1);

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
	if (r != 0) {
		printk("SPI Fake driver init error, %s:%d.\n", __FILE__, __LINE__);
		return -1;
	}

	r = I2S_PowerControl(data->hal_dev, CSK_POWER_FULL);
	if (r != 0) {
		printk("SPI Fake driver init error, %s:%d.\n", __FILE__, __LINE__);
		return -1;
	}

	uint32_t ctrl = CSK_I2S_MODE_MASTER | CSK_I2S_TDM_CHS(4) | CSK_I2S_TXCH_STEREO_SRC_STEREO |
			CSK_I2S_DATA_FORMAT_DUAL_16BIT;

	ctrl |= (CSK_I2S_PROTO_PCMMODE_B << CSK_I2S_PROTO_Pos);
	r = I2S_Control(data->hal_dev, ctrl, 16000);

	if (r != 0) {
		printk("SPI Fake driver init error, %s:%d.\n", __FILE__, __LINE__);
		return -1;
	}

	printk("SPI FAKE Driver Init OK.\n");

	return 0;
}

static struct spi_csk6_fake_data g_spi_csk6_fake_data = { 0 };

DEVICE_DT_INST_DEFINE(0, spi_csk_fake_init, NULL, &g_spi_csk6_fake_data, NULL, POST_KERNEL,
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &csk_spi_driver_api);
