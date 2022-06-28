/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/video.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(main, 4);

extern void s_fs_mount(void);
extern void mcumgr_domain();
extern void sof_domain(void);
extern void network_domain(void);

void main(void)
{
#ifdef CONFIG_SOF_HOST
	s_fs_mount();
#endif

/*Initialization of the SOF must be performed before wifi is enabled，
**because of heap space limitations,*/
#ifdef CONFIG_SOF_HOST
	sof_domain();
#endif

#ifdef CONFIG_WIFI
	network_domain();
#endif

#ifdef CONFIG_MCUMGR
	mcumgr_domain();
#endif

	while (1) {
		k_msleep(1000);
	}
}

typedef struct {
	volatile char *addr;
	volatile uint32_t frame_size;
	volatile uint32_t frame_cnt;
	volatile uint32_t w_pos;
	volatile uint32_t r_pos;
	volatile uint32_t tag;
} __packed frames_info_t;

typedef struct frame_info {
	uint32_t id;
	uint32_t sum;
} __packed frame_info_t;

Z_GENERIC_SECTION(.psram_section)
__attribute__((aligned(32),
	       used)) static char swp_frames[5][sizeof(frame_info_t) + 180 * 128] = { 0 };

static inline char *bf30a2_frames_info_get_free(frames_info_t *info)
{
	if (info == NULL || info->addr == NULL || info->frame_cnt <= 0) {
		return NULL;
	}

	SCB_InvalidateDCache_by_Addr(info, sizeof(frames_info_t));
	int r = info->r_pos;
	int w = info->w_pos;
	int cnt = info->frame_cnt;

	w %= info->frame_cnt;

	if (w > r) {
		if ((r + cnt - w) <= 1) {
			LOG_ERR("No free space to copy datas, w:%d, r=%d.", w, r);
			return NULL;
		}
	} else if (w < r) {
		if (r - w <= 1) {
			LOG_ERR("No free space to copy datas, w:%d, r=%d.", w, r);
			return NULL;
		}
	}

	return (char*)info->addr + w * info->frame_size;
}

static inline int32_t bf30a2_frames_info_update_w_pos(frames_info_t *info)
{
	if (info == NULL || info->addr == NULL || info->frame_cnt <= 0) {
		return -1;
	}

	info->w_pos++;
	info->w_pos %= info->frame_cnt;
	SCB_CleanDCache_by_Addr(info, sizeof(frames_info_t));

	return info->w_pos;
}

#define VIDEO_DEV DT_LABEL(DT_INST(0, byd_bf30a2))

uint32_t calc_sum(uint8_t *src, uint32_t size)
{
	uint32_t sum = 0;

	while (size--) {
		sum += (uint8_t)*src;
		src++;
	}

	return sum;
}

void video_ap2cp_thread(void *p1, void *p2, void *p3)
{
	struct video_buffer *vbuf = NULL;
	frames_info_t *info = (frames_info_t *)0x30704000;

	const struct device *video = device_get_binding(VIDEO_DEV);

	if (!device_is_ready(video)) {
		printk("ipm device is null.\n");
		return;
	}
	memset(info, 0, sizeof(frames_info_t));
	info->addr = &swp_frames[0][0];
	info->frame_size = 180 * 128 + sizeof(frame_info_t);
	info->frame_cnt = 5;
	info->w_pos = 0;
	info->r_pos = 0;
	info->tag = 0x12345678;

	LOG_INF("##########Frames info##########");
	LOG_INF("info       :%p.", (void*)info);
	LOG_INF("buffer addr:%p.", (void*)info->addr);
	LOG_INF("frame  size:%u.", info->frame_size);
	LOG_INF("frame count:%d.", info->frame_cnt);
	LOG_INF("write   pos:%d.", info->w_pos);
	LOG_INF("read    pos:%d.", info->r_pos);

	uint32_t last_tick = 0;
	int id = 0;
	int fps = 0;
	int frame_cnt = 0;
	int discard_cnt = 0;

	while (1) {
		vbuf = NULL;
		video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		frame_cnt++;
		/* send datas to cp core */
		char *buffer = bf30a2_frames_info_get_free(info);
		if (buffer != NULL) {
			frame_info_t *frame_info = (frame_info_t *)buffer;
			frame_info->id = id++;
			memcpy(buffer + sizeof(frame_info_t), vbuf->buffer, 180 * 128);
			frame_info->sum = calc_sum(buffer + sizeof(frame_info_t), 180 * 128);
			SCB_CleanDCache_by_Addr(buffer, info->frame_size);
			bf30a2_frames_info_update_w_pos(info);
			fps = vbuf->timestamp - last_tick > 0 ?
				      1000 / (vbuf->timestamp - last_tick) :
				      0;
			last_tick = vbuf->timestamp;
		} else {
			discard_cnt++;
		}

		if (frame_cnt % 10 == 0) {
			LOG_INF("fps:%d, T:%d, D%d.", fps, frame_cnt, discard_cnt);
		}
		video_enqueue(video, VIDEO_EP_OUT, vbuf);
	}
}

K_THREAD_DEFINE(video_ap2cp, 2048, video_ap2cp_thread, NULL, NULL, NULL, 0, 0, 0);
