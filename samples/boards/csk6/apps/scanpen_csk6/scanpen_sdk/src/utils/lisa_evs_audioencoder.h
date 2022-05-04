#ifndef LISTENAI_EVS_AUDIO_ENCODER_H
#define LISTENAI_EVS_AUDIO_ENCODER_H
#include "speex/speex.h"

typedef struct evs_audioencoder_s {
	void *m_encoder;
	SpeexBits m_bits;
} evs_audioencoder_t;

evs_audioencoder_t *lisa_evs_audioencoder_create();
void lisa_evs_audioencoder_destroy(evs_audioencoder_t *handle);
int lisa_evs_audioencoder_encode(evs_audioencoder_t *handle, const uint8_t *samples, int len, uint8_t *buffer);

#endif
