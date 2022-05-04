#include "lisa_evs_audioencoder.h"
#include "stdlib.h"
#include "stdio.h"
#include <lisa_mem.h>

#define TAG "evs_audioencoder"

#define ENCODE_QUALITY (9)
#define FRAME_SIZE (640)

evs_audioencoder_t *
lisa_evs_audioencoder_create()
{
	evs_audioencoder_t *handle = (evs_audioencoder_t *)lisa_mem_alloc(sizeof(evs_audioencoder_t));
	handle->m_encoder = speex_encoder_init(&speex_wb_mode);
	int frameSize;
	int quality = ENCODE_QUALITY;
	speex_encoder_ctl(handle->m_encoder, SPEEX_SET_QUALITY, &quality);
	speex_encoder_ctl(handle->m_encoder, SPEEX_GET_FRAME_SIZE, &frameSize);
	speex_bits_init(&handle->m_bits);
	return handle;
}

void
lisa_evs_audioencoder_destroy(evs_audioencoder_t *handle)
{
	if (handle) {
		speex_bits_destroy(&handle->m_bits);
		speex_encoder_destroy(handle->m_encoder);
		lisa_mem_free(handle);
	}
}

int
lisa_evs_audioencoder_encode(evs_audioencoder_t *handle, const uint8_t *samples, int len, uint8_t *buffer)
{
	speex_bits_reset(&handle->m_bits);
	speex_encode_int(handle->m_encoder, (spx_int16_t *)samples, &handle->m_bits);
	int ret = speex_bits_write(&handle->m_bits, (char*)buffer, 200);
	return ret;
}
