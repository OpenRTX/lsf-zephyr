/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/spi.h>
#include <string.h>

#define BUF_SIZE 9
uint8_t master_buffer_tx[BUF_SIZE] = {1,2,3,4,5,6,7,8,9};
uint8_t master_buffer_rx[BUF_SIZE] = {};

uint8_t slave_buffer_tx[BUF_SIZE] = {9,8,7,6,5,4,3,2,1};
uint8_t slave_buffer_rx[BUF_SIZE] = {};


#define MASTER_STACK_SIZE 4096
#define SLAVE_STACK_SIZE 4096

K_THREAD_STACK_DEFINE(master_stack_area, MASTER_STACK_SIZE);
K_THREAD_STACK_DEFINE(slave_stack_area, SLAVE_STACK_SIZE);

struct k_thread master_thread_data;
struct k_thread slave_thread_data;

const struct device *spi_master = NULL;
const struct device *spi_slave = NULL;

#if DT_NODE_HAS_STATUS(DT_ALIAS(spi_master), okay)
#define SPIMASTER_DEV_NAME DT_LABEL(DT_ALIAS(spi_master))
#else
#error "can't find SPI master"
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(spi_slave), okay)
#define SPISLAVE_DEV_NAME DT_LABEL(DT_ALIAS(spi_slave))
#else
#error "can't find SPI slave"
#endif



void spi_master_init(void)
{
	spi_master = device_get_binding(SPIMASTER_DEV_NAME);
	//spi_master = DEVICE_DT_GET(DT_NODELABEL(spi0));
	if (spi_master == NULL) {
		printk("spi_master: Device is not found.\n");
		return;
	}

	if (!device_is_ready(spi_master)) {
		printk("spi_master: Device is not ready.\n");
		return;
	}

}

void spi_slave_init(void)
{
	spi_slave = device_get_binding(SPISLAVE_DEV_NAME);
	if (spi_slave == NULL) {
		printk("spi_slave: Device is not found.\n");
		return;
	}

	if (!device_is_ready(spi_slave)) {
		printk("spi_slave: Device is not ready.\n");
		return;
	}
}


void master_thread(void *v1, void *v2, void *v3)
{
	struct spi_config spi_cfg = {0};
	const struct spi_buf tx_bufs[] = {
		{
			.buf = master_buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = master_buffer_rx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};

    /* spi master 8bit, LSB first*/
	spi_cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_LSB;
	spi_cfg.frequency = 10 * 1000000UL;

	while (1) {
		printk("spi master sending data ...\n");

		k_msleep(2000);

		spi_transceive(spi_master, &spi_cfg, &tx, &rx);
		printk("[Master]receive data ...\n");
		for(uint8_t i = 0; i < BUF_SIZE; i++)
		{
			printk("0x%x", master_buffer_rx[i]);
			master_buffer_rx[i] = 0xAA;
		}
		printk("\n");

	}
}

void slave_thread(void *v1, void *v2, void *v3)
{
	struct spi_config spi_cfg = {0};
	const struct spi_buf tx_bufs[] = {
		{
			.buf = slave_buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = slave_buffer_rx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};

    /* spi slave 8bit, LSB first*/
	spi_cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_SLAVE | SPI_TRANSFER_LSB;
	spi_cfg.frequency = 10 * 1000000UL;

	while (1) {
		
		spi_transceive(spi_slave, &spi_cfg, &tx, &rx);

		printk("[Slave]receive data ...\n");
		for(uint8_t i = 0; i < BUF_SIZE; i++)
		{
			printk("0x%x", slave_buffer_rx[i]);
			slave_buffer_rx[i] = 0x55;
		}
		printk("\n");

	}
}


void main(void)
{
	printk("SPI MASTER_SLAVE TEST\n");

	spi_master_init();
	spi_slave_init();

	/* create master and slave thread */
	int pri = k_thread_priority_get(k_current_get());

	k_thread_create(&slave_thread_data, slave_stack_area,
					K_THREAD_STACK_SIZEOF(slave_stack_area),
					slave_thread, NULL, NULL, NULL, pri, 0, K_NO_WAIT);

	k_thread_create(&master_thread_data, master_stack_area,
					K_THREAD_STACK_SIZEOF(master_stack_area), master_thread, NULL,
					NULL, NULL, pri, 0, K_NO_WAIT);

	printk("SPI thread creaeted\n");

	while (1) {
		k_msleep(10);
	}
}
