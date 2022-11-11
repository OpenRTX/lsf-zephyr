/*
 * Copyright (c) 2021 listenai Systems (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Driver for UART on LINSTENAI csk family processor.
 *
 * For full serial function, use the USART controller.
 *
 */

#define DT_DRV_COMPAT listenai_csk_uart

#include "hal/Driver_UART.h"
#include "hal/uart.h"

#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/pinctrl.h>

#include <drivers/uart.h>
#include <string.h>

#define DEV_CFG(dev)  ((const struct csk_uart_config *const)(dev)->config)
#define DEV_DATA(dev) ((struct csk_uart_data *)(dev)->data)

#ifdef CONFIG_UART_ASYNC_API
#define CSK_UART_ASYNC_FIFO_TRIGGER_SIZE (8)
#endif

typedef struct csk_uart_dma_xfer_info {
	uint8_t *buf;
	uint32_t len;
	uint32_t offset;
	uint32_t expe_len;
	uint32_t received_len;
	uint32_t dma_last_recv_len;
} csk_uart_dma_xfer_info_t;

typedef struct csk_uart_dma_config {
	struct k_work_delayable dwork;
	struct dma_config config;
	csk_uart_dma_xfer_info_t xfer_info;
	const struct device *dma_dev;
	uint8_t channel;
} csk_uart_dma_config_t;

typedef struct csk_uart_async_data {
	csk_uart_dma_config_t dma_tx;
	csk_uart_dma_config_t dma_rx;
	uart_callback_t async_cb;
	void *async_user_data;
	const struct device *dev;
	uint8_t *rx_next_buf;
	uint32_t rx_next_len;
	uint32_t rx_next_recv_len;
	uint32_t rx_timeout;
} csk_uart_async_data_t;

struct csk_uart_data {
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t callback;
	void *cb_data;
#endif

#ifdef CONFIG_UART_ASYNC_API
	csk_uart_async_data_t async_data;
#endif
#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
	struct uart_config config;
#endif
};

struct csk_uart_config {
	volatile uint32_t reg_addr;
	volatile uint32_t baud_rate;
	const struct pinctrl_dev_config *pcfg;

#if defined(CONFIG_UART_INTERRUPT_DRIVEN) || defined(CONFIG_UART_ASYNC_API)
	void (*irq_config_func)(const struct device *dev);
#endif
};

void *get_uart_ctx(DW_UART_RegDef *reg_base)
{
	if (reg_base == ((UART_RESOURCES *)UART0())->reg) {
		return UART0();
	} else if (reg_base == ((UART_RESOURCES *)UART1())->reg) {
		return UART1();
	} else if (reg_base == ((UART_RESOURCES *)UART2())->reg) {
		return UART2();
	}
	return NULL;
}

static int csk_uart_poll_in(const struct device *dev, unsigned char *p_char)
{
	int ret = -1;
	volatile uint32_t lsr;
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	if ((uart_reg->REG_LSR.all & UARTC_LSR_RDR)) {
		/* Receive a character */
		*p_char = (uint8_t)(uart_reg->REG_RBR.all & 0xFF);
		ret = 0;
	}

	if (uart_reg->REG_LSR.all & UARTC_LSR_OE) {
		lsr = uart_reg->REG_LSR.all;
	}
	// wait not busy
	while (!(uart_reg->REG_LSR.all & UARTC_LSR_TEMT))
		;
	return ret;
}

static void csk_uart_poll_out(const struct device *dev, unsigned char out_char)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	if (uart_reg->REG_LSR.all & UARTC_LSR_THRE) {
		uart_reg->REG_THR.all = out_char;
	}
	// wait not busy
	while (!(uart_reg->REG_LSR.all & UARTC_LSR_TEMT))
		;
}

#ifdef CONFIG_UART_ASYNC_API

static inline void csk_uart_async_notify_user(csk_uart_async_data_t *async_data, struct uart_event *evt)
{
	if (async_data->async_cb) {
		async_data->async_cb(async_data->dev, evt, async_data->async_user_data);
	}
}

static inline void csk_uart_async_evt_tx_done(csk_uart_async_data_t *async_data)
{
	struct uart_event evt = {
		.type = UART_TX_DONE,
		.data.tx.buf = async_data->dma_tx.xfer_info.buf,
		.data.tx.len = async_data->dma_tx.xfer_info.len,
	};

	csk_uart_async_notify_user(async_data, &evt);
}

static inline void csk_uart_async_evt_tx_aborted(csk_uart_async_data_t *async_data)
{
	struct uart_event evt = {
		.type = UART_TX_ABORTED,
		.data.tx.buf = async_data->dma_tx.xfer_info.buf,
		.data.tx.len = async_data->dma_tx.xfer_info.len,
	};

	csk_uart_async_notify_user(async_data, &evt);
}

