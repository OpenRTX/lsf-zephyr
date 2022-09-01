/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>

#include <drivers/video.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#define VIDEO_DEV DT_LABEL(DT_NODELABEL(dvp))
#define VBUF_NUM 2
#define IMAGE_HEIGHT  240
#define IMAGE_WIDTH   320
#define IMAGE_SIZE	  (IMAGE_WIDTH * IMAGE_HEIGHT * 2)

#define USE_SERIAL_PROTOCOL   (0)

const struct device *video;
uint8_t image_buf[IMAGE_SIZE];

extern int frame_send(uint8_t *frameBuf, uint16_t width, uint16_t height, uint8_t mode);

int calculate_fps(struct video_buffer *vbuf)
{
	static uint32_t last_timestamp = 0;
	int fps;

	fps = vbuf->timestamp - last_timestamp ?
					      1000 / (vbuf->timestamp - last_timestamp) :
					      0;

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
            "fallback to software generator.", VIDEO_DEV);

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
		       (char)fcap->pixelformat,
		       (char)(fcap->pixelformat >> 8),
		       (char)(fcap->pixelformat >> 16),
		       (char)(fcap->pixelformat >> 24),
		       fcap->width_min, fcap->width_max, fcap->width_step,
		       fcap->height_min, fcap->height_max, fcap->height_step);
		i++;
	}

	fmt.pixelformat = VIDEO_PIX_FMT_VYUY;
	fmt.width = IMAGE_WIDTH;
	fmt.height = IMAGE_HEIGHT;
	fmt.pitch = fmt.width * 2;
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

	printk("Capture started\n");

	/* Grab video frames */
	while (1) {
		int err;
		err = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		if (err) {
			LOG_ERR("Unable to dequeue video buf");
			return;
		}

		//必需有刷新操作，除非保证video_buffer_alloc()申请的内存是nocache_region
		SCB_InvalidateDCache_by_Addr((void *)vbuf->buffer, vbuf->bytesused);
		
#if USE_SERIAL_PROTOCOL
		memcpy(image_buf, vbuf->buffer, IMAGE_SIZE);
		frame_send(image_buf, fmt.width, fmt.height, 0);
#endif

		LOG_INF("\rGot frame %u! size: %u; timestamp %u ms fps %d\n",
		       frame++, vbuf->bytesused, vbuf->timestamp, calculate_fps(vbuf));

		err = video_enqueue(video, VIDEO_EP_OUT, vbuf);
		if (err) {
			LOG_ERR("Unable to requeue video buf");
			return;
		}

	}
}
