/*
 * Copyright (c) 2019 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * Add kscan demo:
 * Copyright (c) 2022 Listenai
 * 
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);

#include <zephyr.h>
#include <device.h>
#include <drivers/display.h>
#include <drivers/kscan.h>

#if DT_NODE_HAS_STATUS(DT_INST(0, ilitek_ili9341), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, ilitek_ili9341))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, ilitek_ili9340), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, ilitek_ili9340))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd1306fb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd1306fb))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd16xxfb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd16xxfb))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, sitronix_st7789v), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, sitronix_st7789v))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, sitronix_st7735r), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, sitronix_st7735r))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, fsl_imx6sx_lcdif), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, fsl_imx6sx_lcdif))
#endif

#if DT_NODE_HAS_STATUS(DT_INST(0, betterlife_bl6xxx), okay)
#define DISPLAY_KSAN_DEV_NAME DT_LABEL(DT_INST(0, betterlife_bl6xxx))
#endif

#ifdef CONFIG_SDL_DISPLAY_DEV_NAME
#define DISPLAY_DEV_NAME CONFIG_SDL_DISPLAY_DEV_NAME
#endif

#ifdef CONFIG_DUMMY_DISPLAY_DEV_NAME
#define DISPLAY_DEV_NAME CONFIG_DUMMY_DISPLAY_DEV_NAME
#endif

#ifdef CONFIG_ARCH_POSIX
#include "posix_board_if.h"
#endif

#ifdef CONFIG_ARCH_POSIX
#define RETURN_FROM_MAIN(exit_code) posix_exit_main(exit_code)
#else
#define RETURN_FROM_MAIN(exit_code) return
#endif

enum corner { TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT };

typedef void (*fill_buffer)(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size);

#ifdef CONFIG_ARCH_POSIX
static void posix_exit_main(int exit_code)
{
#if CONFIG_TEST
	if (exit_code == 0) {
		LOG_INF("PROJECT EXECUTION SUCCESSFUL");
	} else {
		LOG_INF("PROJECT EXECUTION FAILED");
	}
#endif
	posix_exit(exit_code);
}
#endif

static void k_callback(const struct device *dev, uint32_t row, uint32_t col, bool pressed)
{
	ARG_UNUSED(dev);
	printk("row = %u col = %u, pressed:%s\n", row, col, pressed ? "TRUE" : "FLASE");
}

void kscan_init(void)
{
#if DT_NODE_HAS_STATUS(DT_INST(0, betterlife_bl6xxx), okay)
	const struct device *kscan_dev = device_get_binding(DISPLAY_KSAN_DEV_NAME);

	if (!device_is_ready(kscan_dev)) {
		LOG_ERR("kscan device %s not ready", kscan_dev->name);
		return;
	}

	kscan_config(kscan_dev, k_callback);
	kscan_enable_callback(kscan_dev);
#endif
}

static void fill_buffer_argb8888(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
	uint32_t color = 0;

	switch (corner) {
	case TOP_LEFT:
		color = 0x00FF0000u;
		break;
	case TOP_RIGHT:
		color = 0x0000FF00u;
		break;
	case BOTTOM_RIGHT:
		color = 0x000000FFu;
		break;
	case BOTTOM_LEFT:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 4) {
		*((uint32_t *)(buf + idx)) = color;
	}
}

static void fill_buffer_rgb888(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
	uint32_t color = 0;

	switch (corner) {
	case TOP_LEFT:
		color = 0x00FF0000u;
		break;
	case TOP_RIGHT:
		color = 0x0000FF00u;
		break;
	case BOTTOM_RIGHT:
		color = 0x000000FFu;
		break;
	case BOTTOM_LEFT:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 3) {
		*(buf + idx + 0) = color >> 16;
		*(buf + idx + 1) = color >> 8;
		*(buf + idx + 2) = color >> 0;
	}
}

static void fill_color(uint32_t color, uint8_t *buf, size_t buf_size)
{
	for (size_t idx = 0; idx < buf_size; idx += 3) {
		*(buf + idx + 0) = color >> 16;
		*(buf + idx + 1) = color >> 8;
		*(buf + idx + 2) = color >> 0;
	}
}

static uint16_t get_rgb565_color(enum corner corner, uint8_t grey)
{
	uint16_t color = 0;
	uint16_t grey_5bit;

	switch (corner) {
	case TOP_LEFT:
		color = 0xF800u;
		break;
	case TOP_RIGHT:
		color = 0x07E0u;
		break;
	case BOTTOM_RIGHT:
		color = 0x001Fu;
		break;
	case BOTTOM_LEFT:
		grey_5bit = grey & 0x1Fu;
		/* shift the green an extra bit, it has 6 bits */
		color = grey_5bit << 11 | grey_5bit << (5 + 1) | grey_5bit;
		break;
	}
	return color;
}

static void fill_buffer_rgb565(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
	uint16_t color = get_rgb565_color(corner, grey);

	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(buf + idx + 0) = (color >> 8) & 0xFFu;
		*(buf + idx + 1) = (color >> 0) & 0xFFu;
	}
}

static void fill_buffer_bgr565(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
	uint16_t color = get_rgb565_color(corner, grey);

	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(uint16_t *)(buf + idx) = color;
	}
}

static void fill_buffer_mono(enum corner corner, uint8_t grey, uint8_t *buf, size_t buf_size)
{
	uint16_t color;

	switch (corner) {
	case BOTTOM_LEFT:
		color = (grey & 0x01u) ? 0xFFu : 0x00u;
		break;
	default:
		color = 0;
		break;
	}

	memset(buf, color, buf_size);
}

