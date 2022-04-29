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

#include <soc_dt.h>
#include <drivers/uart.h>
#include <drivers/pinmux.h>
#include "hal/Driver_UART.h"
#include "hal/uart.h"

#define DEV_CFG(dev) ((const struct csk_uart_config *const)(dev)->config)
#define DEV_DATA(dev) ((struct csk_uart_data *)(dev)->data)

struct csk_uart_data {
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t callback;
	void *cb_data;
#endif
};

struct csk_uart_config {
	volatile uint32_t reg_addr;
	volatile uint32_t baud_rate;
    /* UART alternate configuration */
    const struct csk6_pinctrl_cfg *pinctrl_list;
    uint8_t pinctrl_list_size;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
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

static void csk_uart_isr(const struct device *dev)
{
	struct csk_uart_data *data = dev->data;
	const struct csk_uart_config *config = DEV_CFG(dev);
	DW_UART_RegDef *uart_reg = (DW_UART_RegDef *)config->reg_addr;
	if (uart_reg == NULL) {
		return;
	}
	if (data->callback) {
		data->callback(dev, data->cb_data);
	}
}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int csk_uart_init(const struct device *dev)
{
	unsigned int old_level;
	unsigned int baud_rate;
	/* disable interrupts */
	old_level = irq_lock();
	const struct csk_uart_config *config = DEV_CFG(dev);
	void *uart_driver_ctx = get_uart_ctx((DW_UART_RegDef *)config->reg_addr);
	baud_rate = config->baud_rate;
	UART_Initialize(uart_driver_ctx, NULL, NULL);
	UART_PowerControl(uart_driver_ctx, CSK_POWER_FULL);
	UART_Control(uart_driver_ctx,
		     CSK_UART_Function_CONTROL_Int | CSK_UART_MODE_ASYNCHRONOUS |
			     CSK_UART_DATA_BITS_8 | CSK_UART_PARITY_NONE | CSK_UART_STOP_BITS_1 |
			     CSK_UART_FLOW_CONTROL_NONE | CSK_UART_GPIO_CONTROL_DEFAULT,
		     baud_rate);
	UART_Control(uart_driver_ctx, CSK_UART_CONTROL_TX, 1);
	UART_Control(uart_driver_ctx, CSK_UART_CONTROL_RX, 1);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	config->irq_config_func(dev);
#endif

	/* restore interrupt state */
	irq_unlock(old_level);

	/* Configure RX and TX pin via the pinmux driver */
	PINCTRL_CONFIG_PINS(config->pinctrl_list, config->pinctrl_list_size);

	return 0;
}

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
};

#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
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

#define UART_CSK_INIT(index)                                                                       \
	CSK_UART_IRQ_HANDLER(index)                                                                \
	static struct csk_uart_data csk_uart_data_##index = {};                                    \
                                                                                                   \
																				\
	static const struct csk6_pinctrl_cfg                                        \
        uart_alts_##index[DT_INST_NUM_PINCTRLS_BY_IDX(index, 0)] =              \
            CSK6_DT_ALT_ITEMS_LIST(index);                                      \
																				\
	static const struct csk_uart_config csk_uart_cfg_##index = {                               \
		.reg_addr = DT_INST_REG_ADDR(index),                                               \
		.baud_rate = DT_INST_PROP(index, current_speed),                                   \
		.pinctrl_list = uart_alts_##index,                                       		   \
        .pinctrl_list_size = ARRAY_SIZE(uart_alts_##index),                                \
		CSK_UART_IRQ_CFG_FUNC_CONF(index)                                                  \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(index, &csk_uart_init, NULL, &csk_uart_data_##index,                 \
			      &csk_uart_cfg_##index, PRE_KERNEL_1,                                 \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &csk_uart_driver_api);

DT_INST_FOREACH_STATUS_OKAY(UART_CSK_INIT)
