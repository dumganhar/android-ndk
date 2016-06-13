#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
 
#define AUDIO_TAG   "ndk-audio"
 
typedef struct {
    SLObjectItf object;
    SLEngineItf engine;
} AudioEngine;
 
typedef struct {
    SLObjectItf object;
} AudioOutputMix;
 
typedef struct {
    SLObjectItf object;
    SLPlayItf play;
    SLVolumeItf volume;
    SLAndroidSimpleBufferQueueItf buffer_queue;
} SamplePlayer;
 
typedef struct {
    char* buffer;
    unsigned int num_samples;
    unsigned int sample_rate;
    unsigned short num_channels;
    unsigned short bytes_per_sample;
} Sample;
 
static inline unsigned int byteSize(const Sample* s) {
    return s->num_samples * s->bytes_per_sample * s->num_channels;
}
 
#define DESTROY(OBJ)    \
    if ((OBJ) != NULL) { \
        (*(OBJ))->Destroy(OBJ); \
        (OBJ) = NULL; \
    }

void playPcm(const char* path);
void playPcmBuffer(const char* buf, int bufSize, int channelCount, int sampleRate);

#endif//__AUDIO_H__