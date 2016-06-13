//
// Created by James Chen on 6/13/16.
//

#ifndef HELLO_JNI_AUDIOPLAYER_H
#define HELLO_JNI_AUDIOPLAYER_H


#include "AudioDecoder.h"

class AudioPlayer {
public:
    AudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject, const AudioDecoder::Result& decResult);

    void play();

private:
    void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq);

private:
    SLEngineItf _engineItf;
    SLObjectItf _outputMixObj;
    AudioDecoder::Result _decResult;

    SLObjectItf _playObj;
    SLPlayItf _playItf;
    SLVolumeItf _volumeItf;
    SLAndroidSimpleBufferQueueItf _bufferQueueItf;

    friend class SLAudioPlayerCallbackProxy;
};


#endif //HELLO_JNI_AUDIOPLAYER_H
