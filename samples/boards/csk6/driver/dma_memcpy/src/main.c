/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <string.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/spi.h>
#include <drivers/dma.h>

#define BUFFER_SIZE (1024 * 10)
K_SEM_DEFINE(dma_sem, 0, 1);

void dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status)
{
    if (status == 0) {
        k_sem_give(&dma_sem);
    }
}

void test_std_memcpy(void *src, void *dst, size_t size)
{
    printk("\nSTD memcpy start\n");
    uint32_t start = k_cycle_get_32();
    memcpy(dst, src, size);
    uint32_t time = (double)(k_cycle_get_32() - start) / sys_clock_hw_cycles_per_sec() * 1000 * 1000;
    printk("STD memecpy cost time: %d us\n", time);
    printk("STD memcpy finish\n");
}

void test_dma_copy(const struct device *dma, void *src, void *dst, size_t size)
{
    printk("\nDMA copy start\n");
    uint32_t start = k_cycle_get_32();
    dma_reload(dma, 1, (uint32_t)src, (uint32_t)dst, size);
    dma_start(dma, 1);
    k_sem_take(&dma_sem, K_FOREVER);
    uint32_t time = (double)(k_cycle_get_32() - start) / sys_clock_hw_cycles_per_sec() * 1000 * 1000;
    printk("DMA copy cost time: %d us\n", time);
    printk("DMA copy finish\n");
}

void main(void)
{
    const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma0));
    struct dma_config dma_cfg = {
        .channel_direction = MEMORY_TO_MEMORY,
        .dma_callback = dma_callback,
        .user_data = NULL
    };
    uint8_t *src_buffer = k_calloc(BUFFER_SIZE, sizeof(uint8_t));
    uint8_t *dst_buffer = k_calloc(BUFFER_SIZE, sizeof(uint8_t));
    if (src_buffer == NULL || dst_buffer == NULL) {
        printk("no memory");
        return;
    }
    for (int i = 0; i < BUFFER_SIZE; i++) {
        src_buffer[i] = i % UINT8_MAX;
    }

    dma_config(dma, 1, &dma_cfg);
    while (1) {
        test_dma_copy(dma, src_buffer, dst_buffer, BUFFER_SIZE);
        test_std_memcpy(src_buffer, dst_buffer, BUFFER_SIZE);
        k_msleep(1000);
    }
}