static inline void csk_uart_async_evt_rx_rdy(csk_uart_async_data_t *async_data,
					     csk_uart_dma_xfer_info_t *xfer_info)
{
	struct uart_event evt = {
		.type = UART_RX_RDY,
		.data.rx.buf = xfer_info->buf,
		.data.rx.len = xfer_info->len,
		.data.rx.offset = xfer_info->offset,
	};

	csk_uart_async_notify_user(async_data, &evt);
}

static inline void csk_uart_async_evt_rx_buf_request(csk_uart_async_data_t *async_data)
{
	struct uart_event evt = {
		.type = UART_RX_BUF_REQUEST,
	};

	csk_uart_async_notify_user(async_data, &evt);
}

static inline void csk_uart_async_evt_rx_buf_released(csk_uart_async_data_t *async_data, csk_uart_dma_xfer_info_t *xfer_info)
{
	struct uart_event evt = {
		.type = UART_RX_BUF_RELEASED,
		.data.rx.buf = xfer_info->buf,
	};

	csk_uart_async_notify_user(async_data, &evt);
}

static inline void csk_uart_async_evt_rx_disabled(csk_uart_async_data_t *async_data)
{
	struct uart_event evt = {
		.type = UART_RX_DISABLED,
	};

	csk_uart_async_notify_user(async_data, &evt);
}

static inline void csk_uart_async_rx_stopped(csk_uart_async_data_t *async_data, int err)
{
	struct uart_event evt = {
		.type = UART_RX_STOPPED,
		.data.rx_stop.reason = err,
	};

	csk_uart_async_notify_user(async_data, &evt);
}

static inline void csk_uart_async_update_rx_datas(csk_uart_async_data_t *async_data,
					   DW_UART_RegDef *uart_reg)
{
	uint32_t dma_recv_size = dma_channel_get_count(async_data->dma_rx.channel);

	uint32_t dma_recv_new_size = dma_recv_size - async_data->dma_rx.xfer_info.dma_last_recv_len;
	uint8_t rx_fifo_left_count = 0;
	uint8_t *rx_buf =
		async_data->dma_rx.xfer_info.buf + async_data->dma_rx.xfer_info.received_len;

	rx_buf += dma_recv_size;
	async_data->dma_rx.xfer_info.dma_last_recv_len = dma_recv_size;

	async_data->dma_rx.xfer_info.received_len += dma_recv_new_size;

	int32_t left_len =
		async_data->dma_rx.xfer_info.expe_len - async_data->dma_rx.xfer_info.received_len;

	uint8_t *next_rx_buf = async_data->rx_next_buf;
	uint32_t next_rx_expe_len = async_data->rx_next_len;
	uint8_t next_rx_recv_len = 0;

	/* Read all datas from rx fifo to clear rx timeout interrupt */
	while ((uart_reg->REG_LSR.all & UARTC_LSR_RDR) &&
	       (rx_fifo_left_count < (CSK_UART_ASYNC_FIFO_TRIGGER_SIZE - 1))) {
		char ch = (uint8_t)(uart_reg->REG_RBR.all & 0xFF);
		if (rx_fifo_left_count < left_len) {
			/* copy data to current buffer */
			*rx_buf++ = ch;
		} else {
			/* is next rx buffer valid? */
			if (next_rx_buf) {
				if (next_rx_recv_len < next_rx_expe_len) {
					/* copy data to next buffer */
					*next_rx_buf++ = ch;
					next_rx_recv_len++;
				} else {
					/* next buffer receive complete, drop it */
				}
			} else {
				/* no buffer to rx data, drop it */
			}
		}
		rx_fifo_left_count++;
	}

	async_data->rx_next_recv_len = next_rx_recv_len;

	async_data->dma_rx.xfer_info.len =
		((rx_fifo_left_count < left_len) ? rx_fifo_left_count : left_len) + dma_recv_new_size;

	async_data->dma_rx.xfer_info.offset = async_data->dma_rx.xfer_info.received_len - dma_recv_new_size;

	async_data->dma_rx.xfer_info.received_len += ((rx_fifo_left_count < left_len) ? rx_fifo_left_count : left_len);
}

static void csk_uart_dma_tx_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
	const struct device *uart_dev = (const struct device *)user_data;
	csk_uart_async_data_t *async_data = &DEV_DATA(uart_dev)->async_data;

	k_work_cancel_delayable(&async_data->dma_tx.dwork);
	if (status == 0) {
		async_data->dma_tx.xfer_info.offset = 0;
		async_data->dma_tx.xfer_info.len = async_data->dma_tx.xfer_info.expe_len;
		csk_uart_async_evt_tx_done(async_data);
	} else {
		
	}
}

