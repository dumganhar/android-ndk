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

#define LOG_TAG "PcmAudioPlayer"

#include "audio/android/PcmAudioPlayer.h"

#include <math.h>
#include <unistd.h>
#include <algorithm> // for std::find

#define AUDIO_PLAYER_BUFFER_COUNT (2)

static std::vector<char> __silenceData;

class SLPcmAudioPlayerCallbackProxy
{
public:
    static void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
    {
        PcmAudioPlayer* thiz = reinterpret_cast<PcmAudioPlayer*>(context);
        thiz->samplePlayerCallback(bq);
    }
};

PcmAudioPlayer::PcmAudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject)
        : _engineItf(engineItf)
        , _outputMixObj(outputMixObject)
        , _id(-1)
        , _playObj(nullptr)
        , _playItf(nullptr)
        , _volumeItf(nullptr)
        , _bufferQueueItf(nullptr)
        , _numChannels(-1)
        , _sampleRate(-1)
        , _bufferSizeInBytes(0)
        , _volume(0.0f)
        , _isLoop(false)
        , _state(State::INVALID)
        , _currentBufferIndex(0)
        , _playEventCallback(nullptr)
{
}

PcmAudioPlayer::~PcmAudioPlayer()
{
    LOGD("~PcmAudioPlayer() (%p), before destroy play object", this);
    _stateMutex.lock();
    SL_DESTROY_OBJ(_playObj);
    _stateMutex.unlock();
    LOGD("~PcmAudioPlayer() end");
}

void PcmAudioPlayer::onPlayOver()
{
    if (_state == State::PLAYING)
    {
        setState(State::OVER);
        if (_playEventCallback != nullptr)
        {
            _playEventCallback(_state);
        }
    }

    setId(-1);
    setLoop(false);
    _currentBufferIndex = 0;
    _url = "";
    _decResult.reset();
}

bool PcmAudioPlayer::enqueue()
{
    if (!_decResult.isValid())
    {
        LOGD("_decResult.isValid: false");
        return false;
    }

    char* base = _decResult.pcmBuffer->data();
    char* data = base + _currentBufferIndex;
    int remain = _decResult.pcmBuffer->size() - _currentBufferIndex;
    int size = std::min(remain, _bufferSizeInBytes);

//    LOGD("PcmAudioPlayer (%p, %d) enqueue buffer size: %d, index = %d, totalEnqueueSize: %d", this, getId(), size, _currentBufferIndex, _currentBufferIndex + size);
    SLresult  r = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, data, size);
    SL_RETURN_VAL_IF_FAILED(r, false, "PcmAudioPlayer::enqueue failed");

    _currentBufferIndex += size;

    return true;
}

void PcmAudioPlayer::samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq)
{
    // FIXME: PcmAudioPlayer instance may be destroyed, we need to find a way to wait...
    // It's in sub thread
    _stateMutex.lock();

    if (_state == State::PLAYING)
    {
        bool isPlayOver = false;
        int pcmDataSize = _decResult.pcmBuffer ? _decResult.pcmBuffer->size() : 0;
        if (_currentBufferIndex >= pcmDataSize)
        {
            if (_isLoop)
            {
                _currentBufferIndex = 0;
            }
            else
            {
                onPlayOver();
                setState(State::INITIALIZED);
                (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
                isPlayOver = true;
            }
        }

        if (!isPlayOver)
        {
            enqueue();
        }
    }
    else if (_state == State::INVALID)
    {
        setState(State::INITIALIZED);
    }

    _stateMutex.unlock();
}

void PcmAudioPlayer::play()
{
    std::lock_guard<std::mutex> lk(_stateMutex);
    if (_state == State::PLAYING)
    {
        LOGE("PcmAudioPlayer (%p, %d) is playing, ignore play ...", this, getId());
    }
    else
    {
//        LOGD("PcmAudioPlayer (%p, %d) will play ...", this, getId());
        int counter = 0; // try to wait 1.5ms
        while (_state != State::INITIALIZED)
        {
            if (counter >= 6)
            {
                counter = -1;
                break;
            }
            // If player isn't ready, just wait for 500us
            LOGD("Waiting player (%p) initialzied, count:%d, state: %d", this, counter, _state);
            usleep(250);
            ++counter;
        }

        if (counter == -1)
        {
            LOGD("Player isn't ready, ignore this play!");
        }
        else
        {
            if (enqueue())
            {
                setState(State::PLAYING);
                SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
                SL_RETURN_IF_FAILED(r, "SetPlayState SL_PLAYSTATE_PLAYING failed");
            }
        }
    }
}

void PcmAudioPlayer::stop()
{
    std::lock_guard<std::mutex> lk(_stateMutex);
    LOGD("PcmAudioPlayer(%p, %d)::stop ...", this, getId());
    if (_state == State::PLAYING)
    {
        setLoop(false);
        setState(State::STOPPED);

        if (_playEventCallback != nullptr)
        {
            _playEventCallback(_state);
        }
    }
}

void PcmAudioPlayer::pause()
{
    std::lock_guard<std::mutex> lk(_stateMutex);
    LOGD("PcmAudioPlayer(%p, %d)::pause ...", this, getId());
    if (_state == State::PLAYING)
    {
        SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PAUSED);
        SL_RETURN_IF_FAILED(r, "PcmAudioPlayer::pause()");
        setState(State::PAUSED);
    }
}

void PcmAudioPlayer::resume()
{
    std::lock_guard<std::mutex> lk(_stateMutex);
    if (_state == State::PAUSED)
    {
        LOGD("PcmAudioPlayer(%p, %d)::resume ...", this, getId());
        SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
        SL_RETURN_IF_FAILED(r, "PcmAudioPlayer::resume()");
        setState(State::PLAYING);
    }
}

