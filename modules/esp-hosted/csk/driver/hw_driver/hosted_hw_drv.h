#pragma once

int hosted_spi_init(void);
int hosted_spi_send_recv(void *tx_data, void *rx_data, uint32_t max_buffer_len,
			 k_timeout_t timeout);

void hosted_gpio_init(void);
int hosted_gpio_read_handshake(void);
int hosted_gpio_read_dataready(void);
void hosted_gpio_write_reset(int value);
bool hosted_gpio_is_handshake(int pin);
bool hosted_gpio_is_dataready(int pin);
