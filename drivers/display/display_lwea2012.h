/*
 * Copyright (c) 2023 LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_DRIVERS_DISPLAY_DISPLAY_LWEA2012_H_
#define ZEPHYR_DRIVERS_DISPLAY_DISPLAY_LWEA2012_H_

#define LWEA2012_SOFTRESET 0x01
#define LWEA2012_ID	   0x04
#define LWEA2012_RDISPMODE 0x09
#define LWEA2012_RDDPM	   0x0A
#define LWEA2012_RDDMADCTL 0x0B
#define LWEA2012_RDPIX	   0x0C
#define LWEA2012_RDDIM	   0x0D
#define LWEA2012_RDDSM	   0x0E
#define LWEA2012_RDDSDR	   0x0F
#define LWEA2012_SLPIN	   0x10
#define LWEA2012_SLPOUT	   0x11
#define LWEA2012_NORON	   0x13
#define LWEA2012_INVOFF	   0x20
#define LWEA2012_INVON	   0x21
#define LWEA2012_DISPOFF   0x28
#define LWEA2012_DISPON	   0x29
#define LWEA2012_SETCOL	   0x2A
#define LWEA2012_SETPAGE   0x2B
#define LWEA2012_WRMEMST   0x2C
#define LWEA2012_TEOFF	   0x34
#define LWEA2012_TEON	   0x35
#define LWEA2012_MADCTR	   0x36
#define LWEA2012_IDMOFF	   0x38
#define LWEA2012_IDMON	   0x39
#define LWEA2012_SETPIXEL  0x3A
#define LWEA2012_WRMEMCONT 0x3C
#define LWEA2012_TESS	   0x44
#define LWEA2012_RSS	   0x45
#define LWEA2012_SETBR	   0x51
#define LWEA2012_SETMODE   0x53
#define LWEA2012_RDMODE	   0x54
#define LWEA2012_SETPWR	   0x55
#define LWEA2012_RDPWR	   0x56
#define LWEA2012_SETMINBR  0x5E
#define LWEA2012_RDMINBR   0x5F
#define LWEA2012_RDDDBST   0xA1
#define LWEA2012_RDDDBCON  0xA8

/** Command/data GPIO level for commands. */
#define LWEA2012_CMD 1U

/** Command/data GPIO level for data. */
#define LWEA2012_DATA 0U

#define LWEA2012_PIXEL_SIZE 3
struct lwea2012_config {
	struct spi_dt_spec bus;
	struct gpio_dt_spec cmd_data;
	struct gpio_dt_spec reset;
	struct gpio_dt_spec te;
	uint8_t pixel_format;
	uint16_t height;
	uint16_t width;
	uint8_t madctl;
	uint8_t colmod;
	uint8_t caset[4];
	uint8_t raset[4];
	uint8_t vmctr1;
	uint8_t invctr;
	uint8_t pwctr1[3];
	uint8_t pwctr2[1];
	uint8_t pwctr3[2];
	uint8_t pwctr4[2];
	uint8_t pwctr5[2];
	uint8_t frmctr1[3];
	uint8_t frmctr2[3];
	uint8_t frmctr3[6];
	uint8_t gamctrp1[16];
	uint8_t gamctrn1[16];
	bool inversion_on;
};

struct lwea2012_data {
	struct gpio_callback te_callback;
	struct k_sem te_sem;
	uint16_t x_offset;
	uint16_t y_offset;
	uint32_t pixel_format;
	uint32_t orientation;
};

#endif