static void csk_uart_dma_rx_complete(csk_uart_async_data_t *async_data)
{
	csk_uart_async_evt_rx_rdy(async_data, &async_data->dma_rx.xfer_info);
	csk_uart_async_evt_rx_buf_released(async_data, &async_data->dma_rx.xfer_info);

	if ((async_data->rx_next_buf != NULL) && (async_data->rx_next_len > 0)) {
		const struct device *uart_dev = (const struct device *)async_data->dev;
		const struct csk_uart_config *const cfg = uart_dev->config;
		DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)cfg->reg_addr;

		if (async_data->rx_next_recv_len) {
			struct uart_event evt = {
				.type = UART_RX_RDY,
				.data.rx.buf = async_data->rx_next_buf,
				.data.rx.len = async_data->rx_next_recv_len,
				.data.rx.offset = 0,
			};
			/* callback next buffer received datas */
			csk_uart_async_notify_user(async_data, &evt);
		}

		if (async_data->rx_next_recv_len == async_data->rx_next_len) {
			/* next buffer rx complete */
			struct uart_event evt = {0};

			evt.type = UART_RX_BUF_RELEASED;
			evt.data.rx.buf = async_data->rx_next_buf;
			csk_uart_async_notify_user(async_data, &evt);
		} else {
			uint8_t *rx_buf = async_data->rx_next_buf + async_data->rx_next_recv_len;
			uint32_t rx_len = async_data->rx_next_len - async_data->rx_next_recv_len;

			/* enable rx timeout */
			uart_reg->REG_IER.bit.ECHT = 0x1;

			dma_reload(async_data->dma_rx.dma_dev, async_data->dma_rx.channel,
				   (uint32_t)&uart_reg->REG_RBR.all, (uint32_t)rx_buf,
				   rx_len >= CSK_UART_ASYNC_FIFO_TRIGGER_SIZE
					   ? rx_len - (CSK_UART_ASYNC_FIFO_TRIGGER_SIZE - 1)
					   : 1);
			dma_start(async_data->dma_rx.dma_dev, async_data->dma_rx.channel);

			async_data->dma_rx.xfer_info.buf = async_data->rx_next_buf;
			async_data->dma_rx.xfer_info.expe_len = async_data->rx_next_len;
			async_data->dma_rx.xfer_info.len = 0;
			async_data->dma_rx.xfer_info.offset = async_data->rx_next_recv_len;
			async_data->dma_rx.xfer_info.received_len = async_data->rx_next_recv_len;
			async_data->dma_rx.xfer_info.dma_last_recv_len = 0;
		}

		async_data->rx_next_buf = NULL;
		async_data->rx_next_len = 0;
		async_data->rx_next_recv_len = 0;

		/* request next buffer again */
		csk_uart_async_evt_rx_buf_request(async_data);
	}
	else {
		csk_uart_async_evt_rx_disabled(async_data);
	}
}

static void csk_uart_dma_rx_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
	const struct device *uart_dev = (const struct device *)user_data;
	const struct csk_uart_config *const cfg = uart_dev->config;
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)cfg->reg_addr;
	csk_uart_async_data_t *async_data = &DEV_DATA(uart_dev)->async_data;

	k_work_cancel_delayable(&async_data->dma_rx.dwork);

	/* Disable rx timeout */
	uart_reg->REG_IER.bit.ECHT = 0x0;

	if (status == 0) {
		dma_stop(async_data->dma_rx.dma_dev, async_data->dma_rx.channel);
		csk_uart_async_update_rx_datas(async_data, uart_reg);
		csk_uart_dma_rx_complete(async_data);
	} else {
	}
}

static int csk_uart_callback_set(const struct device *dev, uart_callback_t callback, void *user_data)
{
	struct csk_uart_data *data = DEV_DATA(dev);
	csk_uart_async_data_t *async_data = &data->async_data;

	async_data->async_cb = callback;
	async_data->async_user_data = user_data;

	return 0;
}

int csk_uart_async_tx(const struct device *dev, const uint8_t *buf, size_t len, int32_t timeout)
{
	int err;
	struct csk_uart_data *data = DEV_DATA(dev);
	csk_uart_async_data_t *async_data = &data->async_data;
	const struct csk_uart_config *config = DEV_CFG(dev);
	struct dma_block_config block_config = {0};
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	if (!device_is_ready(async_data->dma_tx.dma_dev)) {
		return -ENOTSUP;
	}

	k_work_cancel_delayable(&async_data->dma_tx.dwork);

	block_config.source_address = (uint32_t)buf;
	block_config.dest_address = (uint32_t)&uart_reg->REG_THR.all;
	block_config.block_size = len;
	block_config.dest_addr_adj = DMA_ADDR_ADJ_NO_CHANGE;
	block_config.source_addr_adj = DMA_ADDR_ADJ_INCREMENT;
	block_config.fifo_mode_control = 1;

	/* other default config has been configed, see macro @CSK_UART_DMA_CONFIG_GET */
	async_data->dma_tx.config.head_block = &block_config;

	async_data->dma_tx.xfer_info.buf = (uint8_t *) buf;
	async_data->dma_tx.xfer_info.expe_len = len;
	async_data->dma_tx.xfer_info.len = 0;
	async_data->dma_tx.xfer_info.offset = 0;
	async_data->dma_tx.xfer_info.received_len = 0;
	async_data->dma_tx.xfer_info.dma_last_recv_len = 0;

	err = dma_config(async_data->dma_tx.dma_dev, async_data->dma_tx.channel, &async_data->dma_tx.config);
	if (err != 0) {
		return err;
	}

	if ((timeout != SYS_FOREVER_US) && (timeout != 0)) {
		/* Note that the return value 1 to indicate successfully scheduled. */
		err = k_work_reschedule(&async_data->dma_tx.dwork, K_USEC(timeout));
		if (!err) {
			return -EINVAL;
		}
	}

	err = dma_start(async_data->dma_tx.dma_dev, async_data->dma_tx.channel);
	if (err != 0) {
		return err;
	}

	return 0;
}

