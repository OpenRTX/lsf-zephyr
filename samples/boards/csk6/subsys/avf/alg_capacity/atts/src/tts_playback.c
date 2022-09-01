#include <device.h>
#include <devicetree.h>
#include <zephyr.h>
#include <fs/fs.h>
#include <drivers/uart.h>
#include <usb/usb_device.h>
#include <avf/slogger.h>
#include <avf/modules/tts/tts_playback.h>

static K_SEM_DEFINE(sem_tts_playover, 0, 1);
#define TTS_PLAY_TIMOUTMS (60000)
static void audio_tts_event_wrap(void *priv, atts_notify_event_flags_t event)
{
	k_sem_give(&sem_tts_playover);
	printk("atts play over!\n");
}

int atts_playback(const char *tts_string)
{
	atts_t *atts_handle;
	amedia_fmt_t fmt;
	int iret = 0;

	atts_handle = atts_create(AVF_STREAM_DIRECTION_PLAYBACK);

	if (atts_handle == NULL) {
		SLOGE(LOG_TAG, "[%s %d]atts_create failed!", __FUNCTION__, __LINE__);
		return -1;
	}

	memset(&fmt, 0, sizeof(amedia_fmt_t));

	atts_set_fmt(atts_handle, &fmt);
	atts_start(atts_handle);
	atts_notify_event_register(atts_handle, audio_tts_event_wrap, ATTS_NOTIFY_PLAYOVER,
				   atts_handle);

	atts_play(atts_handle, tts_string, strlen(tts_string));

	if (0 != k_sem_take(&sem_tts_playover, K_MSEC(TTS_PLAY_TIMOUTMS))) {
		printk("atts play timout!\n");
		iret = -2;
	} else {
		printk("atts play success!\n");
	}

	atts_stop(atts_handle);
	atts_destroy(atts_handle);

	return iret;
}
