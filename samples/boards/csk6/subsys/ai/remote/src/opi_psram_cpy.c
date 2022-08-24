#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <drivers/dma.h>
#include <opi_psram_cpy.h>
#include "hal/reg/dma_venus.h"

// #define ZEPHYR
#define USE_DMA_COPY


static uint32_t volatile DMAEvent;


static void DMA_DrvEvent(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    // printk("[%s]: event = %d, channel = %d, xfer_bytes = %d\r\n", __func__,
    //         event_info & 0xFF, (event_info >> 8) & 0xFF, xfer_bytes);
    DMAEvent = event_info & 0xFF;
}



K_SEM_DEFINE(dma_sem, 0, 1);

void dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
    if (status == 0) {
        k_sem_give(&dma_sem);
    }
}

void dma_init()
{
#ifdef ZEPHYR
    const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma0));
    struct dma_config dma_cfg = {
        .channel_direction = MEMORY_TO_MEMORY,
        .dma_callback = dma_callback,
        .user_data = NULL
    };
	dma_config(dma, 1, &dma_cfg);
#else
	dma_initialize();
#endif
}

void dma_wait_complete(int chn)
{
#ifdef USE_DMA_COPY

#ifdef ZEPHYR
	k_sem_take(&dma_sem, K_FOREVER);
#else
	// Block
	while(1){
		// Complete
		if(DMAEvent & DMA_EVENT_TRANSFER_COMPLETE){
			DMAEvent &= (~DMA_EVENT_TRANSFER_COMPLETE);
			break;
		}
		// Error
		if(DMAEvent & DMA_EVENT_ERROR){
			return;
		}
	}
#endif

#else
	return;
#endif
}

void dma_cpy_async(int chn, void *dst, void *src, int32_t size)
{
#ifdef USE_DMA_COPY
	if ((uint32_t)src >> 20 == 0x5fe)
	{
		src = (void *)((uint32_t)src & (uint32_t)0x480FFFFF);
	}
	if ((uint32_t)dst >> 20 == 0x5fe)
	{
		dst = (void *)((uint32_t)dst & (uint32_t)0x480FFFFF);
	}

#ifdef ZEPHYR
	const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma0));
	dma_reload(dma, 1, (uint32_t)src, (uint32_t)dst, size);
	dma_start(dma, 1);
#else
    uint8_t real_channel = 2;

    dma_channel_select(&real_channel, DMA_DrvEvent, NULL, DMA_CACHE_SYNC_BOTH);

	dma_memcpy(real_channel, src, dst, size);
#endif

#else
	memcpy(dst, src, size);
#endif
}


void opi_psram_cpy_in(void *dst, void *src, int32_t size)
{
	//memcpy(dst, src, size);
	dma_cpy_async(0, dst, src, size);
	dma_wait_complete(0);
}

void opi_psram_cpy_out(void *dst, void *src, int32_t size)
{
	//memcpy(dst, src, size);
	dma_cpy_async(0, dst, src, size);
	dma_wait_complete(0);
}

void opi_psram_cpy_in_pro(void *dst, void *src, int32_t size, dma_sync_call_func func, void* param)
{
	if (NULL != func)
	{
		func(param);
	}
	memcpy(dst, src, size);
}

void opi_psram_cpy_out_pro(void *dst, void *src, int32_t size, dma_sync_call_func func, void* param)
{
	if (NULL != func)
	{
		func(param);
	}
	memcpy(dst, src, size);
}
