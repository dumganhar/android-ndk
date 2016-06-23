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
#define LOG_TAG "PcmAudioPlayerPool"

#include "audio/android/PcmAudioPlayerPool.h"
#include "audio/android/PcmAudioPlayer.h"

#define AUDIO_PLAYER_POOL_SIZE (20)

PcmAudioPlayerPool::PcmAudioPlayerPool(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int deviceBufferSizeInFrames)
        : _engineItf(engineItf)
        , _outputMixObject(outputMixObject)
        , _deviceSampleRate(deviceSampleRate)
        , _deviceBufferSizeInFrames(deviceBufferSizeInFrames)
{
    _audioPlayerPool.reserve(AUDIO_PLAYER_POOL_SIZE);

    for (int i = 0; i < AUDIO_PLAYER_POOL_SIZE; ++i)
    {
        int numChannels = 1;
        if (i >= (AUDIO_PLAYER_POOL_SIZE / 2))
        {
            numChannels = 2;
        }

        auto player = createPlayer(numChannels);
        if (player != nullptr)
        {
            LOGD("Insert a PcmAudioPlayer (%p, channels:%d) to pool ...", player, numChannels);
            _audioPlayerPool.push_back(player);
        }
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

PcmAudioPlayer *PcmAudioPlayerPool::findAvailablePlayer(int numChannels)
{
    std::vector<PcmAudioPlayer*>::iterator freeIter = _audioPlayerPool.end();
    int i = 0;
    for (auto iter = _audioPlayerPool.begin(); iter != _audioPlayerPool.end(); ++iter)
    {
        if ((*iter)->getState() != IAudioPlayer::State::PLAYING)
        {
            if (freeIter == _audioPlayerPool.end())
            {
                freeIter = iter;
            }

            if ((*iter)->getChannelCount() == numChannels)
            {
                LOGD("PcmAudioPlayer %d is working ...", i);
                return (*iter);
            }
        }
        ++i;
    }

    // Try to delete a free player and create a new one matches the channel count
    if (freeIter != _audioPlayerPool.end())
    {
        LOGD("Removing a player (%p, channel:%d)", (*freeIter), (*freeIter)->getChannelCount());
        delete (*freeIter);
        _audioPlayerPool.erase(freeIter);
        auto player = createPlayer(numChannels);
        if (player != nullptr)
        {
            LOGD("Insert a PcmAudioPlayer (%p, channels:%d) to pool ...", player, numChannels);
            _audioPlayerPool.push_back(player);
            return player;
        }
    }

    LOGE("Could not find available audio player with %d channels!", numChannels);
    return nullptr;
}

PcmAudioPlayer *PcmAudioPlayerPool::createPlayer(int numChannels)
{
    PcmAudioPlayer* player = new PcmAudioPlayer(_engineItf, _outputMixObject);
    if (player != nullptr)
    {
        if (!player->initForPlayPcmData(numChannels, _deviceSampleRate, _deviceBufferSizeInFrames * numChannels * 2))
        {
            delete player;
            player = nullptr;
        }
    }

    return player;
}

