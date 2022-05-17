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
    char* pstart;
    char* pcm_data;
    uint32_t buffer_end;
        
    avf_stream_platform_register(dsp_firmware,sizeof(dsp_firmware),dsp_tplg,sizeof(dsp_tplg));
    memset(&fmt,0,sizeof(amedia_fmt_t));
    pcm_data = prase_wav_fmt(test_audio,sizeof(test_audio),&fmt);
    if(pcm_data == NULL){
        printk("test audio fmt error!.\n");
        return;
    }

    printk("Prepare start play wav and get audio fmt <PCM/%d/%d /%d>\n",fmt.rate,fmt.channels,fmt.bits);
    /*step1: create aplyer instance*/
    aplay = aplay_create(APLAYER_SUBSTREAM_MAIN);
    if(aplay == NULL){
        printk("acap_create failed!\n");
        return;
    }
    printk("Create aplay success\n");

    /*step2: set aplayer paramter*/
    if(0 != (iret = aplay_set_fmt(aplay,&fmt))){
        printk("aplay_set_fmt failed!\n");
        return;
    }
    printk("Set audio fmt to aplay success\n");

    /*step3: start play*/
    if(0 != (iret = aplay_start(aplay))){
        printk("aplay_set_fmt failed.\n");
        return;
    }
    printk("Trigger play start success.\n");

    buffer_end =  test_audio + sizeof(test_audio);
    pstart = pcm_data;
    while(pcm_data < buffer_end){

        write_bytes = buffer_end- (uint32_t)pcm_data > PERIOD_BYTES(&fmt)?PERIOD_BYTES(&fmt):buffer_end- (uint32_t)pcm_data;
        /*step4: write data to aplayer*/
        iret = aplay_writei(aplay, pcm_data,write_bytes) ;
        if(iret != write_bytes){
            printk("aplay_writei failed ret %d.\n",iret);
            break;
        }
        pcm_data += iret;
        
    }
    printk("Write play audio data compelete total size %d with timestamp %lld ms\n",(uint32_t)(pcm_data-pstart),k_uptime_get());
    /*step5: drain or stop aplayer*/
    if(0 != (iret = aplay_drain(aplay))){
        printk("aplay_drain failed.\n");
        return;
    }
    printk("play drain finsh with timestamp %lld ms\n",k_uptime_get());
    /*step6: destroy aplayer*/
    if(0 != (iret = aplay_destroy(aplay))){
        printk("aplay_destroy failed.\n");
        return;
    }

    printk("aplay run compelete and exit.\n");
}

