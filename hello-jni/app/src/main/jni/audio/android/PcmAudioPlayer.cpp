//
// Created by James Chen on 7/4/16.
//

#define LOG_TAG "PcmAudioPlayer"

#include "PcmAudioPlayer.h"
#include "OpenSLHelper.h"
#include "AudioFlinger.h"

namespace cocos2d {

PcmAudioPlayer::PcmAudioPlayer(AudioFlinger* flinger)
        : _id(-1)
        , _volume(0.0f)
        , _isLoop(false)
        , _state(State::INVALID)
        , _track(nullptr)
        , _playEventCallback(nullptr)
        , _audioFlinger(flinger)
{
}

PcmAudioPlayer::~PcmAudioPlayer()
{
    SL_SAFE_DELETE(_track);
}

bool PcmAudioPlayer::prepare(const std::string &url, const PcmData &decResult)
{
    std::lock_guard<std::mutex> lk(_stateMutex);
    if (_state == State::PLAYING)
    {
        LOGE("PcmAudioService (%s) is playing, ignore play ...", _url.c_str());
        return false;
    }
    else
    {
        _url = url;
        _decResult = decResult;

        setVolume(1.0f);

        _track = new Track(_decResult);
        _track->onDestroy = [this](){
            delete this;
        };
    }

    return true;
}

void PcmAudioPlayer::rewind()
{
}

void PcmAudioPlayer::setVolume(float volume)
{
    _volume = volume;
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
    return _decResult.duration;
}

float PcmAudioPlayer::getPosition() const
{
    return 0.0f;
}

bool PcmAudioPlayer::setPosition(float pos)
{
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

void PcmAudioPlayer::play()
{
    // put track to AudioFlinger
    LOGD("PcmAudioPlayer (%p) play ...", this);
    _audioFlinger->addTrack(_track);
}

void PcmAudioPlayer::pause()
{
    LOGD("PcmAudioPlayer (%p) pause ...", this);
    _track->setState(Track::State::PAUSED);
}

void PcmAudioPlayer::resume()
{
    LOGD("PcmAudioPlayer (%p) resume ...", this);
    _track->setState(Track::State::RESUMED);
}

void PcmAudioPlayer::stop()
{
    LOGD("PcmAudioPlayer (%p) stop ...", this);
    _track->setState(Track::State::STOPPED);
}


} // namespace cocos2d {
