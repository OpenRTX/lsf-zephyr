#include "display_csk6_lcd.h"
#include <stdint.h>
#include <zephyr/kernel.h>
#include <logging/log.h>

#define LCD_SPI_FAST_CODE __attribute__((section(".itcm")))

struct display_csk6_lcd_context {
	const uint8_t *buffer;
	uint32_t h;
	uint32_t w;
	uint32_t start_offset;
	uint32_t line_size;
	uint32_t line_cnt;
	uint32_t curr_cnt;
	uint32_t step_offset;
	int32_t interval;
};

struct display_csk6_lcd_data {
	void *spi;
	uint8_t dma_ch;
	uint8_t cmd;
	struct display_csk6_lcd_context ctx;
	struct k_sem spi_done_sem;
	struct k_mutex lock;
};

LOG_MODULE_REGISTER(display_csk6_lcd);

struct display_csk6_lcd_data g_display_csk6_lcd_data = {0};

static LCD_SPI_FAST_CODE int32_t display_csk6_lcd_send_datas_with_dma(void *spi_dev, uint8_t dma_ch,
								      const uint8_t *data, uint32_t num,
								      int32_t interval)
{
	SPI_DEV *spi = (spi_dev);
	int32_t stat;

	if ((data == NULL) || (num == 0U) || (num > MAX_TRANCNT))
		return CSK_DRIVER_ERROR_PARAMETER;

	if (!(spi->info->flags & SPI_FLAG_CONFIGURED))
		return CSK_DRIVER_ERROR;

	if (spi->info->status.bit.busy)
		return CSK_DRIVER_ERROR_BUSY;

	// set busy flag
	spi->info->status.bit.busy = 1U;
	spi->info->status.bit.data_unf = 0;
	spi->info->status.bit.tx_mode = NO_IO;
	spi->info->xfer.tx_buf = data;
	spi->info->xfer.tx_cnt = 0U;
	spi->info->xfer.req_tx_cnt = num;
	spi->info->xfer.cur_op = SPI_SEND;

	while ((spi->reg->STATUS) & 0x1)
		;

	spi->reg->TRANSCTRL = (SPI_TRANSMODE_WRONLY | WR_TRANCNT(num) |
			       (spi->reg->TRANSCTRL & SPI_TRANSMODE_CMD_EN));

	spi->info->xfer.dma_tx_done = 0;
	uint32_t is_gather = 0;
	uint32_t control = DMA_CH_CTLL_DST_WIDTH(spi->info->dma_width_shift) |
			   DMA_CH_CTLL_SRC_WIDTH(spi->info->dma_width_shift) |
			   DMA_CH_CTLL_DST_BSIZE(spi->info->tx_bsize_shift) |
			   DMA_CH_CTLL_SRC_BSIZE(spi->info->tx_bsize_shift) | DMA_CH_CTLL_DST_FIX |
			   DMA_CH_CTLL_TTFC_M2P | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) |
			   DMA_CH_CTLL_INT_EN;

	if (interval) {
		is_gather = (interval) | (1) << 20;
		control |= DMA_CH_CTLL_S_GATH_EN | DMA_CH_CTLL_SRC_DEC;
	} else {
		control |= DMA_CH_CTLL_SRC_INC;
	}

	const uint32_t config_low = DMA_CH_CFGL_CH_PRIOR(1);
	const uint32_t config_high = DMA_CH_CFGH_DST_PER(spi->dma_tx.reqsel);

	spi->info->tx_dyn_dma_ch = dma_ch;
	stat = dma_channel_configure(spi->info->tx_dyn_dma_ch, (uint32_t)spi->info->xfer.tx_buf,
				     (uint32_t)(&(spi->reg->DATA)), num, control, config_low,
				     config_high, is_gather, 0);

	if (stat != 0) {
		LOG_ERR("dma ch %d config failed, ret:%d", spi->info->tx_dyn_dma_ch, stat);
		return stat;
	}

	spi->reg->CTRL |= TXDMAEN;
	// enable interrupts
	spi->reg->INTREN = SPI_ENDINT;
	spi->info->status.bit.tx_mode = DMA_IO;
	while (SPI_TXFIFO_EMPTY(spi))
		;
	spi->reg->CMD = 0;

	return CSK_DRIVER_OK;
}

static LCD_SPI_FAST_CODE void display_csk6_lcd_spi_isr_callback(uint32_t event, uint32_t usr_param)
{
	if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
		struct display_csk6_lcd_data *data = (struct display_csk6_lcd_data *)usr_param;

		if (data->cmd) {
			if (data->ctx.curr_cnt < data->ctx.line_cnt) {
				data->ctx.buffer += data->ctx.step_offset;

				display_csk6_lcd_send_datas_with_dma(
					data->spi, data->dma_ch, data->ctx.buffer,
					data->ctx.line_size / 2, data->ctx.interval);

				data->ctx.curr_cnt++;
			} else {
				k_sem_give(&data->spi_done_sem);
			}
		} else {
			k_sem_give(&data->spi_done_sem);
		}
	}
}

