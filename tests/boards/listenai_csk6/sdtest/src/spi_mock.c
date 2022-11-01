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

#define TX_BUF_LEN	(4096)

// #define LOG_LEVEL 4
#include <logging/log.h>
LOG_MODULE_REGISTER(spi_mock);

K_THREAD_STACK_DEFINE(thread_stack, 2048);
struct k_thread spi_mock_thread;

const struct device *spi;

void spi_mock_thread_func(void *dummy1, void *dummy2, void *dummy3)
{
    (void)dummy1;
    (void)dummy2;
    (void)dummy3;

    struct spi_config spi_cfg = {0};
    /* spi master 8bit, LSB first*/
	spi_cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_LSB;
	spi_cfg.frequency = 25 * 1000000UL;

    /* Make spi transaction package buffers */
	const int tx_package_num = 10;
	struct spi_buf *tx_package = k_calloc(tx_package_num, sizeof(struct spi_buf));
	if (tx_package == NULL) {
		LOG_ERR("tx_package calloc failed\n");
		return;
	}
	struct spi_buf_set tx_set;

	for (int tx_count = 0; tx_count < tx_package_num; tx_count++) {
		uint8_t *temp_buf = k_calloc(TX_BUF_LEN, sizeof(uint8_t));
		if (temp_buf == NULL) {
			LOG_ERR("temp_buf calloc failed\n");
			return;
		}

		tx_package[tx_count].len = TX_BUF_LEN;
		tx_package[tx_count].buf = temp_buf;
	}
	tx_set.buffers = tx_package;
	tx_set.count = tx_package_num;

	while (1) {
		LOG_INF("spi master sending data ...\n");
		spi_write(spi, &spi_cfg, &tx_set);
	}
}

void spi_mock(void)
{
	spi = DEVICE_DT_GET(DT_NODELABEL(spi1));
	if (!device_is_ready(spi)) {
		LOG_ERR("SPI device %s is not ready\n", spi->name);
		return;
	}

    k_thread_create(&spi_mock_thread, thread_stack,
                    K_THREAD_STACK_SIZEOF(thread_stack),
                    spi_mock_thread_func, NULL, NULL, NULL,
                    K_HIGHEST_APPLICATION_THREAD_PRIO, K_USER,
                    K_NO_WAIT);

}
