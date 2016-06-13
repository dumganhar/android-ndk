//
// Created by James Chen on 6/13/16.
//

#include "AudioPlayer.h"

#include <android/log.h>

#define LOG_TAG "cjh"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

class SLAudioPlayerCallbackProxy
{
public:
    static void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
        LOGD("samplePlayerCallback, context = %p ...", context);
        AudioPlayer* thiz = reinterpret_cast<AudioPlayer*>(context);
        thiz->samplePlayerCallback(bq);
    }
};

AudioPlayer::AudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject, const AudioDecoder::Result& decResult)
        : _engineItf(engineItf)
        , _outputMixObj(outputMixObject)
        , _decResult(decResult)
{

}

void AudioPlayer::samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq)
{
    // It's in sub thread
    LOGD("samplePlayerCallback, context = %p ...", this);
    SLresult res;
    res = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
    res = (*_bufferQueueItf)->Clear(_bufferQueueItf);
}

void AudioPlayer::play()
{
    LOGD("AudioPlayer::play, bufSize: %d", (int) _decResult.pcmBuffer->size());
    SLuint32 channelMask = SL_SPEAKER_FRONT_CENTER;

    if (_decResult.channelCount == 2) {
        channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    }

    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            (SLuint32)_decResult.channelCount,
            (SLuint32)(_decResult.sampleRate * 1000),
            SL_PCMSAMPLEFORMAT_FIXED_16,
            16,
            channelMask,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            1
    };
    SLDataSource source = {&loc_bufq, &format_pcm};

    SLDataLocator_OutputMix loc_outmix = {
            SL_DATALOCATOR_OUTPUTMIX,
            _outputMixObj
    };
    SLDataSink sink = {&loc_outmix, NULL};

#undef NUM
#define NUM (2)

    const SLInterfaceID ids[NUM] = {
            SL_IID_VOLUME,
            SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
    };
    const SLboolean req[NUM] = {
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
    };

    SLresult res;

    LOGD("audio.cpp, before CreateAudioPlayer ...");
    res = (*_engineItf)->CreateAudioPlayer(_engineItf, &_playObj, &source, &sink, NUM, ids, req);
    LOGD("audio.cpp, after CreateAudioPlayer ...");

    res = (*_playObj)->Realize(_playObj, SL_BOOLEAN_FALSE);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_PLAY, &_playItf);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_VOLUME, &_volumeItf);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &_bufferQueueItf);
    res = (*_bufferQueueItf)->RegisterCallback(_bufferQueueItf, SLAudioPlayerCallbackProxy::samplePlayerCallback, this);
    res = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, _decResult.pcmBuffer->data(), _decResult.pcmBuffer->size());
    res = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
}