int csk_uart_async_rx_enable(const struct device *dev, uint8_t *buf, size_t len, int32_t timeout)
{
	int err;
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;
	struct csk_uart_data *data = DEV_DATA(dev);
	csk_uart_async_data_t *async_data = &data->async_data;
	struct dma_block_config block_config = {0};

	if (!device_is_ready(async_data->dma_rx.dma_dev)) {
		return -ENOTSUP;
	}

	k_work_cancel_delayable(&async_data->dma_rx.dwork);

	async_data->rx_timeout = timeout;

	block_config.source_address = (uint32_t)&uart_reg->REG_RBR.all;
	block_config.dest_address = (uint32_t)buf;
	block_config.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT;
	block_config.source_addr_adj = DMA_ADDR_ADJ_NO_CHANGE;
	block_config.fifo_mode_control = 1;
	block_config.block_size = len >= CSK_UART_ASYNC_FIFO_TRIGGER_SIZE
					  ? len - (CSK_UART_ASYNC_FIFO_TRIGGER_SIZE - 1)
					  : 1;

	/* other default config has been configed, see macro @__CSK_UART_DMA_CONFIG_GET */
	async_data->dma_rx.config.head_block = &block_config;

	memset(&async_data->dma_rx.xfer_info, 0, sizeof(csk_uart_dma_xfer_info_t));

	async_data->dma_rx.xfer_info.buf = buf;
	async_data->dma_rx.xfer_info.expe_len = len;

	uart_reg->REG_IER.bit.ECHT = 0x0;

	/* clear rx fifo */
	uint8_t rx_fifo_left_count = 0;
	char ch;
	while ((uart_reg->REG_LSR.all & UARTC_LSR_RDR)) {
		ch = (uint8_t)(uart_reg->REG_RBR.all & 0xFF);
		rx_fifo_left_count++;
	}
	ARG_UNUSED(ch);

	/* enable rx timeout interrupt */
	uart_reg->REG_IER.bit.ECHT = 0x1;

	/* reset rx fifo */
	uart_reg->REG_FCR.bit.RFIFOR = 0x1;

	/** 
	 * It seems that RCVR has been cleared after set RFIFOR.
	 * SO set it again at here.
	 * Set rx fifo to 1/4 full.
	 * set dma mode.
	 */
	uart_reg->REG_FCR.bit.DMAM = 0x1;
	uart_reg->REG_FCR.bit.RCVR = 0x01;

	/* let the register configuration take effect */
	__asm__ __volatile__("" : : : "memory");

	err = dma_config(async_data->dma_rx.dma_dev, async_data->dma_rx.channel,
			 &async_data->dma_rx.config);
	if (err != 0) {
		return err;
	}

	err = dma_start(async_data->dma_rx.dma_dev, async_data->dma_rx.channel);
	if (err != 0) {
		return err;
	}

	csk_uart_async_evt_rx_buf_request(async_data);
	
	return 0;
}

int csk_uart_async_tx_abort(const struct device *dev)
{
	struct csk_uart_data *data = DEV_DATA(dev);
	csk_uart_async_data_t *async_data = &data->async_data;

	k_work_cancel_delayable(&async_data->dma_tx.dwork);
	dma_stop(async_data->dma_tx.dma_dev, async_data->dma_tx.channel);

	uint32_t c = dma_channel_get_count(async_data->dma_tx.channel);
	async_data->dma_tx.xfer_info.len = c;

	csk_uart_async_evt_tx_aborted(async_data);

	return 0;
}

int csk_uart_async_rx_buf_rsp(const struct device *dev, uint8_t *buf, size_t len)
{
	struct csk_uart_data *data = DEV_DATA(dev);
	csk_uart_async_data_t *async_data = &data->async_data;

	int k = arch_irq_lock();

	async_data->rx_next_buf = buf;
	async_data->rx_next_len = len;

	arch_irq_unlock(k);

	return 0;
}

int csk_uart_async_rx_disable(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;
	struct csk_uart_data *data = DEV_DATA(dev);
	csk_uart_async_data_t *async_data = &data->async_data;

	/* Disable rx timeout */
	uart_reg->REG_IER.bit.ECHT = 0x0;

	uint32_t received_len = async_data->dma_rx.xfer_info.received_len;
	dma_stop(async_data->dma_rx.dma_dev, async_data->dma_rx.channel);
	csk_uart_async_update_rx_datas(async_data, uart_reg);

	if (async_data->dma_rx.xfer_info.received_len != received_len) {
		csk_uart_async_evt_rx_rdy(async_data, &async_data->dma_rx.xfer_info);
	}

	csk_uart_async_evt_rx_buf_released(async_data, &async_data->dma_rx.xfer_info);
	csk_uart_async_evt_rx_disabled(async_data);

	return 0;
}

