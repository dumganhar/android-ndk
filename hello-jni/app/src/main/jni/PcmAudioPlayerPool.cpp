//
// Created by James Chen on 6/16/16.
//

#include <android/log.h>
#include "PcmAudioPlayerPool.h"

#define LOG_TAG "AudioPlayerPool"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

std::vector<PcmAudioPlayer*> PcmAudioPlayerPool::_audioPlayerPool;

bool PcmAudioPlayerPool::init(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int deviceBufferSizeInFrames)
{
    _audioPlayerPool.reserve(AUDIO_PLAYER_POOL_SIZE);

    for (int i = 0; i < AUDIO_PLAYER_POOL_SIZE; ++i)
    {
        int numChannels = 1;
        if (i >= (AUDIO_PLAYER_POOL_SIZE * 2 / 3))
        {
            numChannels = 2;
        }

        auto player = new PcmAudioPlayer(engineItf, outputMixObject);
        if (!player->initForPlayPcmData(numChannels, deviceSampleRate, deviceBufferSizeInFrames * numChannels * 2)) {
            LOGE("UrlAudioPlayer pool only supports size with %d", i-1);
            delete player;
            break;
        }
        LOGD("Insert a UrlAudioPlayer (%d, %p, %d) to pool ...", i, player, numChannels);
        player->setOwnedByPool(true);
        _audioPlayerPool.push_back(player);
    }

    return true;
}

void PcmAudioPlayerPool::destroy() {
    LOGD("PcmAudioPlayerPool::destroy begin ...");
    int i = 0;
    for (const auto& player : _audioPlayerPool)
    {
        LOGD("delete UrlAudioPlayer (%d, %p) ...", i, player);
        delete player;
        ++i;
    }
    _audioPlayerPool.clear();
    LOGD("PcmAudioPlayerPool::destroy end ...");
}

PcmAudioPlayer *PcmAudioPlayerPool::findAvailablePlayer(int numChannels) {
    int i = 0;
    for (const auto& player : _audioPlayerPool)
    {
        if (!player->isPlaying() && player->getChannelCount() == numChannels)
        {
            LOGD("UrlAudioPlayer %d is working ...", i);
            return player;
        }
        ++i;
    }
    LOGE("Could not find available audio player with %d channels!", numChannels);
    return nullptr;
}