#ifndef CONFIG_SOC_LS_ADSP_CSK6
#define CP_BOOT_ADDRESS 0x60000000
#define SYSCTRL_RESET_CP (0xCAFE000B)
#define SYSCTRL_RELEASE_CP (0xCAFE100B)
#define SYSCTRL_RESET_NPU (0xCAFE000C)
#define CP_PSRAM_BASE (0x68400000)
#define AP_PSRAM_BASE (0x30000000)

#define CP_FW_ADDR (0x18000000 + 0x00200000)
#define CP_FW_LEN 500000

#include "venus_ap.h"
#include "cmn_sysctrl_reg_venus.h"
#include "PSRAMManager.h"
#include <cache.h>

void prepare_cp_run()
{
	PSRAM_Initialize();

	SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;

	// set boot address
	cmn_sys_ctrl->REG_CP_CTRL2.all = CP_BOOT_ADDRESS;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 0x1;

	// stall HIFI4
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;

	// release CP & NPU
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	if (cmn_sys_ctrl->REG_SW_RESET_NPU.all) { // = 1 NPU reset
		cmn_sys_ctrl->REG_SW_RESET_NPU.all = SYSCTRL_RESET_NPU;
	}

	__DMB();
	memcpy((void *)AP_PSRAM_BASE, (void *)CP_FW_ADDR, CP_FW_LEN);

	sys_cache_data_range((void *)AP_PSRAM_BASE, CP_FW_LEN, K_CACHE_WB);
	__DSB();

	// run cp
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

	printk("Set CP boot address:%x.\n", CP_BOOT_ADDRESS);
	printk("Boot CP...\n");
}

#endif

void main(void)
{
	size_t x;
	size_t y;
	size_t rect_w;
	size_t rect_h;
	size_t h_step;
	size_t scale;
	size_t grey_count;
	uint8_t *buf;
	int32_t grey_scale_sleep;
	const struct device *display_dev;
	struct display_capabilities capabilities;
	struct display_buffer_descriptor buf_desc;
	size_t buf_size = 0;
	fill_buffer fill_buffer_fnc = NULL;

#ifndef CONFIG_SOC_LS_ADSP_CSK6
	prepare_cp_run();
	while (1) {
		LOG_INF("Display sample for %s", DISPLAY_DEV_NAME);
		k_msleep(500);
	}
#endif

	display_dev = device_get_binding(DISPLAY_DEV_NAME);

	if (display_dev == NULL) {
		printk("Device %s not found. Aborting sample.", DISPLAY_DEV_NAME);
		RETURN_FROM_MAIN(1);
	}

	display_get_capabilities(display_dev, &capabilities);

	if (capabilities.screen_info & SCREEN_INFO_MONO_VTILED) {
		rect_w = 16;
		rect_h = 8;
	} else {
		rect_w = 2;
		rect_h = 1;
	}

	h_step = rect_h;
	scale = (capabilities.x_resolution / 8) / rect_h;

	rect_w *= scale;
	rect_h *= scale;

	if (capabilities.screen_info & SCREEN_INFO_EPD) {
		grey_scale_sleep = 10000;
	} else {
		grey_scale_sleep = 100;
	}

	buf_size = rect_w * rect_h;

	if (buf_size < (capabilities.x_resolution * h_step)) {
		buf_size = capabilities.x_resolution * h_step;
	}

	switch (capabilities.current_pixel_format) {
	case PIXEL_FORMAT_ARGB_8888:
		fill_buffer_fnc = fill_buffer_argb8888;
		buf_size *= 4;
		break;
	case PIXEL_FORMAT_RGB_888:
		fill_buffer_fnc = fill_buffer_rgb888;
		buf_size *= 3;
		break;
	case PIXEL_FORMAT_RGB_565:
		fill_buffer_fnc = fill_buffer_rgb565;
		buf_size *= 2;
		break;
	case PIXEL_FORMAT_BGR_565:
		fill_buffer_fnc = fill_buffer_bgr565;
		buf_size *= 2;
		break;
	case PIXEL_FORMAT_MONO01:
	case PIXEL_FORMAT_MONO10:
		fill_buffer_fnc = fill_buffer_mono;
		buf_size /= 8;
		break;
	default:
		printk("Unsupported pixel format. Aborting sample.");
		RETURN_FROM_MAIN(1);
	}

	buf_size = 240 * 320 * 3;
	printk("buf_size:%lu\n", buf_size);
	buf = k_malloc(buf_size);

	if (buf == NULL) {
		printk("Could not allocate memory. Aborting sample.");
		RETURN_FROM_MAIN(1);
	}

	buf_desc.buf_size = buf_size;
	buf_desc.pitch = capabilities.x_resolution;
	buf_desc.width = capabilities.x_resolution;
	buf_desc.height = 320;
	display_blanking_off(display_dev);

	uint32_t i;
	for (i = 0; i < buf_size; i++) {
		buf[i] = i;
	}
	i = 0;
	while (1) {
		uint32_t tick = k_uptime_get_32();
		fill_color(0xff << (8 * i), buf, buf_size);
		display_write(display_dev, 0, 0, &buf_desc, buf);
		i++;
		i %= 3;
		printk("FPS:%d\n", 1000 / (k_uptime_get_32() - tick));
	}

	RETURN_FROM_MAIN(0);
}
