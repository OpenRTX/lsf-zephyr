/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <device.h>
#include <drivers/dma.h>
#include <drivers/spi.h>
#include <kernel.h>
#include <sys/printk.h>
#include <zephyr.h>

#define BUFFER_SIZE		    (1024 * 100)
#define CONFIG_TEST_DMA_CHANNEL_NUN (1)

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
	uint32_t time =
		(double)(k_cycle_get_32() - start) / sys_clock_hw_cycles_per_sec() * 1000 * 1000;
	printk("STD memecpy cost time: %d us\n", time);
	printk("STD memcpy finish\n");
}

void test_dma_copy(const struct device *dma, void *src, void *dst, size_t size)
{
	int32_t r;

	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = size,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 4,
		.dest_data_size = 1,
		.channel_priority = 1,
	};

	memset(dst, 0, size);

	printk("\nDMA copy start\n");
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
	uint32_t time =
		(double)(k_cycle_get_32() - start) / sys_clock_hw_cycles_per_sec() * 1000 * 1000;
	printk("DMA copy cost time: %d us\n", time);
	printk("DMA copy finish\n");
}

#ifdef CONFIG_NOCACHE_MEMORY
static uint8_t src_buffer[BUFFER_SIZE] __attribute__((__section__(".nocache"))) = {0};
static uint8_t dst_buffer[BUFFER_SIZE] __attribute__((__section__(".nocache"))) = {0};
#else
static uint8_t src_buffer[BUFFER_SIZE] __attribute__((__aligned__(32))) = {0};
static uint8_t dst_buffer[BUFFER_SIZE] __attribute__((__aligned__(32))) = {0};
#endif

void dump(void *datas, uint32_t len)
{
	uint32_t c = 0;
	char *ch = datas;

	while (len--) {
		printk("%02X ", *ch++);
		if (++c % 8 == 0) {
			printk("\n");
		}
	}
	printk("\n");
}

void dump2(void *datas, uint32_t x, uint32_t y, uint8_t bits)
{
	uint32_t c = 0;
	char *ch = datas;
	uint32_t len = x * y;

	while (len--) {
		if (bits == 1) {
			printk("%02X ", *ch++);
		} else {
			printk("%02X", *ch++);
			printk("%02X ", *ch++);
		}

		if (++c % (x) == 0) {
			printk("\n");
		}
	}

	printk("\n");
}

#include <stdint.h>

int32_t test_rotate_90(const struct device *dma, void *src, void *dst, uint32_t h, uint32_t w,
		       uint8_t b)
{
	int r;

	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = w * b,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.dest_scatter_en = 1,
		.dest_scatter_count = 1,
		.dest_scatter_interval = h - 1,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = b,
		.dest_data_size = b,
		.channel_priority = 1,
	};

	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	memset(dst, 0, h * w * b);

	uint32_t i = 0;
	uint32_t time = k_uptime_get_32();

	uint32_t x_offset = 0;
	uint32_t y_offset = h * b;

	while (i < h) {
		x_offset = i * w * b;
		y_offset -= b;
		// printk("x_offset%d, y_offset%d\n", x_offset, y_offset);
		r = dma_reload(dma, CONFIG_TEST_DMA_CHANNEL_NUN, src + x_offset,
			       dst + y_offset, w * b);

		if (r) {
			printk("DMA reload failed, error code:%d\n", r);
			return -1;
		}

		if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
			printk("DMA start failed, error code:%d\n", r);
			return -1;
		}
		k_sem_take(&dma_sem, K_FOREVER);
		i++;
	}
	time = k_uptime_get_32() - time;
	printk("cost time %d\n", time);
	// dump2(src_buffer, w, h, b);
	// dump2(dst_buffer, h, w, b);


	return 0;
}

int32_t sample_dma_scatter(const struct device *dma, void *src, void *dst, size_t size)
{
	int r;

	size = 64;
	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = 4,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.dest_scatter_en = 1,
		.dest_scatter_count = 1,
		.dest_scatter_interval = 3,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 1,
		.dest_data_size = 1,
		.channel_priority = 1,
	};

	memset(dst, 0, size);

	dump2(src, 4, 4, 1);

	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
		printk("DMA start failed, error code:%d\n", r);
		return -1;
	}
	
	k_sem_take(&dma_sem, K_FOREVER);
	printk("------ %s ------\n", __FUNCTION__);
	// dump(dst, size);
	dump2(dst, 4, 4, 1);

	return 0;
}

