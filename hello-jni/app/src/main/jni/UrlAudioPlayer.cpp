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

#include "UrlAudioPlayer.h"

#include <android/log.h>
#include <math.h>
#include <unistd.h>

#define LOG_TAG "UrlAudioPlayer"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

std::vector<UrlAudioPlayer*> UrlAudioPlayer::__unusedPlayers;

UrlAudioPlayer::UrlAudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject)
        : _engineItf(engineItf)
        , _outputMixObj(outputMixObject)
        , _id(-1)
        , _assetFd(0)
        , _playObj(nullptr)
        , _playItf(nullptr)
        , _seekItf(nullptr)
        , _volumeItf(nullptr)
        , _volume(0.0f)
        , _isLoop(false)
        , _duration(0.0f)
        , _state(State::INVALID)
        , _isDestroyed(false)
        , _playEventCallback(nullptr)
{
}

UrlAudioPlayer::~UrlAudioPlayer()
{
    LOGD("~UrlAudioPlayer(): %p", this);
}

class SLUrlAudioPlayerCallbackProxy {
public:
    static void playEventCallback(SLPlayItf caller, void *context, SLuint32 playEvent)
    {
        UrlAudioPlayer* player = (UrlAudioPlayer*) context;
        player->playEventCallback(caller, playEvent);
    }
};

void UrlAudioPlayer::playEventCallback(SLPlayItf caller, SLuint32 playEvent)
{
    //Note that it's on sub thread, please don't invoke OpenSLES API on sub thread
    std::unique_lock<std::mutex> lk(_stateMutex);
    if (playEvent == SL_PLAYEVENT_HEADATEND && _state == State::PLAYING)
    {
        //fix issue#8965:AudioEngine can't looping audio on Android 2.3.x
        if (isLoop())
        {
            //FIXME: Don't invoke OpenSLES API here
            play();
        }
        else
        {
            setState(State::OVER);
            if (_playEventCallback != nullptr)
            {
                _playEventCallback(State::OVER);
                __unusedPlayers.push_back(this);
            }
        }
    }
}

void UrlAudioPlayer::setPlayEventCallback(const PlayEventCallback& playEventCallback)
{
    _playEventCallback = playEventCallback;
}

void UrlAudioPlayer::stop()
{
    SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
    SL_RETURN_IF_FAILED(r, "UrlAudioPlayer::stop failed");

    std::unique_lock<std::mutex> lk(_stateMutex);
    if (_state == State::PLAYING)
    {
        setLoop(false);
        setState(State::STOPPED);

        if (_playEventCallback != nullptr)
        {
            _playEventCallback(_state);
            __unusedPlayers.push_back(this);
        }
    }
}

void UrlAudioPlayer::pause()
{
    SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PAUSED);
    SL_RETURN_IF_FAILED(r, "UrlAudioPlayer::pause failed");
    setState(State::PAUSED);
}

void UrlAudioPlayer::resume()
{
    SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
    SL_RETURN_IF_FAILED(r, "UrlAudioPlayer::resume failed");
    setState(State::PLAYING);
}

void UrlAudioPlayer::play()
{
    SLresult r = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
    SL_RETURN_IF_FAILED(r, "UrlAudioPlayer::play failed");
    setState(State::PLAYING);
}

void UrlAudioPlayer::setVolume(float volume)
{
    _volume = volume;
    int dbVolume = 2000 * log10(volume);
    if(dbVolume < SL_MILLIBEL_MIN){
        dbVolume = SL_MILLIBEL_MIN;
    }
    SLresult r = (*_volumeItf)->SetVolumeLevel(_volumeItf, dbVolume);
    SL_RETURN_IF_FAILED(r, "UrlAudioPlayer::setVolume %d failed", dbVolume);
}

float UrlAudioPlayer::getDuration() const
{
    SLmillisecond duration;
    SLresult r = (*_playItf)->GetDuration(_playItf, &duration);
    SL_RETURN_VAL_IF_FAILED(r, 0.0f, "UrlAudioPlayer::getDuration failed");

    if (duration == SL_TIME_UNKNOWN)
    {
        return -1.0f;
    }
    else
    {
        const_cast<UrlAudioPlayer*>(this)->_duration = duration / 1000.0f;

        if (_duration <= 0)
        {
            return -1.0f;
        }
    }
    return _duration;
}

float UrlAudioPlayer::getPosition() const
{
    SLmillisecond millisecond;
    SLresult r = (*_playItf)->GetPosition(_playItf, &millisecond);
    SL_RETURN_VAL_IF_FAILED(r, 0.0f, "UrlAudioPlayer::getPosition failed");
    return millisecond / 1000.0f;
}

bool UrlAudioPlayer::setPosition(float pos)
{
    SLmillisecond millisecond = 1000.0f * pos;
    SLresult r = (*_seekItf)->SetPosition(_seekItf, millisecond, SL_SEEKMODE_ACCURATE);
    SL_RETURN_VAL_IF_FAILED(r, false, "UrlAudioPlayer::setPosition %f failed", pos);
    return true;
}

