/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_dma

#include <kernel.h>
#include <device.h>
#include <soc.h>
#include <drivers/dma.h>
#include <logging/log.h>

#ifdef CONFIG_SOC_LS_ADSP_CSK6
#include "hal/reg/dma_venus.h"
#else
#include "hal/dma.h"
#endif

#ifdef CONFIG_SOC_LS_ADSP_CSK6
#define DMA_CSK6_RAM_FUNC 
#else
#define DMA_CSK6_RAM_FUNC __attribute__ ((section (".itcm"))) 
#endif /* end of CONFIG_SOC_LS_ADSP_CSK6 */

LOG_MODULE_REGISTER(dma_csk6, CONFIG_DMA_LOG_LEVEL);

static void
csk_dma_event_callback(uint32_t event_ctx, uint32_t xfer_bytes, uint32_t arg);

struct csk_dma_config {
	uint8_t dma_channels_num;
};

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
    struct channel_info *channels_info;
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

	uint32_t direct_mask = 0;
	uint32_t gatter_scatter_mask = 0;
	switch (config->channel_direction) {
		case MEMORY_TO_MEMORY:
			direct_mask = DMA_CH_CTLL_TTFC_M2M;
			gatter_scatter_mask = DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_INC;
		    break;
        case MEMORY_TO_PERIPHERAL:
            direct_mask = DMA_CH_CTLL_TTFC_M2P;
            gatter_scatter_mask = DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_DST_FIX;
            break;
        case PERIPHERAL_TO_MEMORY:
            direct_mask = DMA_CH_CTLL_TTFC_P2M;
            gatter_scatter_mask = DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_INC;
            break;
		default:
			LOG_ERR("DMA does not support peripheral to peripheral direction now");
		    return -ENOTSUP;
	}
    struct channel_info *ch_info = &data->channels_info[real_channel];
	uint32_t dst_width_mask = DMA_CH_CTLL_DST_WIDTH(config->dest_data_size);
	uint32_t src_width_mask = DMA_CH_CTLL_SRC_WIDTH(config->source_data_size);
	uint32_t dst_burst_size_mask = DMA_CH_CTLL_DST_BSIZE(config->dest_burst_length);
	uint32_t src_burst_size_mask = DMA_CH_CTLL_SRC_WIDTH(config->source_burst_length);
	uint32_t dst_src_ms_mask = DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0);
	uint32_t irq_en_mask = DMA_CH_CTLL_INT_EN;
	_DMA_CH_CTLx_CFG_MASK = direct_mask | gatter_scatter_mask | dst_width_mask | src_width_mask |
							dst_burst_size_mask | src_burst_size_mask | dst_src_ms_mask | irq_en_mask;
	_DMA_CH_CFGxL_CFG_MASK = DMA_CH_CFGL_CH_PRIOR(1);
	_DMA_CH_CFGxH_CFG_MASK = DMA_CH_CFGH_FIFO_MODE;
    ch_info->dma_ch_ctlx_cfg_mask = _DMA_CH_CTLx_CFG_MASK;
    ch_info->dma_ch_cfgxl_cfg_mask = _DMA_CH_CFGxL_CFG_MASK;
    ch_info->dma_ch_cfgxh_cfg_mask = _DMA_CH_CFGxH_CFG_MASK;
    ch_info->channel_direction = config->channel_direction;

	if (config->dma_callback) {
        ch_info->cb_info.callback = config->dma_callback;
        ch_info->cb_info.user_data = config->user_data;
	}
    return 0;
}

static int csk_dma_reload(const struct device *dev, uint32_t channel, uint32_t src, uint32_t dst, size_t size)
{
	struct csk_dma_data *data = dev->data;
    if (channel == -1) {
        return -EINVAL;
    }

    struct channel_info *ch_info = &data->channels_info[channel];
    ch_info->src_addr = src;
    ch_info->dst_addr = dst;
    ch_info->transfer_size = size;
    return 0;
}

