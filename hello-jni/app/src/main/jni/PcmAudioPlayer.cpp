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

#include "PcmAudioPlayer.h"

#include <android/log.h>
#include <math.h>
#include <unistd.h>

#define LOG_TAG "PcmAudioPlayer"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

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
        , _isLoop(false)
        , _volume(0.0f)
        , _isPlaying(false)
        , _isDestroyed(false)
        , _currentBufferIndex(0)
        , _playOverCallback(nullptr)
        , _playOverCallbackContext(nullptr)
{
}

PcmAudioPlayer::~PcmAudioPlayer()
{
    LOGD("In the destructor of PcmAudioPlayer (%p)", this);
    _isDestroyed = true;

    while (_isDestroyed)
    {
        _enqueueCond.notify_one();
        usleep(10 * 1000);
    }

    LOGD("PcmAudioPlayer 02");
    SL_DESTROY_OBJ(_playObj);
    LOGD("PcmAudioPlayer end");
}

void PcmAudioPlayer::onPlayOver()
{
    if (isPlaying())
    {
        if (_playOverCallback != nullptr)
        {
            _playOverCallback(this, _playOverCallbackContext);
        }
    }

    reset();
}

void PcmAudioPlayer::onWakeup()
{
    LOGD("PcmAudioPlayer %p onWakeup ...", this);
    setVolume(_volume);
}

void PcmAudioPlayer::enqueue()
{
    char* base = _decResult.pcmBuffer->data();
    char* data = base + _currentBufferIndex;
    int remain = _decResult.pcmBuffer->size() - _currentBufferIndex;
    int size = std::min(remain, _bufferSizeInBytes);

//    LOGD("PcmAudioPlayer (%p, %d) enqueue buffer size: %d, index = %d, totalEnqueueSize: %d", this, getId(), size, _currentBufferIndex, _currentBufferIndex + size);
    SLresult  r = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, data, size);
    SL_RETURN_IF_FAILED(r, "PcmAudioPlayer::enqueue failed");

    _currentBufferIndex += size;
}

void PcmAudioPlayer::samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq)
{
    // FIXME: PcmAudioPlayer instance may be destroyed, we need to find a way to wait...
    // It's in sub thread

    std::unique_lock<std::mutex> lk(_enqueueMutex);
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

            LOGD("PcmAudioPlayer (%p) is waiting ...", this);
            _enqueueCond.wait(lk);

            if (_isDestroyed)
            {
                (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
                (*_bufferQueueItf)->Clear(_bufferQueueItf);
                LOGD("PcmAudioPlayer (%p) was destroyed!", this);
                // Reset _isDestroyed to false
                _isDestroyed = false;
                return;
            }

            onWakeup();
        }
    }

    enqueue();
}

void PcmAudioPlayer::play()
{
    if (isPlaying())
    {
        LOGE("PcmAudioPlayer (%p, %d) is playing, ignore play ...", this, getId());
    }
    else
    {
//        LOGD("PcmAudioPlayer (%p, %d) will play ...", this, getId());
        setPlaying(true);
        _enqueueCond.notify_one();
    }
}

void PcmAudioPlayer::stop()
{
    if (isPlaying())
    {
        LOGD("PcmAudioPlayer(%p, %d)::stop ...", this, getId());
        std::unique_lock<std::mutex> lk(_enqueueMutex);
        _currentBufferIndex = 2147483647;
        setLoop(false);
    }
}

void PcmAudioPlayer::pause()
{
    LOGD("PcmAudioPlayer(%p, %d)::pause ...", this, getId());
    SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PAUSED);
    SL_RETURN_IF_FAILED(r, "PcmAudioPlayer::pause()");
}

void PcmAudioPlayer::resume()
{
    LOGD("PcmAudioPlayer(%p, %d)::resume ...", this, getId());
    SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
    SL_RETURN_IF_FAILED(r, "PcmAudioPlayer::resume()");
}

bool PcmAudioPlayer::initForPlayPcmData(int numChannels, int sampleRate, int bufferSizeInBytes)
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

    return true;
}

bool PcmAudioPlayer::prepare(const std::string& url, const PcmData &decResult)
{
    if (isPlaying())
    {
        LOGE("PcmAudioPlayer (%s) is playing, ignore play ...", _url.c_str());
        return false;
    }
    else
    {
        LOGD("PcmAudioPlayer::prepare %s ...", url.c_str());
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
        LOGD("PcmAudioPlayer::prepare end ..");
    }

    return true;
}

void PcmAudioPlayer::reset()
{
    setId(-1);
    setLoop(false);
    _currentBufferIndex = 0;
    _url = "";
    _decResult.reset();
    setPlaying(false);
}

void PcmAudioPlayer::rewind()
{
    LOGE("PcmAudioPlayer::rewind wan't implemented!");
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

float PcmAudioPlayer::getVolume()
{
    return _volume;
}

void PcmAudioPlayer::setLoop(bool isLoop)
{
    _isLoop = isLoop;
}

bool PcmAudioPlayer::isLoop()
{
    return _isLoop;
}

float PcmAudioPlayer::getDuration()
{
    LOGE("PcmAudioPlayer::getDuration wan't implemented!");
    return 0;
}

float PcmAudioPlayer::getPosition()
{
    LOGE("PcmAudioPlayer::getPosition wan't implemented!");
    return 0;
}

bool PcmAudioPlayer::setPosition(float pos)
{
    LOGE("PcmAudioPlayer::setPosition wan't implemented!");
    return false;
}

void PcmAudioPlayer::setPlayOverCallback(const IAudioPlayer::PlayOverCallback &playOverCallback, void* context) {
    _playOverCallback = playOverCallback;
    _playOverCallbackContext = context;
}
