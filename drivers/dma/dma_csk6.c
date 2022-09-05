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
#define DMA_CSK6_RAM_FUNC
#define ARCH_ISR_DIRECT_DECLARE(name) int name(void)
#else
#include "hal/dma.h"
#define DMA_CSK6_RAM_FUNC __attribute__((section(".itcm")))
#endif /* end of CONFIG_SOC_LS_ADSP_CSK6 */

LOG_MODULE_REGISTER(dma_csk6, CONFIG_DMA_LOG_LEVEL);

static void csk_dma_event_callback(uint32_t event_ctx, uint32_t xfer_bytes, uint32_t arg);

struct csk_dma_config {};

struct csk_dma_callback {
	dma_callback_t callback;
	void *user_data;
};

struct channel_info {
	struct csk_dma_callback cb_info;
	uint32_t channel_direction;
	uint32_t dst_addr;
	uint32_t src_addr;
	uint32_t transfer_size;
	uint32_t dma_ch_ctlx_cfg_mask;
	uint32_t dma_ch_cfgxl_cfg_mask;
	uint32_t dma_ch_cfgxh_cfg_mask;
	bool busy;
};

struct csk_dma_data {
	struct channel_info channels_info[DMA_CHANNEL_NUM_MAX];
};

static int csk_dma_configure(const struct device *dev, uint32_t channel, struct dma_config *config)
{
	struct csk_dma_data *data = dev->data;
	uint8_t real_channel = channel;
	uint32_t _DMA_CH_CTLx_CFG_MASK;
	uint32_t _DMA_CH_CFGxL_CFG_MASK;
	uint32_t _DMA_CH_CFGxH_CFG_MASK;

	if (dev == NULL || config == NULL) {
		return -EINVAL;
	}

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
	}

	if (config->source_chaining_en || config->dest_chaining_en) {
		LOG_ERR("Do not support source/dest block chaining.");
		return -ENOTSUP;
	}

	if (config->head_block == NULL) {
		LOG_ERR("Head blocl is null.");
		return -EINVAL;
	}

	if (config->head_block->source_address == 0 || config->head_block->dest_address == 0) {
		LOG_ERR("Dest address or source address is null.");
		return -EINVAL;
	}

	if (config->head_block->dest_reload_en || config->head_block->source_reload_en) {
		LOG_ERR("Do not support source/dest reload mode.");
		return -ENOTSUP;
	}

	uint32_t direct_mask = 0;
	uint32_t gatter_scatter_mask = 0;

	switch (config->channel_direction) {
	case MEMORY_TO_MEMORY:
		direct_mask = DMA_CH_CTLL_TTFC_M2M;
		break;
	default:
		LOG_ERR("DMA does not support this direction.");
		return -ENOTSUP;
	}

	if (config->head_block->source_addr_adj != DMA_ADDR_ADJ_INCREMENT ||
	    config->head_block->dest_addr_adj != DMA_ADDR_ADJ_INCREMENT) {
		LOG_ERR("Only support increment mode now.");
		return -ENOTSUP;
	}

	struct channel_info *ch_info = &data->channels_info[real_channel];
	uint32_t dst_width_mask = DMA_CH_CTLL_DST_WIDTH(config->dest_data_size);
	uint32_t src_width_mask = DMA_CH_CTLL_SRC_WIDTH(config->source_data_size);
	uint32_t dst_burst_size_mask = DMA_CH_CTLL_DST_BSIZE(config->dest_burst_length);
	uint32_t src_burst_size_mask = DMA_CH_CTLL_SRC_WIDTH(config->source_burst_length);
	uint32_t dst_src_ms_mask = DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
	uint32_t irq_en_mask = DMA_CH_CTLL_INT_EN;
	_DMA_CH_CTLx_CFG_MASK = direct_mask | gatter_scatter_mask | dst_width_mask |
				src_width_mask | dst_burst_size_mask | src_burst_size_mask |
				dst_src_ms_mask | irq_en_mask;
	_DMA_CH_CFGxL_CFG_MASK = DMA_CH_CFGL_CH_PRIOR(1);
	_DMA_CH_CFGxH_CFG_MASK = DMA_CH_CFGH_FIFO_MODE;
	ch_info->dma_ch_ctlx_cfg_mask = _DMA_CH_CTLx_CFG_MASK;
	ch_info->dma_ch_cfgxl_cfg_mask = _DMA_CH_CFGxL_CFG_MASK;
	ch_info->dma_ch_cfgxh_cfg_mask = _DMA_CH_CFGxH_CFG_MASK;
	ch_info->channel_direction = config->channel_direction;

	if (config->block_count > 1) {
		LOG_ERR("Only support a single data transfer.");
		return -ENOTSUP;
	}

	if (config->head_block != NULL) {
		ch_info->dst_addr = config->head_block->dest_address;
		ch_info->src_addr = config->head_block->source_address;
		ch_info->transfer_size = config->head_block->block_size;
	}

	if (config->dma_callback) {
		ch_info->cb_info.callback = config->dma_callback;
		ch_info->cb_info.user_data = config->user_data;
	}

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
	ch_info->src_addr = src;
	ch_info->dst_addr = dst;
	ch_info->transfer_size = size;
	return 0;
}