int32_t sample_dma_gather(const struct device *dma, void *src, void *dst, size_t size)
{
	int r;

	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = size,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_gather_en = 1,
		.source_gather_count = 4,
		.source_gather_interval = 2,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 1,
		.dest_data_size = 1,
		.channel_priority = 1,
	};

	memset(dst, 0, size);

	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
		printk("DMA start failed, error code:%d\n", r);
		return -1;
	}

	k_sem_take(&dma_sem, K_FOREVER);
	printk("------ %s ------\n", __FUNCTION__);
	dump(dst, size);

	return 0;
}

int32_t sample_dma_source_decrement(const struct device *dma, void *src, void *dst, size_t size)
{
	int r;

	struct dma_block_config block_config = {
		.source_address = (uint32_t)((char *)src + size - 1),
		.dest_address = (uint32_t)dst,
		.block_size = size,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_DECREMENT,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 1,
		.dest_data_size = 1,
		.channel_priority = 1,
	};

	memset(dst, 0, size);

	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
		printk("DMA start failed, error code:%d\n", r);
		return -1;
	}

	k_sem_take(&dma_sem, K_FOREVER);
	printk("------ %s ------\n", __FUNCTION__);
	dump(dst, size);

	return 0;
}

int32_t sample_dma_dest_decrement(const struct device *dma, void *src, void *dst, size_t size)
{
	int r;

	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)((char *)dst + size - 1),
		.block_size = size,
		.dest_addr_adj = DMA_ADDR_ADJ_DECREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 1,
		.dest_data_size = 1,
		.channel_priority = 1,
	};

	memset(dst, 0, size);

	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
		printk("DMA start failed, error code:%d\n", r);
		return -1;
	}

	k_sem_take(&dma_sem, K_FOREVER);
	printk("------ %s ------\n", __FUNCTION__);
	dump(dst, size);

	return 0;
}

int32_t sample_dma_dest_data_width(const struct device *dma, void *src, void *dst, size_t size)
{
	int r;

	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = size,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 1,
		.dest_data_size = 2,
		.channel_priority = 1,
	};

	memset(dst, 0, size);

	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
		printk("DMA start failed, error code:%d\n", r);
		return -1;
	}

	k_sem_take(&dma_sem, K_FOREVER);
	printk("------ %s ------\n", __FUNCTION__);
	dump(dst, size);

	return 0;
}

int32_t sample_dma_source_data_width(const struct device *dma, void *src, void *dst, size_t size)
{
	int r;

	struct dma_block_config block_config = {
		.source_address = (uint32_t)src,
		.dest_address = (uint32_t)dst,
		.block_size = size,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	};

	struct dma_config dma_cfg = {
		.channel_direction = MEMORY_TO_MEMORY,
		.dma_callback = dma_callback,
		.user_data = NULL,
		.block_count = 1,
		.head_block = &block_config,
		.source_data_size = 4,
		.dest_data_size = 1,
		.channel_priority = 1,
	};

	memset(dst, 0, size);

	if ((r = dma_config(dma, CONFIG_TEST_DMA_CHANNEL_NUN, &dma_cfg)) != 0) {
		printk("DMA config error, error code:%d\n", r);
		return -1;
	}

	if ((r = dma_start(dma, CONFIG_TEST_DMA_CHANNEL_NUN)) != 0) {
		printk("DMA start failed, error code:%d\n", r);
		return -1;
	}

	k_sem_take(&dma_sem, K_FOREVER);
	printk("------ %s ------\n", __FUNCTION__);
	dump(dst, size);

	return 0;
}

void main(void)
{
	const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma0));

	if (src_buffer == NULL || dst_buffer == NULL) {
		printk("no memory");
		return;
	}

	for (int i = 0; i < BUFFER_SIZE; i++) {
		src_buffer[i] = i % UINT8_MAX;
	}


	// sample_dma_scatter(dma, src_buffer, dst_buffer, 16);


	test_rotate_90(dma, src_buffer, dst_buffer, 2, 90* 454, 2);

	while (1) {
		// sample_dma_gather(dma, src_buffer, dst_buffer, 16);

		// sample_dma_dest_decrement(dma, src_buffer, dst_buffer, 16);
		// sample_dma_source_decrement(dma, src_buffer, dst_buffer, 16);

		// sample_dma_dest_data_width(dma, src_buffer, dst_buffer, 16);
		// sample_dma_source_data_width(dma, src_buffer, dst_buffer, 16);

		// test_dma_copy(dma, src_buffer, dst_buffer, BUFFER_SIZE);
		// test_std_memcpy(src_buffer, dst_buffer, BUFFER_SIZE);
		k_msleep(1000);
	}
}