static int32_t display_csk6_lcd_context_init(struct display_csk6_lcd_context *ctx, const uint8_t *buff,
					     uint32_t h, uint32_t w, uint8_t rotate)
{
	if (ctx == NULL) {
		return -EINVAL;
	}

	ctx->buffer = buff;
	ctx->curr_cnt = 1;
	ctx->h = h;
	ctx->w = w;

	if (rotate) {
		ctx->line_cnt = w;
		ctx->line_size = h * 2;
		ctx->step_offset = 1 * 2;
		ctx->interval = w - 1;
		ctx->start_offset = ctx->w * (ctx->h - 1) * 2;
	} else {
		ctx->line_cnt = h;
		ctx->line_size = w * 2;
		ctx->step_offset = w * 2;
		ctx->interval = 0;
		ctx->start_offset = 0;
	}

	return 0;
}

int32_t LCD_SPI_FAST_CODE display_csk6_lcd_cmd_datas_send(void *handler, uint8_t *datas,
							  uint32_t len)
{
	uint32_t ctrl = 0;
	int r;
	struct display_csk6_lcd_data *lcd_data = handler;

	ctrl |= CSK_SPI_MODE_MASTER;
	ctrl |= CSK_SPI_CPOL0_CPHA0;
	ctrl |= CSK_SPI_MSB_LSB;
	ctrl |= CSK_SPI_TXIO_DMA;
	ctrl |= CSK_SPI_DATA_BITS(8);

	k_mutex_lock(&lcd_data->lock, K_FOREVER);
	r = SPI_Control(lcd_data->spi, ctrl, 50 * 1000 * 1000);
	if (r != 0) {
		LOG_ERR("SPI_Control failed, error code:%d", r);
		k_mutex_unlock(&lcd_data->lock);
		return r;
	}
	k_sem_reset(&lcd_data->spi_done_sem);
	lcd_data->cmd = 0;
	r = SPI_Send(lcd_data->spi, datas, len);
	if (r != 0) {
		LOG_ERR("SPI_Send failed, error code:%d", r);
		k_mutex_unlock(&lcd_data->lock);
		return r;
	}
	k_sem_take(&lcd_data->spi_done_sem, K_FOREVER);
	k_mutex_unlock(&lcd_data->lock);

	return 0;
}

int32_t LCD_SPI_FAST_CODE display_csk6_lcd_display_datas_send(void *handler, const uint8_t *buff,
							      uint32_t h, uint32_t w,
							      uint8_t rotate)
{
	int r;

	if (handler == NULL) {
		return -EINVAL;
	}
	struct display_csk6_lcd_data *lcd_data = handler;
	struct display_csk6_lcd_context *ctx = &lcd_data->ctx;

	r = display_csk6_lcd_context_init(ctx, buff, h, w, rotate);
	if (r) {
		return r;
	}

	SCB_CleanDCache_by_Addr((void *)ctx->buffer, ctx->w * ctx->h * 2);
	ctx->buffer += ctx->start_offset;

	k_mutex_lock(&lcd_data->lock, K_FOREVER);

	const uint32_t ctrl = CSK_SPI_MODE_MASTER | CSK_SPI_CPOL0_CPHA0 | CSK_SPI_MSB_LSB |
			      CSK_SPI_TXIO_DMA | CSK_SPI_DATA_BITS(16);
	const uint32_t speed = 50 * 1000 * 1000;
	r = SPI_Control(lcd_data->spi, ctrl, speed);
	if (r != 0) {
		LOG_ERR("SPI_Control failed, error code:%d, spi:%p", r, lcd_data->spi);
		k_mutex_unlock(&lcd_data->lock);
		return r;
	}

	k_sem_reset(&lcd_data->spi_done_sem);
	lcd_data->cmd = 1;

	r = display_csk6_lcd_send_datas_with_dma(lcd_data->spi, lcd_data->dma_ch, ctx->buffer,
						 ctx->line_size / 2, ctx->interval);
	if (r) {
		LOG_ERR("display_csk6_lcd_send_datas_with_dma returned r = %d", r);
		k_mutex_unlock(&lcd_data->lock);
		return r;
	}

	k_sem_take(&lcd_data->spi_done_sem, K_FOREVER);
	k_mutex_unlock(&lcd_data->lock);

	return 0;
}

void *display_csk6_lcd_init(const char *name, uint8_t dma_ch)
{
	SPI_DEV *hal_spi = NULL;
	int r;

	memset(&g_display_csk6_lcd_data, 0, sizeof(struct display_csk6_lcd_data));

	if (strcmp(name, "SPI_0") == 0) {
		hal_spi = SPI0();
	} else if (strcmp(name, "SPI_1") == 0) {
		hal_spi = SPI1();
	} else {
		LOG_ERR("Unknown SPI device %s", name);
		return NULL;
	}

	hal_spi->info->cb_event = display_csk6_lcd_spi_isr_callback;
	hal_spi->info->usr_param = (uint32_t)&g_display_csk6_lcd_data;

	r = dma_channel_reserve(dma_ch, hal_spi->dma_tx.cb_event, hal_spi->dma_tx.usr_param,
				DMA_CACHE_SYNC_NOP);
	if (r != dma_ch) {
		LOG_ERR("ch %d is not a valid", dma_ch);
		k_panic();
		return NULL;
	}

	g_display_csk6_lcd_data.spi = hal_spi;
	g_display_csk6_lcd_data.dma_ch = dma_ch;

	r = k_sem_init(&g_display_csk6_lcd_data.spi_done_sem, 0, 1);
	if (r) {
		LOG_ERR("sem init failed, r:%d", r);
		return NULL;
	}

	r = k_mutex_init(&g_display_csk6_lcd_data.lock);
	if (r) {
		LOG_ERR("mutex init failed, r:%d", r);
		return NULL;
	}

	return &g_display_csk6_lcd_data;
}
