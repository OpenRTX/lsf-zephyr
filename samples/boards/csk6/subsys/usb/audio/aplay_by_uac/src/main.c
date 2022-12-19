/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample app for Aplay by USB Audio
 */

#include "kernel.h"
#include "logging/log.h"
#include <stdint.h>
#include <string.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>
#include <usb/usb_device.h>
#include <usb/class/usb_audio.h>
#include <avf/framework/avf_platform.h>
#include <licak/modules/audio/aplay.h>
#include <licak/licak.h>
#include <avf/slogger.h>
#include "pwr_amp.c"
#define PERIOD_BYTES(fmt) amedia_frames_to_bytes(fmt, 1000)

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT 0x20746d66
#define ID_DATA 0x61746164

#define UAC_BUF_SIZE (96 * 1000)

#define UAC_BUF_CACHE_SIZE (96 * 5)

#define UAC_BUF_TIMEOUT (K_MSEC(5))	//ms

#define UAC_BUF_TYPE 1

#define PRIORITY 6

#define UAC_BIT_DEPTH 16
#define UAC_SAMPLE_RATE 48000
#define UAC_CHANNEL_SUM 1

struct riff_wave_header {
	uint32_t riff_id;
	uint32_t riff_sz;
	uint32_t wave_id;
};

struct chunk_header {
	uint32_t id;
	uint32_t sz;
};

struct chunk_fmt {
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
};

static aplay_t *aplay = NULL;

K_SEM_DEFINE(uac_sem, 0, 1);
RING_BUF_DECLARE(uac_buf, UAC_BUF_SIZE);

K_THREAD_STACK_DEFINE(thread_uac_stack_area, 1024);
static struct k_thread thread_uac;

static void timer_expiry_cb(struct k_timer *timer);
K_TIMER_DEFINE(uac_timer, timer_expiry_cb, NULL);


static void timer_expiry_cb(struct k_timer *timer)
{
	k_sem_give(&uac_sem);
}

static void put_in_buf(uint8_t *data, size_t size)
{
	int ret;
	uint8_t *buf_data = NULL;
	uint32_t buf_size = 0;
	buf_size = ring_buf_put_claim(&uac_buf, &buf_data, size);
	if (buf_size != size) {
		LOG_ERR("actual claimed size: %d, request size: %d", buf_size, size);
	}
	memcpy(buf_data, data, buf_size);

	ret = ring_buf_put_finish(&uac_buf, buf_size);
	if (ret != 0) {
		LOG_ERR("ring_buf_put_finish ret: %d", ret);
	}

}

static void uac_notify(void)
{
	int used_size = ring_buf_size_get(&uac_buf);
	/* Cache some data then use aplay to play */
	if (used_size >= UAC_BUF_CACHE_SIZE) {
		k_sem_give(&uac_sem);
		k_timer_stop(&uac_timer);
	} else {
		/* Make sure left data in buffer could be consumed */
		k_timer_start(&uac_timer, UAC_BUF_TIMEOUT, K_NO_WAIT);
	}
}

static void get_from_buf(uint8_t **out_data_addr, size_t *out_size)
{
	*out_size = ring_buf_get_claim(&uac_buf, out_data_addr, UAC_BUF_SIZE);
}

static void finish_get_from_buf(size_t size)
{
	int ret;
	ret = ring_buf_get_finish(&uac_buf, size);
	if (ret != 0) {
		LOG_ERR("ring_buf_get_finish ret: %d", ret);
	}
}

static void play_audio(uint8_t *data, size_t size)
{
	int iret;
	iret = aplay_writei(aplay, (char *)data, size);
	if (iret != size) {
		LOG_ERR("### aplay_writei error ret %d, size %d ####", iret, size);
		return;
	}
}

void thread_uac_entry(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	uint8_t *data = NULL;
	size_t size = 0;

	while (true) {
		k_sem_take(&uac_sem, K_FOREVER);
		get_from_buf(&data, &size);
		play_audio(data, size);
		finish_get_from_buf(size);
	}
}

static void init_uac_thread(void)
{
	k_thread_create(&thread_uac, thread_uac_stack_area,
			K_THREAD_STACK_SIZEOF(thread_uac_stack_area), thread_uac_entry, NULL, NULL,
			NULL, PRIORITY, 0, K_NO_WAIT);
}


static void init_sof(void)
{
	int iret;
	amedia_fmt_t fmt;

	licak_init();
	memset(&fmt, 0, sizeof(amedia_fmt_t));

	fmt.channels = UAC_CHANNEL_SUM;
	fmt.rate = UAC_SAMPLE_RATE;
	fmt.compr = AMEDIA_COMPR_PCM;
	fmt.bits = UAC_BIT_DEPTH;

	printk("Prepare start play wav and get audio fmt <PCM/%d/%d /%d>\n", fmt.rate, fmt.channels,
	       fmt.bits);
	/*step1: create aplyer instance*/
	aplay = aplay_create(APLAYER_SUBSTREAM_MAIN);
	if (aplay == NULL) {
		printk("acap_create failed!\n");
		return;
	}
	printk("Create aplay success\n");

	/*step2: set aplayer paramter*/
	if (0 != (iret = aplay_set_fmt(aplay, &fmt))) {
		printk("aplay_set_fmt failed!\n");
		return;
	}
	printk("Set audio fmt to aplay success\n");

	/*Enable audio power-amp*/
	audio_pwr_amp_init();
    audio_pwr_amp_enbale();

	/*step3: start play*/
	if (0 != (iret = aplay_start(aplay))) {
		printk("aplay_set_fmt failed.\n");
		return;
	}
	printk("Trigger play start success.\n");

}

static void data_received(const struct device *dev, struct net_buf *buffer, size_t size)
{
	if (!buffer || !size) {
		/* This should never happen */
		return;
	}

	put_in_buf(buffer->data, size);
	net_buf_unref(buffer);
	uac_notify();
}

static void feature_update(const struct device *dev, const struct usb_audio_fu_evt *evt)
{
	LOG_DBG("Control selector %d for channel %d updated", (uint8_t)evt->cs, evt->channel);
	switch (evt->cs) {
	case USB_AUDIO_FU_MUTE_CONTROL:
		break;
	case USB_AUDIO_FU_VOLUME_CONTROL:
		LOG_DBG("[%s] volume: %d, val_len: %d", __FUNCTION__, *((int16_t *)(evt->val)),
			evt->val_len);
		break;
	default:
		break;
	}
}

static const struct usb_audio_ops ops = {
	.data_received_cb = data_received,
	.feature_update_cb = feature_update,
};

void main(void)
{
	const struct device *hs_dev;
	int ret;

	hs_dev = device_get_binding("HEADSET");

	if (!hs_dev) {
		printk("Can not get USB Headset Device \n");
		return;
	}

	printk("Found USB Headset Device \n");

	usb_audio_register(hs_dev, &ops);

	ret = usb_enable(NULL);
	if (ret != 0) {
		printk("Failed to enable USB \n");
		return;
	}

	printk("USB enabled \n");

	init_sof();
	init_uac_thread();
}
