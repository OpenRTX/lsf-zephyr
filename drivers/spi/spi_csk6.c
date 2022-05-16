/*
 * SPDX-License-Identifier: Apache-2.0
 */
#ifdef CONFIG_SOC_CSK6
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif

#define DT_DRV_COMPAT listenai_csk_spi

#include <assert.h>
#include <kernel.h>
#include <errno.h>
#include <drivers/spi.h>
#include <drivers/clock_control.h>
#include <drivers/pinmux.h>
#include <soc_dt.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(csk6_spi, CONFIG_SPI_LOG_LEVEL);

#include "spi_context.h"

#include "hal/spi.h"
#include "hal/Driver_SPI.h"
#include "hal/IOMuxManager.h"

#define SPI0_LABEL "SPI_0"
#define SPI1_LABEL "SPI_1"

#define DEV_CFG(dev) ((const struct csk_spi_config *const)(dev)->config)
#define DEV_DATA(dev) ((struct csk_spi_data *const)(dev)->data)
#define DEV_LABEL(dev) ((char *)(DEV_CFG(dev))->label)

typedef void *spi_driver_ctx_t;

struct csk_spi_data {
	struct spi_context ctx;
	size_t current_transfer_len;
	spi_driver_ctx_t driver_context;
};

struct csk_spi_config {
	char *label;
	void (*irq_config_func)(const struct device *dev);
	/* SPI alternate configuration */
    const struct csk6_pinctrl_cfg *pinctrl_list;
    uint8_t pinctrl_list_size;
};

static void *spi_csk_get_ctx(const struct device *dev)
{
	const char *label = DEV_LABEL(dev);

#ifdef CONFIG_SOC_CSK6
	if(!strcmp(label, SPI0_LABEL)){
		return SPI0();
	}
#endif
	if (!strcmp(label, SPI1_LABEL)) {
		return SPI1();
	}

	LOG_ERR("ERR SPI LABEL:%s \n", label);

	return NULL;
}


static int csk_spi_configure(const struct device *dev, const struct spi_config *spi_cfg)
{
	struct csk_spi_data *data = dev->data;

	if (spi_context_configured(&data->ctx, spi_cfg)) {
		return 0;
	}

	uint32_t spi_op_mode = (SPI_OP_MODE_GET(spi_cfg->operation) == SPI_OP_MODE_MASTER)
							? CSK_SPI_MODE_MASTER : CSK_SPI_MODE_SLAVE;

	uint32_t spi_sample_mode = 0;
	bool spi_cpol_mode = SPI_MODE_GET(spi_cfg->operation) & SPI_MODE_CPOL;
	bool spi_cpha_mode = SPI_MODE_GET(spi_cfg->operation) & SPI_MODE_CPHA;
	if (spi_cpol_mode) {  /* cpol = active low */
		if (spi_cpha_mode) {  /* cpha = second edge */
			spi_sample_mode = CSK_SPI_CPOL1_CPHA1;
		} else {  /* cpha = first edge */
			spi_sample_mode = CSK_SPI_CPOL1_CPHA0;
		}
	} else {  /* cpol = active high */
		if (spi_cpha_mode) {  /* cpha = second edge */
			spi_sample_mode = CSK_SPI_CPOL0_CPHA1;
		} else {  /* cpha = first edge */
			spi_sample_mode = CSK_SPI_CPOL0_CPHA0;
		}
	}
	uint32_t spi_word_size = SPI_WORD_SIZE_GET(spi_cfg->operation);
	uint32_t spi_word_size_mask = CSK_SPI_DATA_BITS(spi_word_size);
	uint32_t spi_bits_order = (spi_cfg->operation & SPI_TRANSFER_LSB) ?
								CSK_SPI_LSB_MSB : CSK_SPI_MSB_LSB;

	uint32_t spi_dma_en_mask = 0;

#if	!CONFIG_SPI_TX_DMA
	spi_dma_en_mask |= CSK_SPI_TXIO_PIO;
#else
	spi_dma_en_mask |= CSK_SPI_TXIO_DMA;
#endif

#if	!CONFIG_SPI_RX_DMA
	spi_dma_en_mask |= CSK_SPI_RXIO_PIO;
#else
	spi_dma_en_mask |= CSK_SPI_RXIO_DMA;
#endif

	SPI_Control(data->driver_context, spi_op_mode | spi_sample_mode | spi_word_size_mask |
                spi_bits_order | spi_dma_en_mask, spi_cfg->frequency);

	data->ctx.config = spi_cfg;
    spi_context_cs_configure(&data->ctx);
    return 0;
}

static void csk_spi_hal_transceive(const struct device *dev)
{
	struct csk_spi_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	size_t chunk_len = spi_context_max_continuous_chunk(ctx);

	if (chunk_len == 0) {
        spi_context_cs_control(&data->ctx, false);
		spi_context_complete(ctx, 0);
		return;
	}

	/* Resize the transaction item num since spi driver limitation */
    uint32_t real_trans_size;
    if (ctx->tx_buf == NULL){
        real_trans_size = chunk_len;
    } else {
         real_trans_size = (chunk_len > MAX_TRANCNT) ? MAX_TRANCNT : chunk_len;
    }
    uint32_t key = irq_lock();
    data->current_transfer_len = real_trans_size;
	if (ctx->tx_buf != NULL && ctx->rx_buf != NULL) {
		SPI_Transfer(data->driver_context, ctx->tx_buf, ctx->rx_buf, real_trans_size);
	} else if (ctx->tx_buf != NULL) {
		SPI_Send(data->driver_context, ctx->tx_buf, real_trans_size);
	} else if (ctx->rx_buf != NULL) {
		SPI_Receive(data->driver_context, ctx->rx_buf, real_trans_size);
	} else {
        /* should not go into here */
	}
	irq_unlock(key);
}