bool UrlAudioPlayer::prepare(const std::string& url, SLuint32 locatorType, int assetFd, int start, int length)
{
    _url = url;
    _assetFd = assetFd;

    LOGD("UrlAudioPlayer::prepare: %s, %u, %d, %d, %d", _url.c_str(), locatorType, assetFd, start, length);
    SLDataSource audioSrc;

    SLDataFormat_MIME formatMime = {SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_UNSPECIFIED};
    audioSrc.pFormat = &formatMime;

    //Note: locFd & locUri should be outside of the following if/else block
    // Although locFd & locUri are only used inside if/else block, its lifecycle
    // will be destroyed right after '}' block. And since we pass a pointer to
    // 'audioSrc.pLocator=&locFd/&locUri', pLocator will point to an invalid address
    // while invoking Engine::createAudioPlayer interface. So be care of change the postion
    // of these two variables.
    SLDataLocator_AndroidFD locFd;
    SLDataLocator_URI locUri;

    if (locatorType == SL_DATALOCATOR_ANDROIDFD)
    {
        locFd = {locatorType, _assetFd, start, length};
        audioSrc.pLocator = &locFd;
    }
    else if (locatorType == SL_DATALOCATOR_URI)
    {
        locUri = {locatorType, (SLchar *) _url.c_str()};
        audioSrc.pLocator = &locUri;
        LOGD("locUri: locatorType: %d", locUri.locatorType);
    }
    else
    {
        LOGE("Oops, invalid locatorType: %d", locatorType);
        return false;
    }

    // configure audio sink
    SLDataLocator_OutputMix locOutmix = {SL_DATALOCATOR_OUTPUTMIX, _outputMixObj};
    SLDataSink audioSnk = {&locOutmix, nullptr};

    // create audio player
    const SLInterfaceID ids[3] = {SL_IID_SEEK, SL_IID_PREFETCHSTATUS, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    SLresult result = (*_engineItf)->CreateAudioPlayer(_engineItf, &_playObj, &audioSrc, &audioSnk, 3, ids, req);
    SL_RETURN_VAL_IF_FAILED(result, false, "CreateAudioPlayer failed");

    // realize the player
    result = (*_playObj)->Realize(_playObj, SL_BOOLEAN_FALSE);
    SL_RETURN_VAL_IF_FAILED(result, false, "Realize failed");

    // get the play interface
    result = (*_playObj)->GetInterface(_playObj, SL_IID_PLAY, &_playItf);
    SL_RETURN_VAL_IF_FAILED(result, false, "GetInterface SL_IID_PLAY failed");

    // get the seek interface
    result = (*_playObj)->GetInterface(_playObj, SL_IID_SEEK, &_seekItf);
    SL_RETURN_VAL_IF_FAILED(result, false, "GetInterface SL_IID_SEEK failed");

    // get the volume interface
    result = (*_playObj)->GetInterface(_playObj, SL_IID_VOLUME, &_volumeItf);
    SL_RETURN_VAL_IF_FAILED(result, false, "GetInterface SL_IID_VOLUME failed");

    result = (*_playItf)->RegisterCallback(_playItf, SLUrlAudioPlayerCallbackProxy::playEventCallback, this);
    SL_RETURN_VAL_IF_FAILED(result, false, "RegisterCallback failed");

    result = (*_playItf)->SetCallbackEventsMask(_playItf, SL_PLAYEVENT_HEADATEND);
    SL_RETURN_VAL_IF_FAILED(result, false, "SetCallbackEventsMask SL_PLAYEVENT_HEADATEND failed");

    setState(State::INITIALIZED);

    setVolume(1.0f);
    pause();

    return true;
}

void UrlAudioPlayer::rewind()
{
    stop();
    play();
}

float UrlAudioPlayer::getVolume() const
{
    return _volume;
}

void UrlAudioPlayer::setLoop(bool isLoop)
{
    _isLoop = isLoop;

    SLboolean loopEnable = _isLoop ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE;
    SLresult r = (*_seekItf)->SetLoop(_seekItf, loopEnable, 0, SL_TIME_UNKNOWN);
    SL_RETURN_IF_FAILED(r, "UrlAudioPlayer::setLoop %d failed", _isLoop ? 1 : 0);
}

bool UrlAudioPlayer::isLoop() const
{
    return _isLoop;
}

void UrlAudioPlayer::destroyUnusedPlayers()
{
    if (__unusedPlayers.empty())
    {
        return;
    }

    for (auto player : __unusedPlayers)
    {
        delete player;
    }
    __unusedPlayers.clear();
}

void UrlAudioPlayer::destroy()
{
    if (!_isDestroyed)
    {
        LOGD("UrlAudioPlayer::destroy() %p", this);
        _isDestroyed = true;
        SL_DESTROY_OBJ(_playObj);
        LOGD("UrlAudioPlayer::destroy 02");
        if (_assetFd > 0) {
            LOGD("UrlAudioPlayer::destroy 03");
            close(_assetFd);
            _assetFd = 0;
        }
        LOGD("UrlAudioPlayer::destroy end");
    }
}
