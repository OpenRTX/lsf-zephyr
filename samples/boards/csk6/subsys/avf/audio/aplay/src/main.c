/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <avf/framework/avf_platform.h>
#include <avf/modules/audio/aplay.h>
#include <avf/slogger.h>
#include "dsp_resource.h"

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164
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

static char* read_buffer(char* pdest,char* psrc,uint32_t size){
    char* pos = psrc;

    memcpy(pdest,psrc,size);
    pos += size;
    return pos;
}

static char* prase_wav_fmt(char* audio_data,uint32_t size,amedia_fmt_t* fmt){
    struct riff_wave_header riff_wave_header;
    struct chunk_header chunk_header;
    struct chunk_fmt chunk_fmt;
    int more_chunks = 1;
    char* pos = audio_data;

    pos = read_buffer(&riff_wave_header,pos,sizeof(riff_wave_header));
    if ((riff_wave_header.riff_id != ID_RIFF) ||
        (riff_wave_header.wave_id != ID_WAVE)) {
        SLOGE(LOG_TAG, "%s:%d, Error: audio format check failed.", __FUNCTION__, __LINE__);
        return NULL;
    }


    do {
        pos = read_buffer(&chunk_header,pos,sizeof(chunk_header));
        switch (chunk_header.id) {
        case ID_FMT:
            pos = read_buffer(&chunk_fmt,pos,sizeof(chunk_fmt));
            /* If the format header is larger, skip the rest */
            if (chunk_header.sz > sizeof(chunk_fmt)){
                pos += chunk_header.sz - sizeof(chunk_fmt);
            }
            break;
        case ID_DATA:
            /* Stop looking for chunks */
            more_chunks = 0;
            break;
        default:
            /* Unknown chunk, skip bytes */
            pos += chunk_header.sz;
            break;
        }
    } while (more_chunks);

    fmt->bits = chunk_fmt.bits_per_sample;
    fmt->rate = chunk_fmt.sample_rate;
    fmt->channels = chunk_fmt.num_channels;
    fmt->compr = AMEDIA_COMPR_PCM;

    return pos;
}

#define PERIOD_BYTES(fmt) amedia_frames_to_bytes(fmt,1000)

void main(void){
    aplay_t* aplay = NULL;
    amedia_fmt_t fmt;
    int32_t iret,write_bytes;
    char* data_buffer;
    char* pcm_data;
    uint32_t buffer_end;

    avf_stream_platform_register(dsp_firmware,sizeof(dsp_firmware),dsp_tplg,sizeof(dsp_tplg));
    pcm_data = prase_wav_fmt(test_audio,sizeof(test_audio),&fmt);
    if(pcm_data == NULL){
        SLOGE(LOG_TAG,"test audio fmt error!.");
        return;
    }

    /*step1: create aplyer instance*/
    aplay = aplay_create(APLAYER_SUBSTREAM_MAIN);
    if(aplay == NULL){
        SLOGE(LOG_TAG,"[%s %d]acap_create failed!",__FUNCTION__,__LINE__);
        return;
    }

    /*step2: set aplayer paramter*/
    if(0 != (iret = aplay_set_fmt(aplay,&fmt))){
        SLOGE(LOG_TAG, "[%s %d]aplay_set_fmt failed.",__FUNCTION__,__LINE__);
        return;
    }

    /*step3: start play*/
    if(0 != (iret = aplay_start(aplay))){
        SLOGE(LOG_TAG, "[%s %d]aplay_start failed.",__FUNCTION__,__LINE__);
        return;
    }
    buffer_end =  test_audio + sizeof(test_audio);
    while(pcm_data < buffer_end){

        write_bytes = buffer_end- (uint32_t)pcm_data > PERIOD_BYTES(&fmt)?PERIOD_BYTES(&fmt):buffer_end- (uint32_t)pcm_data;
        /*step4: write data to aplayer*/
        iret = aplay_writei(aplay, pcm_data,write_bytes) ;
        if(iret != write_bytes){
            SLOGE(LOG_TAG,"aplay_writei failed ret %d.",iret);
            break;
        }
        pcm_data += iret;
        
    }
    /*step5: drain or stop aplayer*/
    if(0 != (iret = aplay_drain(aplay))){
        SLOGE(LOG_TAG, "[%s %d]aplay_drain failed.",__FUNCTION__,__LINE__);
        return;
    }
    
    /*step6: destroy aplayer*/
    aplay_destroy(aplay);

    SLOGI(LOG_TAG, "[%s %d]aplay run compelete and exit.",__FUNCTION__,__LINE__);
}

