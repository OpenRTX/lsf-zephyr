/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>

#include <device.h>
#include <drivers/uart.h>
#include <zephyr.h>

#define UART0		 DT_NODELABEL(uart1)
#define MAX_TX_FRAME_LEN (32 * 1024)
#define UART_TX_BLOCK	 (MAX_TX_FRAME_LEN >> 1)

const struct device *uart_dev;
uint8_t txBuf[UART_TX_BLOCK + 1]; // append 1 byte checksum
uint8_t txFrame[MAX_TX_FRAME_LEN +
		4]; // tx frame buffer + header & ender & checksum w/ or w/o encoded bytes

uint32_t rfc1662Encode(uint8_t *buf, uint32_t bufLen, uint8_t *pData, uint32_t dataLen);

static uint8_t calc_checksum(uint8_t *pData, int len)
{
	uint8_t checksum = 0;
	for (int i = 0; i < len; i++) {
		checksum += pData[i];
	}
	return checksum;
}

int serial_transmit(uint8_t *buf, int len)
{
	static uint8_t first = 1;
	if (first) {
		uart_dev = DEVICE_DT_GET(UART0);
		if (!device_is_ready(uart_dev)) {
			printk("device:%s is not ready!", uart_dev->name);
			return -1;
		}
		first = 0;
	}

	if (uart_dev) {
		for (uint32_t i = 0; i < len; i++) {
			uart_poll_out(uart_dev, buf[i]);
		}
	}
	return 0;
}

int frame_send(uint8_t *frameBuf, uint16_t width, uint16_t height, uint8_t mode)
{
	int txLen;

	txLen = width * height * 2;

	int txPos = 0;
	while (txLen > UART_TX_BLOCK) {
		memcpy(txBuf, &frameBuf[txPos], UART_TX_BLOCK);
		txBuf[UART_TX_BLOCK] = calc_checksum(txBuf, UART_TX_BLOCK);
		uint32_t encLen = rfc1662Encode(txFrame, sizeof(txFrame), (uint8_t *)txBuf,
						UART_TX_BLOCK + 1);
		if (encLen == 0) {
			while (1)
				;
		}
		serial_transmit(txFrame, encLen);

		txPos += UART_TX_BLOCK;
		txLen -= UART_TX_BLOCK;
	}
	if (txLen > 0) {
		memcpy(txBuf, &frameBuf[txPos], txLen);
		txBuf[txLen] = calc_checksum(txBuf, txLen);
		uint32_t encLen =
			rfc1662Encode(txFrame, sizeof(txFrame), (uint8_t *)&txBuf, txLen + 1);
		if (encLen == 0) {
			while (1)
				;
		}
		serial_transmit(txFrame, encLen);
	}
	txFrame[0] = 0x7E;
	txFrame[1] = 0x00;
	txFrame[2] = 0x7E;
	// indication of frame dimension
	serial_transmit(txFrame, 3);

	txBuf[0] = width % 256;
	txBuf[1] = width / 256;
	txBuf[2] = height % 256;
	txBuf[3] = height / 256;
	txBuf[4] = mode;
	txBuf[5] = 0;
	txBuf[6] = calc_checksum(txBuf, 6);
	uint32_t encLen = rfc1662Encode(txFrame, sizeof(txFrame), (uint8_t *)&txBuf, 7);
	// indication of frame dimension
	serial_transmit(txFrame, encLen);

	txFrame[0] = 0x7E;
	txFrame[1] = 0x01;
	txFrame[2] = 0x7E;
	// indication of end of frame
	serial_transmit(txFrame, 3);

	return 0;
}

#define RFC1662_FLAG   (0x7E)
#define RFC1662_ESCAPE (0x7D)

uint32_t rfc1662Encode(uint8_t *buf, uint32_t bufLen, uint8_t *pData, uint32_t dataLen)
{
	uint32_t encLen = 0;

	// Add start flag
	// (bufLen checks at each insert return error on overflow.)
	if (bufLen <= encLen)
		goto error;
	buf[encLen++] = RFC1662_FLAG;

	// Copy pData to buf, escaping flags and escape chars in content
	for (uint32_t n = 0; n < dataLen; n++) {
		// If next byte needs to be escaped...
		if ((pData[n] == RFC1662_FLAG) || (pData[n] == RFC1662_ESCAPE)) {
			// If overflowed, return error
			if (bufLen <= encLen - 1)
				goto error;

			// Add escaped data to buffer
			buf[encLen++] = RFC1662_ESCAPE;
			buf[encLen++] = pData[n] ^ 0x20;
		} else {
			// If overflowed, return error
			if (bufLen <= encLen)
				goto error;

			// Add data to buffer
			buf[encLen++] = pData[n];
		}
	}

	// Add stop flag
	if (bufLen <= encLen)
		goto error;
	buf[encLen++] = RFC1662_FLAG;

	// return the length of data in the buffer
	return encLen;

	// On error, return 0
error:
	return 0;
}
