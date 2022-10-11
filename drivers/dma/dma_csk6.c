/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_dma

#include <kernel.h>
#include <device.h>
#include <soc.h>
#include <drivers/dma.h>
#include <logging/log.h>

#define DMA_CHANNEL_NUM_MAX DT_INST_PROP(0, dma_channels)

#ifdef CONFIG_SOC_LS_ADSP_CSK6
#include "hal/reg/dma_venus.h"
#define DMA_CSK6_RAM_FUNC __attribute__ ((section (".iram0.text")))
#define ARCH_ISR_DIRECT_DECLARE(name) int name(void)
#else
#include "hal/dma.h"
#define DMA_CSK6_RAM_FUNC __attribute__((section(".itcm")))
#endif /* end of CONFIG_SOC_LS_ADSP_CSK6 */

#define DMA_CH_SCATTER_INTERVAL_MASK (0xFFFFF)
#define DMA_CH_MAX_BLOCK_XFER_SIZE (4095)

LOG_MODULE_REGISTER(dma_csk6, CONFIG_DMA_LOG_LEVEL);

static void csk_dma_event_callback(uint32_t event_ctx, uint32_t xfer_bytes, uint32_t arg);

struct csk_dma_config {};

struct csk_dma_callback {
	dma_callback_t callback;
	void *user_data;
};

struct channel_info {
	struct csk_dma_callback cb_info;
	uint32_t direction;
	uint32_t dst_addr;
	uint32_t src_addr;
	uint32_t xfer_count;
	uint32_t reg_ctrl;
	uint32_t reg_cfg_low;
	uint32_t reg_cfg_high;
	uint32_t reg_scatter;
	uint32_t reg_gather;
	bool busy;
};

struct csk_dma_data {
	struct dma_context ctx;
	struct channel_info channels_info[DMA_CHANNEL_NUM_MAX];
};

static uint8_t csk_dma_burst_length_convert(uint32_t len)
{
	/* See @DMAC DataBook */
	const uint16_t table[] = { 1, 4, 8, 16, 32, 64, 128, 256 };
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(table); i++) {
		if (len == table[i]) {
			return i;
		}
	}

	__ASSERT(false, "Unsupported burst length(%d)", len);

	/* return default register value */
	return 0;
}

