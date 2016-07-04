//
// Created by James Chen on 7/4/16.
//

#define LOG_TAG "PcmAudioPlayer"

#include "PcmAudioPlayer.h"
#include "OpenSLHelper.h"

PcmAudioPlayer::PcmAudioPlayer()
        : _id(-1)
        , _volume(0.0f)
        , _isLoop(false)
        , _state(State::INVALID)
        , _playEventCallback(nullptr)
{

}

bool PcmAudioPlayer::prepare(const std::string& url, const PcmData &decResult)
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

void PcmAudioPlayer::play() {

}

void PcmAudioPlayer::pause() {

}

void PcmAudioPlayer::resume() {

}

void PcmAudioPlayer::stop() {

}
