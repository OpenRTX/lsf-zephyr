/*
 * Based on ili9488 sample:
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);

#include <zephyr.h>
#include <device.h>
#include <drivers/display.h>

#if CONFIG_FRAME_MEM_PSRAM
#define K_HEAP_CUSTOM_SECTION_DEFINE(name, bytes) \
    Z_HEAP_DEFINE_IN_SECT(name, bytes, __attribute__((section(".psram_section"))))

K_HEAP_CUSTOM_SECTION_DEFINE(display_buffer_pool, CONFIG_DISPLAY_X_RES * CONFIG_DISPLAY_Y_RES * 3 + 128);
#elif CONFIG_FRAME_MEM_SRAM
#define K_HEAP_CUSTOM_SECTION_DEFINE(name, bytes) K_HEAP_DEFINE(name, bytes)

K_HEAP_CUSTOM_SECTION_DEFINE(display_buffer_pool, CONFIG_DISPLAY_X_RES * CONFIG_DISPLAY_Y_RES * 3 + 128);
#else
	#error "Must select the memory in which image data is stored" 
#endif


enum color {
	COLOR_RED,
	COLOR_GREEN,
	COLOR_BLUE,
	COLOR_CUSTOM
};


typedef void (*fill_buffer)(enum color rgb, uint8_t grey, uint8_t *buf,
			    size_t buf_size);


static void fill_buffer_argb8888(enum color rgb, uint8_t grey, uint8_t *buf,
				 size_t buf_size)
{
	uint32_t color = 0;

	switch (rgb) {
	case COLOR_RED:
		color = 0x00FF0000u;
		break;
	case COLOR_GREEN:
		color = 0x0000FF00u;
		break;
	case COLOR_BLUE:
		color = 0x000000FFu;
		break;
	case COLOR_CUSTOM:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 4) {
		*((uint32_t *)(buf + idx)) = color;
	}
}

static void fill_buffer_rgb888(enum color rgb, uint8_t grey, uint8_t *buf,
			       size_t buf_size)
{
	uint32_t color = 0;

	switch (rgb) {
	case COLOR_RED:
		color = 0x00FF0000u;
		break;
	case COLOR_GREEN:
		color = 0x0000FF00u;
		break;
	case COLOR_BLUE:
		color = 0x000000FFu;
		break;
	case COLOR_CUSTOM:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 3) {
		*(buf + idx + 0) = color >> 16;
		*(buf + idx + 1) = color >> 8;
		*(buf + idx + 2) = color >> 0;
	}
}

static uint16_t get_rgb565_color(enum color rgb, uint8_t grey)
{
	uint16_t color = 0;
	uint16_t grey_5bit;

	switch (rgb) {
	case COLOR_RED:
		color = 0xF800u;
		break;
	case COLOR_GREEN:
		color = 0x07E0u;
		break;
	case COLOR_BLUE:
		color = 0x001Fu;
		break;
	case COLOR_CUSTOM:
		grey_5bit = grey & 0x1Fu;
		/* shift the green an extra bit, it has 6 bits */
		color = grey_5bit << 11 | grey_5bit << (5 + 1) | grey_5bit;
		break;
	}
	return color;
}

static void fill_buffer_rgb565(enum color rgb, uint8_t grey, uint8_t *buf,
			       size_t buf_size)
{
	uint16_t color = get_rgb565_color(rgb, grey);

	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(buf + idx + 0) = (color >> 8) & 0xFFu;
		*(buf + idx + 1) = (color >> 0) & 0xFFu;
	}
}

static void fill_buffer_bgr565(enum color rgb, uint8_t grey, uint8_t *buf,
			       size_t buf_size)
{
	uint16_t color = get_rgb565_color(rgb, grey);

	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(uint16_t *)(buf + idx) = color;
	}
}

int calculate_fps(uint32_t in, uint32_t out)
{
	uint32_t time_us;
	time_us = (out - in) * 33 / 10000;	
	return 1000000 / time_us; 
}

void main(void)
{
	uint8_t *buf;
	const struct device *display_dev;
	struct display_capabilities capabilities;
	struct display_buffer_descriptor buf_desc;
	size_t buf_size = 0;
	uint32_t x_res,y_res,depth;
	fill_buffer fill_buffer_fnc = NULL;

	LOG_INF("Display sample for %s", CONFIG_DISPLAY_DEV_NAME);

	display_dev = device_get_binding(CONFIG_DISPLAY_DEV_NAME);

	if (display_dev == NULL) {
		LOG_ERR("Device %s not found. Aborting sample.",
			CONFIG_DISPLAY_DEV_NAME);
		return;
	}

	display_get_capabilities(display_dev, &capabilities);
	LOG_INF("display capabilities: x_resolution %d  y_resolution %d  format %d\n", 
				capabilities.x_resolution, 
				capabilities.y_resolution, 
				capabilities.current_pixel_format);

	x_res = (CONFIG_DISPLAY_X_RES > capabilities.x_resolution)?(capabilities.x_resolution):(CONFIG_DISPLAY_X_RES);
	y_res = (CONFIG_DISPLAY_Y_RES > capabilities.y_resolution)?(capabilities.y_resolution):(CONFIG_DISPLAY_Y_RES);

	buf_size = x_res * y_res;

	switch (capabilities.current_pixel_format) {
		case PIXEL_FORMAT_ARGB_8888:
			fill_buffer_fnc = fill_buffer_argb8888;
			buf_size *= 4;
			depth = 4;
			break;
		case PIXEL_FORMAT_RGB_888:
			fill_buffer_fnc = fill_buffer_rgb888;
			buf_size *= 3;
			depth = 3;
			break;
		case PIXEL_FORMAT_RGB_565:
			fill_buffer_fnc = fill_buffer_rgb565;
			buf_size *= 2;
			depth = 2; 
			break;
		case PIXEL_FORMAT_BGR_565:
			fill_buffer_fnc = fill_buffer_bgr565;
			buf_size *= 2;
			depth = 2;
			break;
		default:
			LOG_ERR("Unsupported pixel format. Aborting sample.");
			return;
	}

	buf = k_heap_alloc(&display_buffer_pool, buf_size, K_FOREVER);
	if (buf == NULL) {
		LOG_ERR("Could not allocate memory. Aborting sample.");
		return;
	}

	buf_desc.buf_size = buf_size;
	buf_desc.pitch = x_res;
	buf_desc.width = x_res;
	buf_desc.height = y_res;

	display_blanking_off(display_dev);

	uint8_t cnt = 0;
	uint32_t timestamp_in,timestamp_out;
	while (1) {
		if(0 == (cnt % 3)){
			cnt = 0;
		}
		fill_buffer_fnc(cnt++, 0, buf, buf_size);

		timestamp_in = k_cycle_get_32();
		display_write(display_dev, 0, 0, &buf_desc, buf);
		timestamp_out = k_cycle_get_32();
		LOG_INF("display frame: xy (%d %d) depth %d byte, period: %d us, write fps: %d \n", 
				x_res, 
				y_res,
				depth,
				((timestamp_out - timestamp_in) * 33) / 10000, 
				calculate_fps(timestamp_in, timestamp_out));

		k_msleep(1000);
	}
}