bool PcmAudioPlayer::init(int numChannels, int sampleRate, int bufferSizeInBytes)
{
    _numChannels = numChannels;
    _sampleRate = sampleRate;
    _bufferSizeInBytes = bufferSizeInBytes;

    if (__silenceData.empty())
    {
        __silenceData.resize(_bufferSizeInBytes * AUDIO_PLAYER_BUFFER_COUNT, 0);
    }

    SLuint32 channelMask = SL_SPEAKER_FRONT_CENTER;

    if (numChannels > 1)
    {
        channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    }

    SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            (SLuint32) numChannels,
            (SLuint32) sampleRate * 1000,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            channelMask,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataLocator_AndroidSimpleBufferQueue locBufQueue = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            AUDIO_PLAYER_BUFFER_COUNT
    };
    SLDataSource source = {&locBufQueue, &formatPcm};

    SLDataLocator_OutputMix locOutmix = {
            SL_DATALOCATOR_OUTPUTMIX,
            _outputMixObj
    };
    SLDataSink sink = {&locOutmix, nullptr};

    const SLInterfaceID ids[] = {
            SL_IID_PLAY,
            SL_IID_VOLUME,
            SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
    };

    const SLboolean req[] = {
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
    };

    SLresult r;

    r = (*_engineItf)->CreateAudioPlayer(_engineItf, &_playObj, &source, &sink, sizeof(ids) / sizeof(ids[0]), ids, req);
    SL_RETURN_VAL_IF_FAILED(r, false, "CreateAudioPlayer failed");

    r = (*_playObj)->Realize(_playObj, SL_BOOLEAN_FALSE);
    SL_RETURN_VAL_IF_FAILED(r, false, "Realize failed");

    r = (*_playObj)->GetInterface(_playObj, SL_IID_PLAY, &_playItf);
    SL_RETURN_VAL_IF_FAILED(r, false, "GetInterface SL_IID_PLAY failed");

    r = (*_playObj)->GetInterface(_playObj, SL_IID_VOLUME, &_volumeItf);
    SL_RETURN_VAL_IF_FAILED(r, false, "GetInterface SL_IID_VOLUME failed");

    r = (*_playObj)->GetInterface(_playObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &_bufferQueueItf);
    SL_RETURN_VAL_IF_FAILED(r, false, "GetInterface SL_IID_ANDROIDSIMPLEBUFFERQUEUE failed");

    r = (*_bufferQueueItf)->RegisterCallback(_bufferQueueItf, SLPcmAudioPlayerCallbackProxy::samplePlayerCallback, this);
    SL_RETURN_VAL_IF_FAILED(r, false, "_bufferQueueItf RegisterCallback failed");

    r = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, __silenceData.data(), _bufferSizeInBytes);
    SL_RETURN_VAL_IF_FAILED(r, false, "_bufferQueueItf Enqueue failed");

    r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
    SL_RETURN_VAL_IF_FAILED(r, false, "SetPlayState SL_PLAYSTATE_PLAYING failed");

    setState(State::INVALID);

    return true;
}

bool PcmAudioPlayer::prepare(const std::string& url, const PcmData &decResult)
{
    std::lock_guard<std::mutex> lk(_stateMutex);
    if (_state == State::PLAYING)
    {
        LOGE("PcmAudioPlayer (%s) is playing, ignore play ...", _url.c_str());
        return false;
    }
    else
    {
//        std::string pcmInfo = decResult.toString();
//        LOGD("PcmAudioPlayer::prepare %s, decResult: %s", url.c_str(), pcmInfo.c_str());
        _url = url;
        _decResult = decResult;
        _currentBufferIndex = 0;

        if (_decResult.sampleRate != _sampleRate)
        {
            LOGE("Wrong sampleRate %d , expected %d", _decResult.sampleRate, _sampleRate);
        }

        if (_decResult.numChannels != _numChannels)
        {
            LOGE("Wrong channel count %d , expected %d", _decResult.numChannels, _numChannels);
        }

        setVolume(1.0f);
    }

    return true;
}

void PcmAudioPlayer::rewind()
{
    LOGE("PcmAudioPlayer::rewind wasn't implemented!");
}

void PcmAudioPlayer::setVolume(float volume)
{
    _volume = volume;
    int dbVolume = 2000 * log10(volume);
    if(dbVolume < SL_MILLIBEL_MIN){
        dbVolume = SL_MILLIBEL_MIN;
    }
    SLresult r = (*_volumeItf)->SetVolumeLevel(_volumeItf, dbVolume);
    SL_RETURN_IF_FAILED(r, "PcmAudioPlayer::setVolume %f", volume);
}

float PcmAudioPlayer::getVolume() const
{
    return _volume;
}

void PcmAudioPlayer::setLoop(bool isLoop)
{
    _isLoop = isLoop;
}

bool PcmAudioPlayer::isLoop() const
{
    return _isLoop;
}

float PcmAudioPlayer::getDuration() const
{
    LOGE("PcmAudioPlayer::getDuration wan't implemented!");
    return 0;
}

float PcmAudioPlayer::getPosition() const
{
    LOGE("PcmAudioPlayer::getPosition wan't implemented!");
    return 0;
}

bool PcmAudioPlayer::setPosition(float pos)
{
    LOGE("PcmAudioPlayer::setPosition wan't implemented!");
    return false;
}

void PcmAudioPlayer::setPlayEventCallback(const PlayEventCallback &playEventCallback)
{
    _playEventCallback = playEventCallback;
}

void PcmAudioPlayer::setState(State state)
{
    _state = state;
}
