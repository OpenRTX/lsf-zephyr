#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lisa_factory_test_tools.h"
#include <lisa_mem.h>

#define PCM_CHANNEL			(1)
#define PCM_BITS_PER_SAMPLE	(16)
#define PCM_SAMPLERATE		(16000)
#define PCM_FREQUENCY		(1000)
#define PCM_AMP				(0.8)
#define PCM_DURATION		(3)
#define PI					(3.1415926)

extern int analysis_record_pcm(const unsigned char* data, int size);

void lisa_factory_genSinePcm(const genPcmCb cb) {
	if (!cb)
		return;
	const int per_sample_count = 100; // callback 100 samples for once
	short *buffer = (short *)lisa_mem_alloc(sizeof(short) * per_sample_count);
	const int numSamples = PCM_SAMPLERATE * PCM_DURATION;
	int i, j;
	for (i = 0, j = 0; i < numSamples; i++) {
		buffer[j++] = (short)(32767 * PCM_AMP * (sin(i * 2 * PI * PCM_FREQUENCY / PCM_SAMPLERATE)));
		if (j == per_sample_count) {
			j = 0;
			cb(buffer, per_sample_count);
		}
	}
	lisa_mem_free(buffer);
}

int lisa_factory_analysisRecPcm(const uint8_t* recBuffer, int size)
{
	return analysis_record_pcm(recBuffer, size);
}