static bool csk_dma_addr_is_right_access(void *addr, uint32_t size)
{
	if (addr == NULL) {
		return false;
	}

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
#endif

	return !((uint32_t)addr & (__SCB_DCACHE_LINE_SIZE - 1U));
}

static int csk_dma_start(const struct device *dev, uint32_t channel)
{
	uint8_t real_channel = channel;
	struct csk_dma_data *data = dev->data;

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
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

	struct channel_info *ch_info = &data->channels_info[channel];
	if (ch_info->channel_direction == MEMORY_TO_MEMORY) {
		if (!csk_dma_addr_is_right_access((void *)ch_info->dst_addr,
						  ch_info->transfer_size)) {
			LOG_ERR("The destination address(0x%x) must be aligned in the nocache segment or on a 32-byte address!",
				ch_info->dst_addr);
			return -EINVAL;
		}

		int ret = dma_memcpy(channel, ch_info->src_addr, ch_info->dst_addr,
				     ch_info->transfer_size);
		if (ret != 0) {
			return -EIO;
		}
	} else if (ch_info->channel_direction == PERIPHERAL_TO_PERIPHERAL) {
		LOG_ERR("DMA does not support peripheral to peripheral direction now");
		return -ENOTSUP;
	} else {
		int ret =
			dma_channel_configure(channel, ch_info->src_addr, ch_info->dst_addr,
					      ch_info->transfer_size, ch_info->dma_ch_ctlx_cfg_mask,
					      ch_info->dma_ch_cfgxl_cfg_mask,
					      ch_info->dma_ch_cfgxh_cfg_mask, 0, 0);
		if (ret != 0) {
			return -EIO;
		}
	}
	ch_info->busy = true;
	return 0;
}

static int csk_dma_stop(const struct device *dev, uint32_t channel)
{
	return -ENOTSUP; /* Not available now */
}

static int csk_dma_get_status(const struct device *dev, uint32_t channel, struct dma_status *status)
{
	struct csk_dma_data *data = dev->data;

	if (channel >= DMA_CHANNEL_NUM_MAX) {
		LOG_ERR("Invalid channel num.");
		return -EINVAL;
	}

	struct channel_info *ch_info = &data->channels_info[channel];
	if (ch_info->busy) {
		status->busy = true;
		status->pending_length = ch_info->transfer_size - dma_channel_get_count(channel);
	} else {
		status->busy = false;
		status->pending_length = 0;
	}
	status->dir = ch_info->channel_direction;
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
	int ret = 1;
	struct device *dev = (struct device *)arg;
	struct csk_dma_data *data = dev->data;
	uint8_t channel = (uint16_t)event_ctx >> 8;
	uint8_t event_type = event_ctx & 0xFF;
	struct channel_info *ch_info = &data->channels_info[channel];
	if (event_type == DMA_EVENT_TRANSFER_COMPLETE) {
		ret = 0;
		ch_info->busy = false;
	}
	ch_info->cb_info.callback(dev, ch_info->cb_info.user_data, channel, ret);
}

ISR_DIRECT_DECLARE(dma_direct_irq_handler)
{
	extern void dma_irq_handler(void);
	dma_irq_handler();

	return 1;
}

static int csk_dma_init(const struct device *dev)
{
	int ret = dma_initialize();
	if (ret != 0) {
		return -EIO; /* unknown exception */
	}

#ifdef CONFIG_SOC_LS_ADSP_CSK6
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), dma_direct_irq_handler,
		    DEVICE_DT_INST_GET(0), 0);
#else
	IRQ_DIRECT_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), dma_direct_irq_handler,
			   Z_COND_CODE_1(CONFIG_ZERO_LATENCY_IRQS, (IRQ_ZERO_LATENCY), (0)));
#endif
	irq_enable(DT_INST_IRQN(0));
	return 0;
}

static struct csk_dma_config csk_dma_0_config = {};

static struct csk_dma_data csk_dma_0_data;

DEVICE_DT_INST_DEFINE(0, &csk_dma_init, NULL, &csk_dma_0_data, &csk_dma_0_config, POST_KERNEL,
		      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &csk_dma_driver_api);