static int csk_dma_configure(const struct device *dev, uint32_t channel, struct dma_config *config)
{
	struct csk_dma_data *data;
	struct channel_info *ch_info;

	if (dev == NULL || config == NULL) {
		return -EINVAL;
	}

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
	}

	data = dev->data;
	ch_info = &data->channels_info[channel];

	if (ch_info->busy) {
		LOG_ERR("channel channel is busy");
		return -EBUSY;
	}

	memset(ch_info, 0, sizeof(struct channel_info));

	if (config->source_chaining_en || config->dest_chaining_en) {
		LOG_ERR("Do not support source/dest block chaining.");
		return -ENOTSUP;
	}

	/**
	 *  CSK6 DMA controller is not support hardware LLP mode.
	 * 
	 * DMA driver is not implement the software LLP mode now
	 * since software LLP has the poor real-time performance.
	 * If user want to use DMA to transfer multiple blocks,
	 * he should separate the data block and start multiple single DMA transaction.
	 */
	if (config->block_count > 1) {
		LOG_ERR("Only support data transfer once.");
		return -ENOTSUP;
	}

	if (config->dest_data_size != 1 && config->dest_data_size != 2 &&
	    config->dest_data_size != 4) {
		LOG_ERR("Do not support dest data width(%d).", config->dest_data_size);
		return -EINVAL;
	}

	if (config->source_data_size != 1 && config->source_data_size != 2 &&
	    config->source_data_size != 4) {
		LOG_ERR("Do not support src data width(%d).", config->source_data_size);
		return -EINVAL;
	}

	if (config->head_block != NULL) {
		if (config->head_block->dest_reload_en || config->head_block->source_reload_en) {
			LOG_ERR("Do not support reload mode.");
			return -ENOTSUP;
		}

		ch_info->dst_addr = config->head_block->dest_address;
		ch_info->src_addr = config->head_block->source_address;
		ch_info->xfer_count = config->head_block->block_size / config->source_data_size;

		if (config->head_block->fifo_mode_control) {
			/**
			 * Data available is greater than or equal to half the FIFO depth
			 * for destination transfers and space available is greater than half
			 * the fifo depth for source transfers. The exceptions are at the end
			 * of a burst transaction request or at the end of a block transfer.
			 */
			ch_info->reg_cfg_high |= DMA_CH_CFGH_FIFO_MODE;
		}

		__ASSERT(config->head_block->block_size % config->dest_data_size == 0,
			 "block size(%d) is not match data width(%d).",
			 config->head_block->block_size, config->source_data_size);

		if (ch_info->xfer_count > DMA_CH_MAX_BLOCK_XFER_SIZE) {
			LOG_ERR("transfer size out of range. Max transfer size is %d in bytes at data with is %d.",
				DMA_CH_MAX_BLOCK_XFER_SIZE * config->source_data_size,
				config->source_data_size);
			return -ENOTSUP;
		}
	}

	if (config->dma_callback != NULL) {
		ch_info->cb_info.callback = config->dma_callback;
		ch_info->cb_info.user_data = config->user_data;
	}

	/* width size set */
	ch_info->reg_ctrl = DMA_CH_CTLL_DST_WIDTH(config->dest_data_size >> 1);
	ch_info->reg_ctrl |= DMA_CH_CTLL_SRC_WIDTH(config->source_data_size >> 1);

	/* burst size set */
	ch_info->reg_ctrl |=
		DMA_CH_CTLL_DST_BSIZE(csk_dma_burst_length_convert(config->dest_burst_length));
	ch_info->reg_ctrl |=
		DMA_CH_CTLL_SRC_BSIZE(csk_dma_burst_length_convert(config->source_burst_length));

	/* intterupt enable */
	ch_info->reg_ctrl |= DMA_CH_CTLL_INT_EN;

	/* hardware specific configuration, do not touch this field. */
	ch_info->reg_ctrl |= DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);

	switch (config->channel_direction) {
	case MEMORY_TO_MEMORY:
		ch_info->reg_ctrl |= DMA_CH_CTLL_TTFC_M2M;
		break;
	case MEMORY_TO_PERIPHERAL:
		ch_info->reg_ctrl |= DMA_CH_CTLL_TTFC_M2P;
		break;
	case PERIPHERAL_TO_MEMORY:
		ch_info->reg_ctrl |= DMA_CH_CTLL_TTFC_P2M;
		break;
	default:
		LOG_ERR("DMA does not support this direction.");
		return -ENOTSUP;
	}

	switch (config->head_block->dest_addr_adj) {
	case DMA_ADDR_ADJ_INCREMENT:
		ch_info->reg_ctrl |= DMA_CH_CTLL_DST_INC;
		break;
	case DMA_ADDR_ADJ_DECREMENT:
		ch_info->reg_ctrl |= DMA_CH_CTLL_DST_DEC;
		break;
	case DMA_ADDR_ADJ_NO_CHANGE:
		ch_info->reg_ctrl |= DMA_CH_CTLL_DST_FIX;
		break;
	default:
		LOG_ERR("Invalid dest addr adj.");
		return -EINVAL;
	}

	switch (config->head_block->source_addr_adj) {
	case DMA_ADDR_ADJ_INCREMENT:
		ch_info->reg_ctrl |= DMA_CH_CTLL_SRC_INC;
		break;
	case DMA_ADDR_ADJ_DECREMENT:
		ch_info->reg_ctrl |= DMA_CH_CTLL_SRC_DEC;
		break;
	case DMA_ADDR_ADJ_NO_CHANGE:
		ch_info->reg_ctrl |= DMA_CH_CTLL_SRC_FIX;
		break;
	default:
		LOG_ERR("Invalid source addr adj.");
		return -EINVAL;
	}

	/**
	 * channel priority set
	 * 
	 * It should be noted that the priority represents the 
	 * response priority of the channel, not the preemption priority.
	 */
	ch_info->reg_cfg_low =
		DMA_CH_CFGL_CH_PRIOR(config->channel_priority) & DMA_CH_CFGL_CH_PRIOR_MASK;

	if (config->channel_direction == PERIPHERAL_TO_MEMORY) {
		ch_info->reg_cfg_high |= DMA_CH_CFGH_SRC_PER(config->dma_slot);
	} else if (config->channel_direction == MEMORY_TO_PERIPHERAL) {
		ch_info->reg_cfg_high |= DMA_CH_CFGH_DST_PER(config->dma_slot);
	}

	if (config->head_block->dest_scatter_en) {
		ch_info->reg_scatter =
			(config->head_block->dest_scatter_interval & DMA_CH_SCATTER_INTERVAL_MASK) |
			(config->head_block->dest_scatter_count << 20);
		ch_info->reg_ctrl |= DMA_CH_CTLL_D_SCAT_EN;
	}

	if (config->head_block->source_gather_en) {
		ch_info->reg_gather = (config->head_block->source_gather_interval &
				       DMA_CH_SCATTER_INTERVAL_MASK) |
				      (config->head_block->source_gather_count << 20);
		ch_info->reg_ctrl |= DMA_CH_CTLL_S_GATH_EN;
	}

	ch_info->direction = config->channel_direction;

	/**
	 * Due to the limitations of the underlying hal library,
	 * we do not directly write these configurations to the registers,
	 * we will write to the registers in the start interface
	 * and start the DMA channel transfer.
	 * 
	 * We don't yet have an elegant way to control the interrupt register,
	 * so we temporarily default to using DMA for complete interrupts and error interrupts.
	 */

	return 0;
}

