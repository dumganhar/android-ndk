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

#include <assert.h>
#include "audio/android/PcmAudioPlayerPool.h"
#include "audio/android/PcmAudioPlayer.h"
#include "audio/android/CCThreadPool.h"

#define AUDIO_PLAYER_POOL_SIZE (20)

PcmAudioPlayerPool::PcmAudioPlayerPool(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int deviceBufferSizeInFrames)
        : _engineItf(engineItf)
        , _outputMixObject(outputMixObject)
        , _deviceSampleRate(deviceSampleRate)
        , _deviceBufferSizeInFrames(deviceBufferSizeInFrames)
{
    _audioPlayerPool.reserve(AUDIO_PLAYER_POOL_SIZE);
    _threadPool = ThreadPool::newCachedThreadPool(4, 10, 10, 2, 2);
    prepareEnoughPlayers();
}

PcmAudioPlayerPool::~PcmAudioPlayerPool()
{
    LOGD("PcmAudioPlayerPool::destroy begin ...");
    delete _threadPool;
    for (const auto& player : _audioPlayerPool)
    {
        LOGD("~PcmAudioPlayerPool(): delete PcmAudioPlayer (%p) ...", player);
        delete player;
    }
    _audioPlayerPool.clear();
    LOGD("PcmAudioPlayerPool::destroy end ...");
}

PcmAudioPlayer *PcmAudioPlayerPool::findAvailablePlayer(int numChannels)
{
    assert(numChannels == 2);

    for (const auto& player : _audioPlayerPool)
    {
        if (player->getState() == IAudioPlayer::State::INITIALIZED)
        {
            LOGD("PcmAudioPlayer (%p) is working ...", player);
            return player;
        }
    }

    LOGE("Could not find available audio player");
    return nullptr;
}

PcmAudioPlayer *PcmAudioPlayerPool::createPlayer(int numChannels)
{
    PcmAudioPlayer* player = new (std::nothrow) PcmAudioPlayer(_engineItf, _outputMixObject, _threadPool);
    if (player != nullptr)
    {
        if (!player->init(numChannels, _deviceSampleRate,
                          _deviceBufferSizeInFrames * numChannels * 2))
        {
            SL_SAFE_DELETE(player);
        }
    }

    return player;
}

void PcmAudioPlayerPool::prepareEnoughPlayers()
{
    ssize_t toAddPlayerCount = AUDIO_PLAYER_POOL_SIZE - _audioPlayerPool.size();
    if (toAddPlayerCount <= 0)
    {
        return;
    }

    LOGD("PcmAudioPlayerPool::prepareEnoughPlayers, toAddPlayerCount: %d", (int)toAddPlayerCount);
    for (int i = 0; i < toAddPlayerCount; ++i)
    {
        auto player = createPlayer(2);
        if (player != nullptr)
        {
            LOGD("Insert a PcmAudioPlayer (%p) to pool ...", player);
            _audioPlayerPool.push_back(player);
        }
        else
        {
            LOGW("Could not create more PcmAudioPlayers, current count: %d", i);
            // should release an extra PcmAudioPlayer for UrlAudioPlayer
            if (!_audioPlayerPool.empty())
            {
                LOGD("Release a seat of PcmAudioPlayer for UrlAudioPlayer");
                delete _audioPlayerPool.at(0);
                _audioPlayerPool.erase(_audioPlayerPool.begin());
            }
            break;
        }
    }
}

void PcmAudioPlayerPool::releaseUnusedPlayers()
{
    LOGD("PcmAudioPlayerPool::releaseUnusedPlayers begin ...");
    PcmAudioPlayer* player = nullptr;
    auto iter = std::begin(_audioPlayerPool);
    while (iter != std::end(_audioPlayerPool))
    {
        player = (*iter);
        if (player->getState() == IAudioPlayer::State::INITIALIZED)
        {
            LOGD("PcmAudioPlayerPool::releaseUnusedPlayers, delete PcmAudioPlayer (%p) ...", player);
            delete player;
            iter = _audioPlayerPool.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    LOGD("PcmAudioPlayerPool::releaseUnusedPlayers end, remain: %d ...", (int)_audioPlayerPool.size());
}
