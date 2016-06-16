//
// Created by James Chen on 6/16/16.
//

#ifndef HELLO_JNI_AUDIOPLAYERPOOL_H
#define HELLO_JNI_AUDIOPLAYERPOOL_H

#include "AudioPlayer.h"

#include <vector>

#define AUDIO_PLAYER_POOL_SIZE (10)

class AudioPlayerPool
{
public:
    static bool init(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int deviceBufferSizeInFrames);
    static void destroy();

    // Only supports player which plays pcm data
    static AudioPlayer* findAvailableAudioPlayer(int numChannels);

private:
    static std::vector<AudioPlayer*> _audioPlayerPool;
};


#endif //HELLO_JNI_AUDIOPLAYERPOOL_H