static int csk_dma_reload(const struct device *dev, uint32_t channel, uint32_t src, uint32_t dst,
			  size_t size)
{
	struct csk_dma_data *data = dev->data;

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
	}

	struct channel_info *ch_info = &data->channels_info[channel];

	if (ch_info->busy) {
		LOG_ERR("Channel busy");
		return -EBUSY;
	}

	ch_info->src_addr = src;
	ch_info->dst_addr = dst;
	ch_info->xfer_count = size;

	return 0;
}

static bool csk_dma_addr_is_right_access(void *addr, uint32_t size)
{
	if (addr == NULL) {
		return false;
	}
#ifndef CONFIG_DMA_CSK6_BUFFER_CHECK
	return true;
#else

#ifndef CONFIG_SOC_LS_ADSP_CSK6
	/* D-Cache is disabled? */
	if (!(SCB->CCR & SCB_CCR_DC_Msk)) {
		return true;
	}
#if CONFIG_NOCACHE_MEMORY
	extern char _nocache_ram_start[];
	extern char _nocache_ram_end[];
	extern char _nocache_ram_size[];

	/* make compiler happy */
	(void)_nocache_ram_size;

	/* may be at no-cache section */
	if (((uint32_t)_nocache_ram_start <= (uint32_t)addr) &&
	    (((uint32_t)addr) <= (uint32_t)_nocache_ram_end)) {
		return true;
	}

#endif /* End of CONFIG_NOCACHE_MEMORY */
	return !((uint32_t)addr & (__SCB_DCACHE_LINE_SIZE - 1U));
#else
	return true;
#endif /* End of CONFIG_SOC_LS_ADSP_CSK6 */

#endif /* End of CONFIG_DMA_CSK6_BUFFER_CHECK */
}

static int csk_dma_start(const struct device *dev, uint32_t channel)
{
	uint8_t real_channel = channel;
	struct csk_dma_data *data = dev->data;
	struct channel_info *ch_info = &data->channels_info[channel];

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
	}

	if (ch_info->busy) {
		return -EBUSY;
	}

	dma_channel_select(&real_channel, csk_dma_event_callback, (uint32_t)dev,
			   DMA_CACHE_SYNC_BOTH);

	if (real_channel == DMA_CHANNEL_ANY) {
		LOG_ERR("NO free dma channel is available");
		return -EBUSY;
	} else if (real_channel != channel) {
		LOG_ERR("DMA channel [%d] is not available, try to use channel [%d]", channel,
			real_channel);
		return -EBUSY;
	}

	if (ch_info->direction == MEMORY_TO_MEMORY || ch_info->direction == PERIPHERAL_TO_MEMORY) {
		uint8_t width = (ch_info->reg_ctrl & DMA_CH_CTLL_DST_WIDTH_MASK) >>
				DMA_CH_CTLL_DST_WIDTH_POS;
		/* Register value convert to real width */
		width <<= 1;

		if (!csk_dma_addr_is_right_access((void *)ch_info->dst_addr,
						  ch_info->xfer_count * width)) {
			LOG_ERR("The destination address(0x%x) must be aligned in the nocache memory or on a 32-byte address!",
				ch_info->dst_addr);
			return -EINVAL;
		}
	}

	if (ch_info->src_addr == 0 || ch_info->dst_addr == 0) {
		LOG_ERR("Source address or destination address is null!");
		return -EINVAL;
	}

	int ret = dma_channel_configure(channel, ch_info->src_addr, ch_info->dst_addr,
					ch_info->xfer_count, ch_info->reg_ctrl,
					ch_info->reg_cfg_low, ch_info->reg_cfg_high,
					ch_info->reg_gather, ch_info->reg_scatter);

	if (ret != 0) {
		return -EIO;
	}

	ch_info->busy = true;

	return 0;
}