static int transceive(const struct device *dev,
		      const struct spi_config *spi_cfg,
		      const struct spi_buf_set *tx_bufs,
		      const struct spi_buf_set *rx_bufs,
		      bool asynchronous,
		      struct k_poll_signal *signal)
{
	struct csk_spi_data *data = dev->data;

	spi_context_lock(&data->ctx, asynchronous, signal, spi_cfg);
	int ret = csk_spi_configure(dev, spi_cfg);
	if (ret) {
		goto out;
	}
	spi_context_buffers_setup(&data->ctx, tx_bufs, rx_bufs, 1);
    spi_context_cs_control(&data->ctx, true);
	csk_spi_hal_transceive(dev);
	ret = spi_context_wait_for_completion(&data->ctx);

out:
	spi_context_release(&data->ctx, ret);
	return ret;
}

static int csk_spi_transceive(const struct device *dev,
			       const struct spi_config *spi_cfg,
			       const struct spi_buf_set *tx_bufs,
			       const struct spi_buf_set *rx_bufs)
{
	return transceive(dev, spi_cfg, tx_bufs, rx_bufs, false, NULL);
}

#ifdef CONFIG_SPI_ASYNC
static int csk_spi_transceive_async(const struct device *dev,
				     const struct spi_config *spi_cfg,
				     const struct spi_buf_set *tx_bufs,
				     const struct spi_buf_set *rx_bufs,
				     struct k_poll_signal *async)
{
	return transceive(dev, spi_cfg, tx_bufs, rx_bufs, true, async);
}
#endif /* CONFIG_SPI_ASYNC */

static int csk_spi_release(const struct device *dev, const struct spi_config *spi_cfg)
{
    struct csk_spi_data *data = dev->data;
    spi_context_unlock_unconditionally(&data->ctx);
    return 0;
}

static void csk_spi_isr_callback(uint32_t event, uint32_t usr_param)
{
	const struct device *dev = (const struct device *)usr_param;
	struct csk_spi_data *data = dev->data;

	if (event & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
		spi_context_update_tx(&data->ctx, 1, data->current_transfer_len);
		spi_context_update_rx(&data->ctx, 1, data->current_transfer_len);
		csk_spi_hal_transceive(dev);
	}
#ifdef CONFIG_SOC_LS_ADSP_CSK6
    else if (event & CSK_SPI_EVENT_DMA_COMPLETE){
		spi_context_update_tx(&data->ctx, 1, data->current_transfer_len);
		spi_context_update_rx(&data->ctx, 1, data->current_transfer_len);
		csk_spi_hal_transceive(dev);		
	}
#endif /* end of CONFIG_SOC_LS_ADSP_CSK6 */

    /* TODO: add transfer error processing */
}

static void csk_zephyr_spi_isr(const struct device *dev)
{
	struct csk_spi_data *data = dev->data;
	SPI_DEV *hal_spi = data->driver_context;

	if(hal_spi->irq_handler != NULL)
	{
		hal_spi->irq_handler();
	}
}

static const struct spi_driver_api csk_spi_driver_api = {
	.transceive = csk_spi_transceive,
 #ifdef CONFIG_SPI_ASYNC
 	.transceive_async = csk_spi_transceive_async,
 #endif
	 .release = csk_spi_release,
};

static int spi_csk_init(const struct device *dev)
{
	const struct csk_spi_config *config = DEV_CFG(dev);
	struct csk_spi_data *data = DEV_DATA(dev);

	/* Get device handle */
	data->driver_context = spi_csk_get_ctx(dev);
             
	/* Fixme: rework the SPI_Initialize(), let the user parameter become void* type */  
	SPI_Initialize(data->driver_context, csk_spi_isr_callback, (uint32_t)dev);                    
	SPI_PowerControl(data->driver_context, CSK_POWER_FULL);                                        

	PINCTRL_CONFIG_PINS(config->pinctrl_list, config->pinctrl_list_size);   

	/* IRQ_CONNECT and irq_enable */
	config->irq_config_func(dev);

	spi_context_unlock_unconditionally(&data->ctx);		

	return 0;	
}

#define SPI_DEVICE_INIT(n)                                                 	\
																				\
	static void spi_csk_config_func_##n(const struct device *dev)               \
	{             																		\
																						\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), csk_zephyr_spi_isr,                \
			    DEVICE_DT_INST_GET(n), 0);                                             \
                                                                                                   \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}																			\
																				\
	static const struct csk6_pinctrl_cfg                                        \
        spi_alts_##n[DT_INST_NUM_PINCTRLS_BY_IDX(n, 0)] =               \
            CSK6_DT_ALT_ITEMS_LIST(n);                                      \
									               								\
	static const struct csk_spi_config csk_spi_config_##n = {      			\
		.label = DT_INST_LABEL(n),											\
		.irq_config_func = spi_csk_config_func_##n,                             \
        .pinctrl_list = spi_alts_##n,                   					\
		.pinctrl_list_size = ARRAY_SIZE(spi_alts_##n),                      \
	};                                                                     		\
																				\
	static struct csk_spi_data csk_spi_data_##n = {	     					\
		SPI_CONTEXT_INIT_LOCK(csk_spi_data_##n, ctx),		 					\
		SPI_CONTEXT_INIT_SYNC(csk_spi_data_##n, ctx),		 					\
	};																			\
																				\
	DEVICE_DT_INST_DEFINE(n, &spi_csk_init, NULL,                    		\
			    &csk_spi_data_##n,                           				\
			    &csk_spi_config_##n, POST_KERNEL,            				\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                				\
			    &csk_spi_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPI_DEVICE_INIT)

#ifdef CONFIG_SOC_CSK6
#pragma GCC pop_options
#endif
