/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/spi.h>
#include <string.h>

#define MASTER_MODE 0
#define BUF_SIZE 9

#if MASTER_MODE
uint8_t master_buffer_tx[BUF_SIZE] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

#if (CONFIG_SPI_RX_DMA == 1) & (CONFIG_NOCACHE_MEMORY == 1)
uint8_t master_buffer_rx[BUF_SIZE] __attribute__((section(".nocache"))) = {0};
#else
uint8_t *master_buffer_rx = NULL;
#endif /* end of (CONFIG_SPI_RX_DMA == 1) & (CONFIG_NOCACHE_MEMORY == 1) */

#else

uint8_t slave_buffer_tx[BUF_SIZE] = { 9, 8, 7, 6, 5, 4, 3, 2, 1 };
#if (CONFIG_SPI_RX_DMA == 1) & (CONFIG_NOCACHE_MEMORY == 1)
uint8_t slave_buffer_rx[BUF_SIZE] __attribute__((section(".nocache"))) = {0};
#else
uint8_t *slave_buffer_rx = NULL;
#endif /* end of (CONFIG_SPI_RX_DMA == 1) & (CONFIG_NOCACHE_MEMORY == 1) */

#endif /* end of MASTER_MODE */

#define MASTER_STACK_SIZE 4096
#define SLAVE_STACK_SIZE 4096

K_THREAD_STACK_DEFINE(master_stack_area, MASTER_STACK_SIZE);
K_THREAD_STACK_DEFINE(slave_stack_area, SLAVE_STACK_SIZE);

struct k_thread master_thread_data;
struct k_thread slave_thread_data;

const struct device *spi_master = NULL;
const struct device *spi_slave = NULL;

#if MASTER_MODE
int spi_master_init(void)
{
	spi_master = DEVICE_DT_GET(DT_NODELABEL(spi1));
	if (spi_master == NULL) {
		printk("spi_master: Device is not found.\n");
		return -ENODEV;
	}

	if (!device_is_ready(spi_master)) {
		printk("spi_master: Device is not ready.\n");
		return -ENODEV;
	}

	return 0;
}
#else
int spi_slave_init(void)
{
	spi_slave = DEVICE_DT_GET(DT_NODELABEL(spi1));
	if (spi_slave == NULL) {
		printk("spi_slave: Device is not found.\n");
		return -ENODEV;
	}

	if (!device_is_ready(spi_slave)) {
		printk("spi_slave: Device is not ready.\n");
		return -ENODEV;
	}

	return 0;
}
#endif

#if MASTER_MODE
void master_thread(void *v1, void *v2, void *v3)
{
	struct spi_config spi_cfg = { 0 };
	uint8_t *rx_buff;

	rx_buff = master_buffer_rx;
	if (rx_buff == NULL) {
#if CONFIG_SPI_RX_DMA
		rx_buff = k_aligned_alloc(32, BUF_SIZE);
		printk("master rx buffer use k_aligned_alloc.\n");
#else
		rx_buff = k_malloc(BUF_SIZE);
		printk("master rx buffer use k_malloc.\n");
#endif
	} else {
		printk("master rx buffer use static allocation.\n");
	}

	if (rx_buff == NULL) {
		printk("rx_buff: memory alloc failed\n");
		return;
	}

	const struct spi_buf tx_bufs[] = {
		{
			.buf = master_buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = rx_buff,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = { .buffers = tx_bufs, .count = ARRAY_SIZE(tx_bufs) };
	const struct spi_buf_set rx = { .buffers = rx_bufs, .count = ARRAY_SIZE(rx_bufs) };

	/* spi master 8bit, LSB first*/
	spi_cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_LSB;
	spi_cfg.frequency = 10 * 1000000UL;

	while (1) {
		k_msleep(1000);
		memset(rx_buff, 0, BUF_SIZE);
		spi_transceive(spi_master, &spi_cfg, &tx, &rx);
		printk("[Master]receive:");
		for (uint8_t i = 0; i < BUF_SIZE; i++) {
			printk("0x%02x ", rx_buff[i]);
		}
		printk("\n");
	}
}
#else
void slave_thread(void *v1, void *v2, void *v3)
{
	struct spi_config spi_cfg = { 0 };
	uint8_t *rx_buff;

	rx_buff = slave_buffer_rx;
	if (rx_buff == NULL) {
#if CONFIG_SPI_RX_DMA
		rx_buff = k_aligned_alloc(32, BUF_SIZE);
		printk("slave rx buffer use k_aligned_alloc.\n");
#else
		rx_buff = k_malloc(BUF_SIZE);
		printk("slave rx buffer use k_malloc.\n");
#endif
	} else {
		printk("slave rx buffer use static allocation.\n");
	}

	if (rx_buff == NULL) {
		printk("rx_buff: memory alloc failed\n");
		return;
	}

	const struct spi_buf tx_bufs[] = {
		{
			.buf = slave_buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = rx_buff,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = { .buffers = tx_bufs, .count = ARRAY_SIZE(tx_bufs) };
	const struct spi_buf_set rx = { .buffers = rx_bufs, .count = ARRAY_SIZE(rx_bufs) };

	/* spi slave 8bit, LSB first*/
	spi_cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_SLAVE | SPI_TRANSFER_LSB;
	spi_cfg.frequency = 10 * 1000000UL;

	while (1) {
		memset(rx_buff, 0, BUF_SIZE);
		spi_transceive(spi_slave, &spi_cfg, &tx, &rx);

		printk("[Slave]receive:");
		for (uint8_t i = 0; i < BUF_SIZE; i++) {
			printk("0x%02x ", rx_buff[i]);
		}
		printk("\n");
	}
}
#endif

void main(void)
{
	int ret = 0;

	printk("SPI test start \n");

#if MASTER_MODE
	ret = spi_master_init();
#else
	ret = spi_slave_init();
#endif
	if(ret != 0){
		return;
	}

	/* create master or slave thread */
	int pri = k_thread_priority_get(k_current_get());

#if MASTER_MODE
	k_thread_create(&master_thread_data, master_stack_area,
			K_THREAD_STACK_SIZEOF(master_stack_area), master_thread, NULL, NULL, NULL,
			pri, 0, K_NO_WAIT);
#else
	k_thread_create(&slave_thread_data, slave_stack_area,
			K_THREAD_STACK_SIZEOF(slave_stack_area), slave_thread, NULL, NULL, NULL,
			pri, 0, K_NO_WAIT);
#endif
	printk("SPI thread created\n");

	while (1) {
		k_msleep(10);
	}
}