static int csk_dma_start(const struct device *dev, uint32_t channel)
{
    uint8_t real_channel = channel;
    struct csk_dma_data *data = dev->data;
    if (channel == -1) {
        return -EINVAL;
    }

    dma_channel_select(&real_channel, csk_dma_event_callback, (uint32_t) dev, DMA_CACHE_SYNC_BOTH);
    if (real_channel == DMA_CHANNEL_ANY) {
        LOG_ERR("NO free dma channel is available");
        return -EBUSY;
    } else if (real_channel != channel) {
        LOG_ERR("DMA channel [%d] is not available, try to use channel [%d]", channel, real_channel);
        return -EBUSY;
    }

    struct channel_info *ch_info = &data->channels_info[channel];
    if (ch_info->channel_direction == MEMORY_TO_MEMORY) {
        int ret = dma_memcpy(channel, ch_info->src_addr, ch_info->dst_addr, ch_info->transfer_size);
        if (ret != 0) {
            return -EIO;
        }
    } else if (ch_info->channel_direction == PERIPHERAL_TO_PERIPHERAL) {
        LOG_ERR("DMA does not support peripheral to peripheral direction now");
        return -ENOTSUP;
    } else {
        int ret = dma_channel_configure(channel,
                                        ch_info->src_addr, ch_info->dst_addr,
                                        ch_info->transfer_size,
                                        ch_info->dma_ch_ctlx_cfg_mask,
                                        ch_info->dma_ch_cfgxl_cfg_mask,
                                        ch_info->dma_ch_cfgxh_cfg_mask,
                                        0, 0);
        if (ret != 0) {
            return -EIO;
        }
    }
    ch_info->busy = true;
    return 0;
}

static int csk_dma_stop(const struct device *dev, uint32_t channel)
{
    return -ENOTSUP;    /* Not available now */
}

static int csk_dma_get_status(const struct device *dev, uint32_t channel, struct dma_status *status)
{
    struct csk_dma_data *data = dev->data;
    if (channel == -1) {
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

DMA_CSK6_RAM_FUNC
 static void
csk_dma_event_callback(uint32_t event_ctx, uint32_t xfer_bytes, uint32_t arg)
{
    int ret = 1;
    struct device* dev = (struct device*)arg;
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

extern void dma_irq_handler(void);
DMA_CSK6_RAM_FUNC
static void
csk_dma_irq_handler(const struct device *dev)
{
    GPIO_RegDef* reg = 0x45900000;
    reg->REG_DATAOUT.all |= (1 << 4);
    dma_irq_handler();
    reg->REG_DATAOUT.all &= ~(1 << 4);
}

ISR_DIRECT_DECLARE(dma_direct_irq_handler)
{
    GPIO_RegDef* reg = 0x45900000;
    reg->REG_DATAOUT.all |= (1 << 4);
    dma_irq_handler();
    reg->REG_DATAOUT.all &= ~(1 << 4);
    return 1;
}

static int csk_dma_init(const struct device *dev)
{
	struct csk_dma_data *data = dev->data;
    const struct csk_dma_config *config = dev->config;
	int ret = dma_initialize();
    if (ret != 0) {
        return -EIO; /* unknown exception */
    }

	data->channels_info = k_calloc(config->dma_channels_num, sizeof(struct channel_info));
	if (data->channels_info == NULL) {
		return -ENOMEM;
	}
    
    IRQ_DIRECT_CONNECT(DT_INST_IRQN(0),DT_INST_IRQ(0, priority),
                dma_direct_irq_handler, Z_COND_CODE_1(CONFIG_ZERO_LATENCY_IRQS, (IRQ_ZERO_LATENCY), (0)));
	irq_enable(DT_INST_IRQN(0));
    return 0;
}

static struct csk_dma_config csk_dma_0_config = {
    .dma_channels_num = DT_INST_PROP(0, dma_channels),
};

static struct csk_dma_data csk_dma_0_data;

DEVICE_DT_INST_DEFINE(0,					                    \
                    &csk_dma_init,				                \
                    NULL,					                    \
                    &csk_dma_0_data,                            \
                    &csk_dma_0_config,                          \
                    POST_KERNEL,                                \
                    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,        \
                    &csk_dma_driver_api);
