/****************************************************************************
Copyright (c) 2016 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
#include "PcmAudioPlayerPool.h"
#include "PcmAudioPlayer.h"

#include <android/log.h>
#define LOG_TAG "PcmAudioPlayerPool"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

PcmAudioPlayerPool::PcmAudioPlayerPool(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int deviceBufferSizeInFrames)
{
    _audioPlayerPool.reserve(AUDIO_PLAYER_POOL_SIZE);

    for (int i = 0; i < AUDIO_PLAYER_POOL_SIZE; ++i)
    {
        //FIXME: find a better algorithm to pre-allocate PcmAudioPlayer,
        // should not take up all audio resources, since we need remain at least one for UrlAudioPlayer
        int numChannels = 1;
        if (i >= (AUDIO_PLAYER_POOL_SIZE * 2 / 3))
        {
            numChannels = 2;
        }

        auto player = new PcmAudioPlayer(engineItf, outputMixObject);
        if (!player->initForPlayPcmData(numChannels, deviceSampleRate, deviceBufferSizeInFrames * numChannels * 2)) {
            LOGE("PcmAudioPlayer pool only supports size with %d", i-1);
            delete player;
            break;
        }
        LOGD("Insert a PcmAudioPlayer (%d, %p, %d) to pool ...", i, player, numChannels);
        _audioPlayerPool.push_back(player);
    }
}

PcmAudioPlayerPool::~PcmAudioPlayerPool()
{
    LOGD("PcmAudioPlayerPool::destroy begin ...");
    int i = 0;
    for (const auto& player : _audioPlayerPool)
    {
        LOGD("delete PcmAudioPlayer (%d, %p) ...", i, player);
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
            LOGD("PcmAudioPlayer %d is working ...", i);
            return player;
        }
        ++i;
    }
    LOGE("Could not find available audio player with %d channels!", numChannels);
    return nullptr;
}
