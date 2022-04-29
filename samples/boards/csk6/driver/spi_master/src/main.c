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

#define UINT8_MAX_NUM	(0xff)

void main(void)
{
    printk("spi master example\n");
    const struct device *spi;
	spi = DEVICE_DT_GET(DT_NODELABEL(spi0));
	if (!device_is_ready(spi)) {
		printk("SPI device %s is not ready\n", spi->name);
		return;
	}

    struct spi_config spi_cfg = {0};
    /* spi master 8bit, LSB first*/
	spi_cfg.operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_LSB;
	spi_cfg.frequency = 10 * 1000000UL;

#ifdef CONFIG_SPI_SOFT_GPIO_CS_ENABLE
    const struct spi_cs_control spi_cs_ctrl = {
        .gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
        .gpio_pin = 20,
        .gpio_dt_flags = GPIO_ACTIVE_LOW, /* Invert gpio active level logic */
        .delay = 5                       /* Delay 5us microseconds to wait before starting the */
                                        /* transmission and before releasing the CS line */
    };
    spi_cfg.cs = &spi_cs_ctrl;
#endif

    /* Make spi transaction package buffers */
	const int tx_package_num = 10;
	struct spi_buf *tx_package = k_calloc(tx_package_num, sizeof(struct spi_buf));
	if (tx_package == NULL) {
		printk("tx_package calloc failed\n");
		return;
	}
	struct spi_buf_set tx_set;

	for (int tx_count = 0; tx_count < tx_package_num; tx_count++) {
		uint8_t *temp_buf = k_calloc(UINT8_MAX_NUM, sizeof(uint8_t));
		if (temp_buf == NULL) {
			printk("temp_buf calloc failed\n");
			return;
		}

		for (int num = 0; num < UINT8_MAX_NUM; num++) {
			if (tx_count % 2) {
				temp_buf[num] = UINT8_MAX_NUM - num;
			} else {
				temp_buf[num] = num;
			}
		}
		tx_package[tx_count].len = UINT8_MAX_NUM;
		tx_package[tx_count].buf = temp_buf;
	}
	tx_set.buffers = tx_package;
	tx_set.count = tx_package_num;

	while (1) {
		printk("spi master sending data ...\n");
		spi_write(spi, &spi_cfg, &tx_set);
		k_msleep(1000);
	}
}