void csk_uart_async_rx_dwork_callback(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	csk_uart_dma_config_t *dma_rx = CONTAINER_OF(dwork, csk_uart_dma_config_t, dwork);
	csk_uart_async_data_t *async_data = CONTAINER_OF(dma_rx, csk_uart_async_data_t, dma_rx);

	if (async_data->dma_rx.xfer_info.received_len == async_data->dma_rx.xfer_info.expe_len) {
		dma_stop(async_data->dma_rx.dma_dev, async_data->dma_rx.channel);
		const struct csk_uart_config *config = DEV_CFG(async_data->dev);
		DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;
		/* Disable rx timeout */
		uart_reg->REG_IER.bit.ECHT = 0x0;
		csk_uart_dma_rx_complete(async_data);
	} else {
#if !defined(CONFIG_SOC_LS_ADSP_CSK6) && defined(CONFIG_CSK6_DCACHE)
		SCB_InvalidateDCache_by_Addr(dma_rx->xfer_info.buf,
					     dma_rx->xfer_info.expe_len);
#endif
		csk_uart_async_evt_rx_rdy(async_data, &dma_rx->xfer_info);
	}
}

void csk_uart_async_tx_dwork_callback(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	csk_uart_dma_config_t *dma_tx = CONTAINER_OF(dwork, csk_uart_dma_config_t, dwork);
	csk_uart_async_data_t *async_data = CONTAINER_OF(dma_tx, csk_uart_async_data_t, dma_tx);

	csk_uart_async_tx_abort(async_data->dev);
}

#endif

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static int csk_uart_fifo_fill(const struct device *dev, const uint8_t *tx_data, int size)
{
	unsigned int num_tx = 0U;
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	while (((size - num_tx) > 0)) {
		/* Send a character */
		if (uart_reg->REG_LSR.all & UARTC_LSR_THRE) {
			uart_reg->REG_THR.all = tx_data[num_tx];
		}

		// wait not busy
		while (!(uart_reg->REG_LSR.all & UARTC_LSR_TEMT)) {
			;
		}
		num_tx++;
	}

	return (int)num_tx;
}

static int csk_uart_fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
	unsigned int num_rx = 0U;
	volatile uint32_t lsr;
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	while (((size - num_rx) > 0) && (uart_reg->REG_LSR.all & UARTC_LSR_RDR)) {
		/* Receive a character */
		rx_data[num_rx++] = (uint8_t)(uart_reg->REG_RBR.all & 0xFF);

		// wait not busy
		while (!(uart_reg->REG_LSR.all & UARTC_LSR_TEMT))
			;
	}

	if (uart_reg->REG_LSR.all & UARTC_LSR_OE) {
		lsr = uart_reg->REG_LSR.all;
	}

	if (uart_reg->REG_LSR.all & UARTC_LSR_PE) {
		lsr = uart_reg->REG_LSR.all;
	}
	return num_rx;
}

static void csk_uart_irq_tx_enable(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	uart_reg->REG_IER.all |= UARTC_IER_THRE;
}

static void csk_uart_irq_tx_disable(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	uart_reg->REG_IER.all &= ~UARTC_IER_THRE;
}

static int csk_uart_irq_tx_ready(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	// Get interrupt status
	uint32_t iir = uart_reg->REG_IIR.all & UARTC_IIR_INT_MASK;

	if ((iir == UARTC_IIR_NONE) || (iir == UARTC_IIR_THRE)) {
		return (uart_reg->REG_LSR.all & UARTC_LSR_THRE) ? true : false;
	} else {
		return false;
	}
}

static void csk_uart_irq_rx_enable(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	uart_reg->REG_IER.all |= UARTC_IER_RDR;
}

static void csk_uart_irq_rx_disable(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	uart_reg->REG_IER.all &= ~UARTC_IER_RDR;
}

static int csk_uart_irq_rx_ready(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	// Get interrupt status
	uint32_t iir = uart_reg->REG_IIR.all & UARTC_IIR_INT_MASK;

	if ((iir == UARTC_IIR_NONE) || (iir == UARTC_IIR_RDA) || (iir == UARTC_IIR_RTO) || (iir == UARTC_IIR_RLS)) {
		// Get rx data status
		return (uart_reg->REG_LSR.all & UARTC_LSR_RDR) ? true : false;
	} else {
		return false;
	}
}

static void csk_uart_irq_err_enable(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	uart_reg->REG_IER.all |= UARTC_IER_RLS;
}

static void csk_uart_irq_err_disable(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	uart_reg->REG_IER.all &= ~UARTC_IER_RLS;
}

static int csk_uart_irq_is_pending(const struct device *dev)
{
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;

	// Get interrupt status
	uint32_t iir = uart_reg->REG_IIR.all;

	return ((iir & UARTC_IIR_INT_MASK) == UARTC_IIR_RDA) ||
			       ((iir & UARTC_IIR_INT_MASK) == UARTC_IIR_THRE) ?
		       true :
			     false;
}

