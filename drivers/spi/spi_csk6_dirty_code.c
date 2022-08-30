/*
 * Copyright (c) 2022 LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "hal/dma.h"
#include "hal/spi.h"

#include <zephyr/zephyr.h>

typedef struct {
	__IO DWORD_REG(SAR);
	__IO DWORD_REG(DAR);
	__IO DWORD_REG(LLP);
	__IO uint32_t CTL_LO;
	__IO uint32_t CTL_HI;
	__IO DWORD_REG(SSTAT);
	__IO DWORD_REG(DSTAT);
	__IO DWORD_REG(SSTATAR);
	__IO DWORD_REG(DSTATAR);
	__IO uint32_t CFG_LO;
	__IO uint32_t CFG_HI;
	__IO DWORD_REG(SGR);
	__IO DWORD_REG(DSR);
} DMA_CHANNEL_REG;

typedef struct {
	__IO DWORD_REG(XFER);
	__IO DWORD_REG(BLOCK);
	__IO DWORD_REG(SRC_TRAN);
	__IO DWORD_REG(DST_TRAN);
	__IO DWORD_REG(ERROR);
} DMA_IRQ_REG;

typedef struct {
	DMA_CHANNEL_REG CHANNEL[DMA_MAX_NR_CHANNELS];
	DMA_IRQ_REG RAW;
	DMA_IRQ_REG STATUS;
	DMA_IRQ_REG MASK;
	DMA_IRQ_REG CLEAR;

	__IO DWORD_REG(STA_INT);
	__IO DWORD_REG(REQ_SRC);
	__IO DWORD_REG(REQ_DST);
	__IO DWORD_REG(SGL_REQ_SRC);
	__IO DWORD_REG(SGL_REQ_DST);
	__IO DWORD_REG(LAST_SRC);
	__IO DWORD_REG(LAST_DST);

	__IO DWORD_REG(CFG);
	__IO DWORD_REG(CH_EN);
	__IO DWORD_REG(ID);
	__IO DWORD_REG(TEST);

	__IO DWORD_REG(__RSVD0);
	__IO DWORD_REG(__RSVD1);

	__I uint64_t COMP_PARAMS[6];
	__I uint64_t COMP_VER;

} DMA_RegMap;

#define CSK_DMA ((DMA_RegMap *)DMAC_BASE)
#define DMA_CHANNEL(n) ((DMA_CHANNEL_REG *)&(CSK_DMA->CHANNEL[n]))

int32_t dma_ch_config(uint8_t ch, uint32_t src_addr, uint32_t dst_addr, uint32_t total_size,
		      uint32_t control, uint32_t config_low, uint32_t config_high,
		      uint32_t src_gath, uint32_t dst_scat)
{
	uint8_t ch_bit = 1 << ch;

	SCB_CleanDCache_by_Addr((uint32_t *)src_addr, total_size);

	/* clear all interrupts */
	CSK_DMA->CLEAR.XFER = (ch_bit << 8);
	CSK_DMA->CLEAR.BLOCK = (ch_bit << 8);
	CSK_DMA->CLEAR.SRC_TRAN = (ch_bit << 8);
	CSK_DMA->CLEAR.DST_TRAN = (ch_bit << 8);
	CSK_DMA->CLEAR.ERROR = (ch_bit << 8);

	CSK_DMA->MASK.XFER = (ch_bit << 8);
	CSK_DMA->MASK.BLOCK = (ch_bit << 8);
	CSK_DMA->MASK.SRC_TRAN = (ch_bit << 8);
	CSK_DMA->MASK.DST_TRAN = (ch_bit << 8);
	CSK_DMA->MASK.ERROR = (ch_bit << 8);

	DMA_CHANNEL_REG *dma_ch = DMA_CHANNEL(ch);
	dma_ch->CTL_HI = (total_size & DMA_CH_CTLH_BLOCK_TS_MASK);

	dma_ch->SAR = src_addr;
	dma_ch->DAR = dst_addr;

	/* Write control & configuration etc. registers */
	dma_ch->CTL_LO = control | DMA_CH_CTLL_INT_EN; // Enable interrupt
	dma_ch->CFG_LO = config_low;
	dma_ch->CFG_HI = config_high;

	/* Reset LLP, use mock BLOCK transfer(NOT real LLP !!) */
	dma_ch->LLP = 0;

	/* Reset scatter/gather etc. registers */
	dma_ch->SGR = 0;
	dma_ch->DSR = 0;

	CSK_DMA->CH_EN = (ch_bit << 8) | ch_bit;

	return 0;
}

int32_t dma_ch_wait_finish(uint8_t ch, uint32_t _timeout)
{
	return 0;
}

