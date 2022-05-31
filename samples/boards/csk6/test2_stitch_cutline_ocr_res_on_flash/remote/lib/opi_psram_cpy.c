#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sof/audio/cv/include/opi_cpy/opi_psram_cpy.h>

// #define USE_DMA_COPY

void dma_init()
{
	dma_initialize();
}

void dma_wait_complete(int chn)
{
#ifdef USE_DMA_COPY
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

    uint8_t real_channel = 2;

    dma_channel_select(&real_channel, DMA_DrvEvent, NULL, DMA_CACHE_SYNC_BOTH);

	dma_memcpy(real_channel, src, dst, size);
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
