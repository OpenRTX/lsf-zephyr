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

#define BUFFER_SIZE (8192)
#define TEST_BUFFER_SIZE 2048

K_SEM_DEFINE(dma_sem, 0, 1);

void dump(void *data, uint32_t size)
{
    char *ch = (char *)data;
    uint32_t c = 0;
    while(size--) {
        printk("%02X ", *ch++);

        if (++c % 8 == 0) {
            printk("\n");
        }
    }
    printk("\n");
}

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
    uint32_t time = (k_cycle_get_32() - start);
    printk("STD memecpy cost cycles: %d\n", time);
    printk("STD memcpy finish\n");
}

void test_dma_copy(const struct device *dma, void *src, void *dst, size_t size)
{
    int32_t r;

    struct dma_block_config block_config = {
        .source_address = (uint32_t)((char *)src ),
        .dest_address = (uint32_t)((char *)dst),
        .block_size = size,
        .dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
        .source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
        .dest_scatter_count = 4,
        .dest_scatter_en = 0,
        .dest_scatter_interval = 2,
    };

    struct dma_config dma_cfg = {
        .channel_direction = MEMORY_TO_MEMORY,
        .dma_callback = dma_callback,
        .user_data = NULL,
        .block_count = 1,
        .head_block = &block_config,
        .source_data_size = 4,
        .dest_data_size = 4,
        .channel_priority = 1,
    };

    memset(dst, 0, size);

    uint32_t start = k_cycle_get_32();
    r = dma_config(dma, 1, &dma_cfg);
    if (r != 0) {
        printk("\nDMA config failed, return value:%d\n", r);
        return;
    }
    r = dma_start(dma, 1);
    if (r != 0) {
        printk("\nDMA start failed.\n");
        return;
    }
    k_sem_take(&dma_sem, K_FOREVER);
    uint32_t time = (k_cycle_get_32() - start);
    printk("DMA copy cost cycles: %d\n", time);
    printk("DMA copy finish\n");
    // dump(dst, size);
}

static uint8_t src_buffer[BUFFER_SIZE] __attribute__((__section__(".nocache"))) = { 0 };
static uint8_t dst_buffer[BUFFER_SIZE] __attribute__((__section__(".nocache"))) = { 0 };

void main(void)
{
    const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma0));

    if (src_buffer == NULL || dst_buffer == NULL) {
        printk("no memory");
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        src_buffer[i] = i;
    }

    while (1) {
        test_dma_copy(dma, src_buffer, dst_buffer, TEST_BUFFER_SIZE);
        test_std_memcpy(src_buffer, dst_buffer, TEST_BUFFER_SIZE);
        k_msleep(1000);
    }
}