int32_t spi_send_data(void *spi_dev, const void *data, uint32_t num)
{
	SPI_DEV *spi = (spi_dev);
	if (spi == NULL) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	int32_t stat;

	if ((data == NULL) || (num == 0U) || (num > MAX_TRANCNT))
		return CSK_DRIVER_ERROR_PARAMETER;

	if (!(spi->info->flags & SPI_FLAG_CONFIGURED))
		return CSK_DRIVER_ERROR;

	if (spi->info->status.bit.busy)
		return CSK_DRIVER_ERROR_BUSY;

	spi->info->status.bit.busy = 1U;
	spi->info->status.bit.data_unf = 0;
	spi->info->status.bit.tx_mode = NO_IO;

	spi->info->xfer.tx_buf = data;
	spi->info->xfer.tx_cnt = 0U;
	spi->info->xfer.req_tx_cnt = num;
	spi->info->xfer.cur_op = SPI_SEND;

	spi->reg->TRANSCTRL = (SPI_TRANSMODE_WRONLY | WR_TRANCNT(num) |
			       (spi->reg->TRANSCTRL & SPI_TRANSMODE_CMD_EN));

	bool xfer_started = false;
	while (TX_DMA(spi->info->txrx_mode)) {
		bool dma_only = TX_DMA_ONLY(spi->info->txrx_mode);
		spi->info->xfer.dma_tx_done = 0;
		spi->info->tx_dyn_dma_ch = spi->dma_tx.channel;
		spi->info->tx_dyn_dma_ch = CONFIG_SPI_TX_DMA_FIX_CHANNEL;

		if (!dma_channel_is_reserved(spi->info->tx_dyn_dma_ch)) {
			if (dma_channel_reserve(spi->info->tx_dyn_dma_ch, NULL, 0, 0) !=
			    spi->info->tx_dyn_dma_ch) {
				printk("Register fixed channel(%d) for spi failed.\n",
				       spi->info->tx_dyn_dma_ch);
				return -EINVAL;
			}
		}

		if (spi->info->tx_dyn_dma_ch == DMA_CHANNEL_ANY) {
			printk("%s: NO free DMA channel!!\r\n", __func__);
			if (dma_only)
				return CSK_DRIVER_ERROR;
			break;
		}

		uint32_t control, config_low, config_high;
		control = DMA_CH_CTLL_DST_WIDTH(spi->info->dma_width_shift) |
			  DMA_CH_CTLL_SRC_WIDTH(spi->info->dma_width_shift) |
			  DMA_CH_CTLL_DST_BSIZE(spi->info->tx_bsize_shift) |
			  DMA_CH_CTLL_SRC_BSIZE(spi->info->tx_bsize_shift) | DMA_CH_CTLL_DST_FIX |
			  DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P | DMA_CH_CTLL_DMS(0) |
			  DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN;
		config_low = DMA_CH_CFGL_CH_PRIOR(1);
		config_high = DMA_CH_CFGH_DST_PER(spi->dma_tx.reqsel);

		stat = dma_ch_config(spi->info->tx_dyn_dma_ch, (uint32_t)spi->info->xfer.tx_buf,
				     (uint32_t)(&(spi->reg->DATA)), num, control, config_low,
				     config_high, 0, 0);

		if (stat == -1) {
			printk("%s: Failed to call dma_channel_configure!!\r\n", __func__);
			dma_channel_disable(spi->info->tx_dyn_dma_ch, false);
			spi->info->tx_dyn_dma_ch = DMA_CHANNEL_ANY;
			if (dma_only)
				return CSK_DRIVER_ERROR;
			break;
		}

		spi->reg->CTRL |= TXDMAEN;

		spi->reg->INTREN = SPI_ENDINT;

		spi->info->status.bit.tx_mode = DMA_IO;
		xfer_started = true;
		break;
	}

	if (!xfer_started && TX_PIO(spi->info->txrx_mode)) {
		spi->reg->INTREN = (SPI_TXFIFOINT | SPI_ENDINT);

		spi->info->status.bit.tx_mode = PIO_IO;
		xfer_started = true;
	}

	if (!xfer_started) {
		printk("%s: Neither DMA nor PIO is started!!\r\n", __func__);
		return CSK_DRIVER_ERROR;
	}

	if ((spi->info->txrx_mode & CSK_SPI_MODE_Msk) == CSK_SPI_MODE_SLAVE) {
		CLR_SLV_WRCNT(spi);
		spi->reg->INTREN |= SPI_TXFIFOURINT;
		spi->reg->SLVST |= SLVST_READY;
	}

	if ((spi->info->txrx_mode & CSK_SPI_MODE_Msk) == CSK_SPI_MODE_MASTER) {
		while (SPI_TXFIFO_EMPTY(spi));
		spi->reg->CMD = 0;
	}

	int32_t r = dma_ch_wait_finish(spi->info->tx_dyn_dma_ch, 100);
	spi->info->status.bit.busy = 0;

	return r;
}