static int csk_uart_irq_update(const struct device *dev)
{
	return 1;
}

static void csk_uart_irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb,
				      void *cb_data)
{
	struct csk_uart_data *data = dev->data;

	data->callback = cb;
	data->cb_data = cb_data;
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

#if defined(CONFIG_UART_INTERRUPT_DRIVEN) || defined(CONFIG_UART_ASYNC_API)
static void csk_uart_isr(const struct device *dev)
{
	struct csk_uart_data *data = dev->data;
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;
#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
	if (uart_reg == NULL) {
		return;
	}
	if (data->callback) {
		data->callback(dev, data->cb_data);
	}
#endif

#if defined(CONFIG_UART_ASYNC_API)

	uint8_t iid = uart_reg->REG_IIR.all;

	if (iid & UARTC_IIR_RTO) {
		/* rx timeout */
		csk_uart_async_data_t *async_data = &data->async_data;

		dma_stop(async_data->dma_rx.dma_dev, async_data->dma_rx.channel);
		csk_uart_async_update_rx_datas(async_data, uart_reg);

		int32_t left_len = async_data->dma_rx.xfer_info.expe_len -
				   async_data->dma_rx.xfer_info.received_len;

		if (left_len > 0) {
			async_data->dma_rx.xfer_info.dma_last_recv_len = 0;
			uint8_t *rx_buf = async_data->dma_rx.xfer_info.buf +
					  async_data->dma_rx.xfer_info.received_len;
			dma_reload(async_data->dma_rx.dma_dev, async_data->dma_rx.channel,
				   (uint32_t)&uart_reg->REG_RBR.all, (uint32_t)rx_buf,
				   left_len >= CSK_UART_ASYNC_FIFO_TRIGGER_SIZE
					   ? left_len - (CSK_UART_ASYNC_FIFO_TRIGGER_SIZE - 1)
					   : 1);

			dma_start(async_data->dma_rx.dma_dev, async_data->dma_rx.channel);
		}

		/* start dwork to notify user */
		if ((async_data->rx_timeout != SYS_FOREVER_US) && (async_data->rx_timeout != 0)) {
			/* return value 1 to indicate successfully scheduled. */
			k_work_reschedule(&async_data->dma_rx.dwork,
					  K_USEC(async_data->rx_timeout));
		}
	}
#endif
}
#endif

static int csk_uart_init(const struct device *dev)
{
	unsigned int old_level;
	unsigned int baud_rate;
	const struct csk_uart_config *config = DEV_CFG(dev);
	struct csk_uart_data *data = DEV_DATA(dev);

	/* make complier happy */
	ARG_UNUSED(data);

	/* disable interrupts */
	old_level = irq_lock();

	/* Set the pin to UART alternate function. */
	pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);

	void *uart_driver_ctx = get_uart_ctx((DW_UART_RegDef *)config->reg_addr);
	baud_rate = config->baud_rate;
	UART_Initialize(uart_driver_ctx, NULL, NULL);
	UART_PowerControl(uart_driver_ctx, CSK_POWER_FULL);

	uint32_t ctrl = CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT | CSK_UART_DATA_BITS_8 | CSK_UART_PARITY_NONE |
			CSK_UART_STOP_BITS_1 | CSK_UART_FLOW_CONTROL_NONE |
			CSK_UART_GPIO_CONTROL_DEFAULT;

#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
	struct uart_config *cfg = &data->config;
	cfg->baudrate = baud_rate;
	cfg->parity = UART_CFG_PARITY_NONE;
	cfg->stop_bits = UART_CFG_STOP_BITS_1;
	cfg->data_bits = UART_CFG_DATA_BITS_8;
	cfg->flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
#endif

#ifdef CONFIG_UART_ASYNC_API
	if (data->async_data.dma_tx.dma_dev != NULL || data->async_data.dma_rx.dma_dev != NULL) {
		ctrl |= CSK_UART_Function_CONTROL_Dma;
	} else {
		ctrl |= CSK_UART_Function_CONTROL_Int;
	}
#else
	ctrl |= CSK_UART_Function_CONTROL_Int;
#endif

	UART_Control(uart_driver_ctx, ctrl, baud_rate);
	UART_Control(uart_driver_ctx, CSK_UART_CONTROL_TX, 1);
	UART_Control(uart_driver_ctx, CSK_UART_CONTROL_RX, 1);

#if defined(CONFIG_UART_INTERRUPT_DRIVEN) || defined(CONFIG_UART_ASYNC_API)
	config->irq_config_func(dev);
#endif

	/* restore interrupt state */
	irq_unlock(old_level);

#ifdef CONFIG_UART_ASYNC_API
	if (data->async_data.dma_tx.dma_dev != NULL || data->async_data.dma_rx.dma_dev != NULL) {
		DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;
		/* Set rx fifo to 1/4 full */
		uart_reg->REG_FCR.bit.RCVR = 0x01;
	}
#endif
	return 0;
}

