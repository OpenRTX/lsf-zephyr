/*
 * Copyright (c) 2023 Anhui(Shenzhen) Listenai Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <device.h>
#include <drivers/video.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#define VIDEO_DEV		DT_LABEL(DT_NODELABEL(dvp))
#define VBUF_NUM		2
#define IMAGE_HEIGHT		480
#define IMAGE_WIDTH		640
#define PIXEL_BYTES_SIZE	2
#define IMAGE_SIZE		(IMAGE_WIDTH * IMAGE_HEIGHT * PIXEL_BYTES_SIZE)

#define USE_SERIAL_PROTOCOL (1)

#define WEBUSB_IMAGE_DOWNSAMPLING	(5) // 缩小倍数

#ifdef WEBUSB_IMAGE_DOWNSAMPLING

#define WEBUSB_IMAGE_SCALE 	(1.0f / WEBUSB_IMAGE_DOWNSAMPLING)
#define RESAMPLE_HEIGHT		(IMAGE_HEIGHT / WEBUSB_IMAGE_DOWNSAMPLING)
#define RESAMPLE_WIDTH		(IMAGE_HEIGHT / WEBUSB_IMAGE_DOWNSAMPLING)

static uint8_t resample_img[RESAMPLE_HEIGHT * RESAMPLE_WIDTH * 2] __attribute__((section(".psram_section")));

void downsample(unsigned char *input_image, unsigned char *output_image,
				uint32_t img_w, uint32_t img_h, uint8_t downsampling,
				uint8_t alg_type) {
	uint32_t new_w = img_w / downsampling;
	uint32_t new_h = img_h / downsampling;

	if (alg_type == 0) {
		// Nearest neighbor algorithm
		for (uint32_t y = 0; y < new_h; ++y) {
			for (uint32_t x = 0; x < new_w; ++x) {
				uint32_t input_idx = (y * downsampling * img_w + x * downsampling) * 2;
				uint32_t output_idx = (y * new_w + x) * 2;
				output_image[output_idx] = input_image[input_idx];     // V
				output_image[output_idx + 1] = input_image[input_idx + 1]; // Y
				output_image[output_idx + 2] = input_image[input_idx + 2]; // U
				output_image[output_idx + 3] = input_image[input_idx + 3]; // Y
			}
		}
	} else if (alg_type == 1) {
		// Averaging algorithm
		for (uint32_t y = 0; y < new_h; ++y) {
			for (uint32_t x = 0; x < new_w; ++x) {
				uint32_t output_idx = (y * new_w + x) * 2;
				uint32_t sum_v = 0, sum_y1 = 0, sum_u = 0, sum_y2 = 0;

				for (uint8_t dy = 0; dy < downsampling; ++dy) {
					for (uint8_t dx = 0; dx < downsampling; ++dx) {
					uint32_t input_idx = ((y * downsampling + dy) * img_w + x * downsampling + dx) * 2;

					sum_v += input_image[input_idx];
					sum_y1 += input_image[input_idx + 1];
					sum_u += input_image[input_idx + 2];
					sum_y2 += input_image[input_idx + 3];
					}
				}

				output_image[output_idx] = sum_v / (downsampling * downsampling);
				output_image[output_idx + 1] = sum_y1 / (downsampling * downsampling);
				output_image[output_idx + 2] = sum_u / (downsampling * downsampling);
				output_image[output_idx + 3] = sum_y2 / (downsampling * downsampling);
			}
		}
	} else if (alg_type == 2) {

		// 2 bytes per pixel.
		int pos = 0;
		// Process every other line.
		for (int pixelY = 0; pixelY < img_h; pixelY += downsampling) { 
			// Work in blocks of 2 pixels, we discard the second.
			for (int pixelX = 0; pixelX < img_w; pixelX += 2 * downsampling) {
				// Position of pixel bytes.
				int start = ((pixelY * img_w) + pixelX) * 2;

				output_image[pos] = input_image[start];
				output_image[pos + 1] = input_image[start + 1];
				output_image[pos + 2] = input_image[start + 2];
				output_image[pos + 3] = input_image[start + 3];

				pos += 4;
			}
		}
	}
}

#endif

const struct device *video;

extern int frame_send(uint8_t *frameBuf, uint16_t width, uint16_t height, uint8_t mode);

int calculate_fps(struct video_buffer *vbuf)
{
	static uint32_t last_timestamp = 0;
	int fps;

	fps = vbuf->timestamp - last_timestamp ? 1000 / (vbuf->timestamp - last_timestamp) : 0;

	last_timestamp = vbuf->timestamp;

	return fps;
}

void main(void)
{
	struct video_buffer *buffers[VBUF_NUM], *vbuf;
	struct video_format fmt;
	struct video_caps caps;
	unsigned int frame = 0;
	size_t bsize;
	int i = 0;

	video = device_get_binding(VIDEO_DEV);

	if (video == NULL) {
		LOG_ERR("Video device %s not found, "
			"fallback to software generator.",
			VIDEO_DEV);

		return;
	}

	printk("- Device name: %s\n", VIDEO_DEV);

	// /* Get capabilities */
	if (video_get_caps(video, VIDEO_EP_OUT, &caps)) {
		LOG_ERR("Unable to retrieve video capabilities");
		return;
	}

	printk("- Capabilities:\n");
	while (caps.format_caps[i].pixelformat) {
		const struct video_format_cap *fcap = &caps.format_caps[i];
		/* fourcc to string */
		printk("  %c%c%c%c width [%u; %u; %u] height [%u; %u; %u]\n",
		       (char)fcap->pixelformat, (char)(fcap->pixelformat >> 8),
		       (char)(fcap->pixelformat >> 16), (char)(fcap->pixelformat >> 24),
		       fcap->width_min, fcap->width_max, fcap->width_step, fcap->height_min,
		       fcap->height_max, fcap->height_step);
		i++;
	}

	fmt.pixelformat = VIDEO_PIX_FMT_VYUY;
	fmt.width = IMAGE_WIDTH;
	fmt.height = IMAGE_HEIGHT;
	fmt.pitch = fmt.width * PIXEL_BYTES_SIZE;
	if (video_set_format(video, VIDEO_EP_OUT, &fmt)) {
		LOG_ERR("Unable to set video format");
		return;
	}

	/* Size to allocate for each buffer */
	bsize = fmt.height * fmt.pitch;

	/* Alloc video buffers and enqueue for capture */
	for (i = 0; i < ARRAY_SIZE(buffers); i++) {
		LOG_INF("Alloc vide buffer: %d\n", bsize);
		buffers[i] = video_buffer_alloc(bsize);
		if (buffers[i] == NULL) {
			LOG_ERR("Unable to alloc video buffer");
			return;
		}

		video_enqueue(video, VIDEO_EP_OUT, buffers[i]);
	}

	/* Start video capture */
	if (video_stream_start(video)) {
		LOG_ERR("Unable to start capture (interface)");
		return;
	}

	/* Grab video frames */
	while (1) {
		int err;
		err = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		if (err) {
			LOG_ERR("Unable to dequeue video buf");
			return;
		}

#if USE_SERIAL_PROTOCOL
	#ifdef WEBUSB_IMAGE_DOWNSAMPLING
			uint32_t dst_width = IMAGE_WIDTH * WEBUSB_IMAGE_SCALE;
			uint32_t dst_height = IMAGE_HEIGHT * WEBUSB_IMAGE_SCALE;
			downsample(vbuf->buffer, resample_img, 
				IMAGE_WIDTH, IMAGE_HEIGHT, WEBUSB_IMAGE_DOWNSAMPLING, 0);
			frame_send(resample_img, dst_width, dst_height, 0);
	#else
			frame_send(vbuf->buffer, fmt.width, fmt.height, 0);
	#endif
#endif

		LOG_INF("Got frame %u! size: %u; timestamp %u ms fps %d", frame++,
			vbuf->bytesused, vbuf->timestamp, calculate_fps(vbuf));

		err = video_enqueue(video, VIDEO_EP_OUT, vbuf);
		if (err) {
			LOG_ERR("Unable to requeue video buf");
			return;
		}
	}
}