static int csk_dma_stop(const struct device *dev, uint32_t channel)
{
	struct csk_dma_data *data = dev->data;
	struct channel_info *ch_info = NULL;

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
	}

	if (dma_channel_disable(channel, 0) != 0) {
		return -EIO;
	}

	ch_info = &data->channels_info[channel];
	ch_info->busy = false;

	return 0;
}

static int csk_dma_get_status(const struct device *dev, uint32_t channel, struct dma_status *status)
{
	struct csk_dma_data *data = dev->data;
	struct channel_info *ch_info = NULL;

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
	}

	ch_info = &data->channels_info[channel];

	status->busy = ch_info->busy;
	status->dir = ch_info->direction;
	status->pending_length =
		status->busy ? ch_info->xfer_count - dma_channel_get_count(channel) : 0;

	return 0;
}

static const struct dma_driver_api csk_dma_driver_api = {
	.config = csk_dma_configure,
	.reload = csk_dma_reload,
	.start = csk_dma_start,
	.stop = csk_dma_stop,
	.get_status = csk_dma_get_status,
};

DMA_CSK6_RAM_FUNC static void csk_dma_event_callback(uint32_t event_ctx, uint32_t xfer_bytes,
						     uint32_t arg)
{
	int status = -EIO;
	struct device *dev = (struct device *)arg;
	struct csk_dma_data *data = dev->data;
	uint8_t channel = (uint16_t)event_ctx >> 8;
	uint8_t event_type = event_ctx & 0xFF;
	struct channel_info *ch_info = &data->channels_info[channel];

	switch (event_type) {
	case DMA_EVENT_TRANSFER_COMPLETE:
		ch_info->busy = false;
		status = 0;
		break;
	case DMA_EVENT_ERROR:
		ch_info->busy = false;
		status = -EIO;
		break;
	default:
		/* No callback required */
		return;
	}

	if (ch_info->cb_info.callback) {
		ch_info->cb_info.callback(dev, ch_info->cb_info.user_data, channel, status);
	}
}

ISR_DIRECT_DECLARE(dma_direct_irq_handler)
{
	extern void dma_irq_handler(void);
	dma_irq_handler();

	return 1;
}

static int csk_dma_init(const struct device *dev)
{
	struct csk_dma_data *data = dev->data;

	if (dma_initialize() != 0) {
		return -EIO; /* unknown exception */
	}

	/* Not supported context */
	memset(&data->ctx, 0, sizeof(struct dma_context));

#ifdef CONFIG_SOC_LS_ADSP_CSK6
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), dma_direct_irq_handler,
		    DEVICE_DT_INST_GET(0), 0);
#else
#ifdef CONFIG_ZERO_LATENCY_IRQS
	IRQ_DIRECT_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), dma_direct_irq_handler,
			   IRQ_ZERO_LATENCY);
#else
	IRQ_DIRECT_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), dma_direct_irq_handler, 0);
#endif
#endif

	irq_enable(DT_INST_IRQN(0));

	return 0;
}

static struct csk_dma_config csk_dma_0_config = {};

static struct csk_dma_data csk_dma_0_data;

DEVICE_DT_INST_DEFINE(0, &csk_dma_init, NULL, &csk_dma_0_data, &csk_dma_0_config, POST_KERNEL,
		      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &csk_dma_driver_api);