#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
static int csk_uart_configure(const struct device *dev, const struct uart_config *cfg)
{
	struct csk_uart_data *data = DEV_DATA(dev);
	const struct csk_uart_config *config = DEV_CFG(dev);

	void *uart_driver_ctx = get_uart_ctx((DW_UART_RegDef *)config->reg_addr);
	uint32_t ctrl = CSK_UART_MODE_ASYNCHRONOUS;

	switch (cfg->parity) {
	case UART_CFG_PARITY_NONE:
		break;
	case UART_CFG_PARITY_ODD:
		ctrl |= UARTC_LCR_PARITY_ODD;
		break;
	case UART_CFG_PARITY_EVEN:
		ctrl |= UARTC_LCR_PARITY_EVEN;
		break;
	default:
		return -ENOTSUP;
	}

	switch (cfg->stop_bits) {
	case UART_CFG_STOP_BITS_1:
		ctrl |= CSK_UART_STOP_BITS_1;
		break;
	case UART_CFG_STOP_BITS_1_5:
		ctrl |= CSK_UART_STOP_BITS_1_5;
		break;
	case UART_CFG_STOP_BITS_2:
		ctrl |= CSK_UART_STOP_BITS_2;
		break;
	default:
		return -ENOTSUP;
	}

	switch (cfg->data_bits) {
	case UART_CFG_DATA_BITS_5:
		ctrl |= CSK_UART_DATA_BITS_5;
		break;
	case UART_CFG_DATA_BITS_6:
		ctrl |= CSK_UART_DATA_BITS_6;
		break;
	case UART_CFG_DATA_BITS_7:
		ctrl |= CSK_UART_DATA_BITS_7;
		break;
	case UART_CFG_DATA_BITS_8:
		ctrl |= CSK_UART_DATA_BITS_8;
		break;
	default:
		return -ENOTSUP;
	}

	switch (cfg->flow_ctrl) {
	case UART_CFG_FLOW_CTRL_NONE:
		break;
	case UART_CFG_FLOW_CTRL_RTS_CTS:
		ctrl |= CSK_UART_FLOW_CONTROL_RTS_CTS;
		break;
	default:
		return -ENOTSUP;
	}

#ifdef CONFIG_UART_ASYNC_API
	if (data->async_data.dma_tx.dma_dev != NULL || data->async_data.dma_rx.dma_dev != NULL) {
		ctrl |= CSK_UART_Function_CONTROL_Dma;
	} else {
		ctrl |= CSK_UART_Function_CONTROL_Int;
	}
#else
	ctrl |= CSK_UART_Function_CONTROL_Int;
#endif

	int k = arch_irq_lock();
	if (UART_Control(uart_driver_ctx, ctrl, cfg->baudrate) != 0) {
		arch_irq_unlock(k);
		return -EIO;
	}

	memcpy(&data->config, cfg, sizeof(struct uart_config));
	arch_irq_unlock(k);

	return 0;
}

static int csk_uart_config_get(const struct device *dev, struct uart_config *cfg)
{
	const struct csk_uart_data *data = DEV_DATA(dev);

	if (cfg != NULL) {
		memcpy(cfg, &data->config, sizeof(struct csk_uart_config));
		return 0;
	}

	return -EINVAL;
}
#endif

static const struct uart_driver_api csk_uart_driver_api = {
	.poll_in = csk_uart_poll_in,
	.poll_out = csk_uart_poll_out,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = csk_uart_fifo_fill,
	.fifo_read = csk_uart_fifo_read,
	.irq_tx_enable = csk_uart_irq_tx_enable,
	.irq_tx_disable = csk_uart_irq_tx_disable,
	.irq_tx_ready = csk_uart_irq_tx_ready,
	.irq_rx_enable = csk_uart_irq_rx_enable,
	.irq_rx_disable = csk_uart_irq_rx_disable,
	.irq_rx_ready = csk_uart_irq_rx_ready,
	.irq_err_enable = csk_uart_irq_err_enable,
	.irq_err_disable = csk_uart_irq_err_disable,
	.irq_is_pending = csk_uart_irq_is_pending,
	.irq_update = csk_uart_irq_update,
	.irq_callback_set = csk_uart_irq_callback_set,
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
#ifdef CONFIG_UART_ASYNC_API
	.callback_set = csk_uart_callback_set,
	.tx = csk_uart_async_tx,
	.tx_abort = csk_uart_async_tx_abort,
	.rx_enable = csk_uart_async_rx_enable,
	.rx_disable = csk_uart_async_rx_disable,
	.rx_buf_rsp = csk_uart_async_rx_buf_rsp,
#endif
#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
	.configure = csk_uart_configure,
	.config_get = csk_uart_config_get,
#endif /* CONFIG_UART_USE_RUNTIME_CONFIGURE */
};

