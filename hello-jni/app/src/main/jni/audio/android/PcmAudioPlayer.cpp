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

#include "audio/android/cutils/log.h"
#include "audio/android/PcmAudioPlayer.h"
#include "audio/android/OpenSLHelper.h"
#include "audio/android/AudioMixerController.h"

namespace cocos2d {

PcmAudioPlayer::PcmAudioPlayer(AudioMixerController * controller)
        : _id(-1)
        , _volume(0.0f)
        , _isLoop(false)
        , _state(State::INVALID)
        , _track(nullptr)
        , _playEventCallback(nullptr)
        , _controller(controller)
{
}

PcmAudioPlayer::~PcmAudioPlayer()
{
    ALOGV("In the destructor of PcmAudioPlayer (%p)", this);
    SL_SAFE_DELETE(_track);
}

bool PcmAudioPlayer::prepare(const std::string &url, const PcmData &decResult)
{
    std::lock_guard<std::mutex> lk(_stateMutex);
    if (_state == State::PLAYING)
    {
        ALOGE("PcmAudioService (%s) is playing, ignore play ...", _url.c_str());
        return false;
    }
    else
    {
        _url = url;
        _decResult = decResult;

        setVolume(1.0f);

        _track = new Track(_decResult);
        _track->onStateChanged = [this](Track::State state) {
            if (_playEventCallback != nullptr)
            {
                if (state == Track::State::OVER)
                {
                    _playEventCallback(State::OVER);
                }
                else if (state == Track::State::STOPPED)
                {
                    _playEventCallback(State::STOPPED);
                }
                else if (state == Track::State::DESTROYED)
                {
                    delete this;
                }
            }
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
    // put track to AudioMixerController
    ALOGV("PcmAudioPlayer (%p) play (%s) ...", this, _url.c_str());
    _controller->addTrack(_track);
}

void PcmAudioPlayer::pause()
{
    ALOGV("PcmAudioPlayer (%p) pause ...", this);
    _track->setState(Track::State::PAUSED);
}

void PcmAudioPlayer::resume()
{
    ALOGV("PcmAudioPlayer (%p) resume ...", this);
    _track->setState(Track::State::RESUMED);
}

void PcmAudioPlayer::stop()
{
    ALOGV("PcmAudioPlayer (%p) stop ...", this);
    _track->setState(Track::State::STOPPED);
}


} // namespace cocos2d {
