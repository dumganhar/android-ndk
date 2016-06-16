//
// Created by James Chen on 6/16/16.
//

#ifndef HELLO_JNI_AUDIOPLAYERPOOL_H
#define HELLO_JNI_AUDIOPLAYERPOOL_H

#include "PcmAudioPlayer.h"

#include <vector>

#define AUDIO_PLAYER_POOL_SIZE (10)

class PcmAudioPlayerPool
{
public:
    static bool init(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int deviceBufferSizeInFrames);
    static void destroy();

    static PcmAudioPlayer* findAvailablePlayer(int numChannels);

private:
    static std::vector<PcmAudioPlayer*> _audioPlayerPool;
};


#endif //HELLO_JNI_AUDIOPLAYERPOOL_H