#if defined(CONFIG_UART_INTERRUPT_DRIVEN) || defined(CONFIG_UART_ASYNC_API)
#define CSK_UART_IRQ_HANDLER(index)                                                                \
	static void csk_uart_irq_config_func_##index(const struct device *dev)                     \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(index), DT_INST_IRQ(index, priority), csk_uart_isr,       \
			DEVICE_DT_INST_GET(index), 0);                                         \
		irq_enable(DT_INST_IRQN(index));                                                   \
	}
#define CSK_UART_IRQ_CFG_FUNC_CONF(index) .irq_config_func = csk_uart_irq_config_func_##index
#else
#define CSK_UART_IRQ_HANDLER(index)
#define CSK_UART_IRQ_CFG_FUNC_CONF(index)
#endif

#if CONFIG_UART_ASYNC_API

#define CSK_UART_DMA_DIR_tx MEMORY_TO_PERIPHERAL
#define CSK_UART_DMA_DIR_rx PERIPHERAL_TO_MEMORY
#define CSK_UART_DMA_DIR_BY_NAME(name) CSK_UART_DMA_DIR_##name

#define CSK_UART0_DMA_SLOT_tx DMA_HSID_UART0_TX
#define CSK_UART0_DMA_SLOT_rx DMA_HSID_UART0_RX
#define CSK_UART1_DMA_SLOT_tx DMA_HSID_UART1_TX
#define CSK_UART1_DMA_SLOT_rx DMA_HSID_UART1_RX
#define CSK_UART2_DMA_SLOT_tx DMA_HSID_UART2_TX
#define CSK_UART2_DMA_SLOT_rx DMA_HSID_UART2_RX
#define CSK_UART_DMA_SLOT_BY_NAME(idx, name) CSK_UART##idx##_DMA_SLOT_##name

#define CSK_UART_DMA_BURST_SIZE_tx 1
#define CSK_UART_DMA_BURST_SIZE_rx 1

#define CSK_UART_DMA_BURST_SIZE_BY_NAME(name) CSK_UART_DMA_BURST_SIZE_##name

#define __CSK_UART_DMA_CONFIG_GET(idx, name)                                               \
	{                                                                                      \
		.dwork = Z_WORK_DELAYABLE_INITIALIZER(csk_uart_async_##name##_dwork_callback),     \
		.dma_dev = DEVICE_DT_GET(DT_DMAS_CTLR_BY_NAME(DT_DRV_INST(idx), name)),            \
		.channel = DT_INST_DMAS_CELL_BY_NAME(idx, name, channel),                          \
		.config = {                                                                        \
			.channel_direction = CSK_UART_DMA_DIR_BY_NAME(name),                       \
			.source_data_size = 1,                                                     \
			.dest_data_size = 1,                                                       \
			.source_burst_length = CSK_UART_DMA_BURST_SIZE_BY_NAME(name),              \
			.dest_burst_length = CSK_UART_DMA_BURST_SIZE_BY_NAME(name),                \
			.dma_callback = csk_uart_dma_##name##_callback,                            \
			.user_data = (void *)DEVICE_DT_GET(DT_DRV_INST(idx)),                      \
			.block_count = 1,                                                          \
			.dma_slot = CSK_UART_DMA_SLOT_BY_NAME(idx, name),                          \
		},                                                                                 \
	}

#define CSK_UART_DMA_CONFIG_DEFINE(idx, name)                                                 \
	COND_CODE_1(DT_INST_DMAS_HAS_NAME(idx, name),                                          \
		(.dma_##name = __CSK_UART_DMA_CONFIG_GET(idx, name),), ())

#define CSK_UART_ASYNC_DATA_DEFINE(idx)                                                    \
	.async_data = {                                                                        \
		.dev = DEVICE_DT_GET(DT_DRV_INST(idx)),                                            \
		CSK_UART_DMA_CONFIG_DEFINE(idx, tx)   /* no comma here */                          \
		CSK_UART_DMA_CONFIG_DEFINE(idx, rx)   /* no comma here */                          \
	}

#else
#define CSK_UART_ASYNC_DATA_DEFINE(idx)
#endif /* end of CONFIG_UART_ASYNC_API */

#define UART_CSK_INIT(index)                                                                       \
	CSK_UART_IRQ_HANDLER(index)                                                                \
	PINCTRL_DT_INST_DEFINE(index);                                                             \
	static struct csk_uart_data csk_uart_data_##index = {                                      \
		CSK_UART_ASYNC_DATA_DEFINE(index)    /* no comma here */                               \
	};                                                                                         \
	static const struct csk_uart_config csk_uart_cfg_##index = {                               \
		.reg_addr = DT_INST_REG_ADDR(index),                                                   \
		.baud_rate = DT_INST_PROP(index, current_speed),                                       \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(index),				                           \
		CSK_UART_IRQ_CFG_FUNC_CONF(index)                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(index, &csk_uart_init, NULL, &csk_uart_data_##index,                 \
			      &csk_uart_cfg_##index, PRE_KERNEL_1,                                         \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &csk_uart_driver_api);

DT_INST_FOREACH_STATUS_OKAY(UART_CSK_INIT)
