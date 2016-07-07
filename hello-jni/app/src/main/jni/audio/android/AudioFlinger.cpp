//
// Created by James Chen on 7/5/16.
//

#define LOG_TAG "AudioFlinger"

#include "AudioFlinger.h"

#include <algorithm>
#include <audio/android/cutils/log.h>
#include "audio/android/AudioMixer.h"
#include "audio/android/Track.h"
#include "OpenSLHelper.h"

namespace cocos2d {

AudioFlinger::AudioFlinger(int bufferSizeInFrames, int sampleRate, int channelCount)
        : _bufferSizeInFrames(bufferSizeInFrames)
        , _sampleRate(sampleRate)
        , _channelCount(channelCount)
        , _mixingThread(nullptr)
        , _mixer(nullptr)
        , _isDestroy(false)
        , _isPaused(false)
{
    for (int i = 0; i < sizeof(_buffers) / sizeof(_buffers[0]); ++i)
    {
        _buffers[i].size = (size_t) bufferSizeInFrames * 2 * channelCount;
        posix_memalign(&_buffers[i].buf, 32, _buffers[i].size);
        memset(_buffers[i].buf, 0, _buffers[i].size);
        _buffers[i].state = BufferState::EMPTY;
    }

    _busy = &_buffers[0];
    _current = &_buffers[1];
    _next = &_buffers[2];
    _mixing = nullptr;
}

AudioFlinger::~AudioFlinger()
{
    destroy();
    if (_mixingThread != nullptr)
    {
        _mixingThread->join();
        delete _mixingThread;
        _mixingThread = nullptr;
    }

    if (_mixer != nullptr)
    {
        delete _mixer;
        _mixer = nullptr;
    }

    for (int i = 0; i < sizeof(_buffers) / sizeof(_buffers[0]); ++i)
    {
        free(_buffers[i].buf);
    }
}

bool AudioFlinger::init()
{
    _mixer = new (std::nothrow) AudioMixer(_bufferSizeInFrames, _sampleRate);
    _mixingThread = new (std::nothrow) std::thread(&AudioFlinger::mixingThreadLoop, this);
    return true;
}

bool AudioFlinger::addTrack(Track* track)
{
    bool ret = false;

    std::lock_guard<std::mutex> lk(_activeTracksMutex);

    auto iter = std::find(_activeTracks.begin(), _activeTracks.end(), track);
    if (iter == _activeTracks.end())
    {
        _activeTracks.push_back(track);
        _mixingCondition.notify_one();
        ret = true;
    }

    return ret;
}

template <typename T>
static void removeItemFromVector(std::vector<T>& v, T item)
{
    auto iter = std::find(v.begin(), v.end(), item);
    if (iter != v.end())
    {
        v.erase(iter);
    }
}

void AudioFlinger::mixingThreadLoop()
{
    auto doWait = [this](){
        std::unique_lock<std::mutex> lk(_mixingMutex);
        _switchMutex.unlock();
        _activeTracksMutex.unlock();

        _mixingCondition.wait(lk);

        _switchMutex.lock();
        _activeTracksMutex.lock();
    };

    for (;;)
    {
        LOGD("AudioFlinger::mixingThreadLoop()");


        _switchMutex.lock();
        _activeTracksMutex.lock();

        if (_isPaused)
        {
            doWait();
        }

        if (_activeTracks.empty() || (_current->state == BufferState::FULL && _next->state == BufferState::FULL))
        {
            doWait();
        }

        if (_isDestroy)
        {
            _switchMutex.unlock();
            _activeTracksMutex.unlock();
            _isDestroy = false;
            return;
        }

        if (_current->state == BufferState ::EMPTY)
        {
            _mixing = _current;
        }
        else if (_next->state == BufferState::EMPTY)
        {
            _mixing = _next;
        }
        _switchMutex.unlock();

        float f = AudioMixer::UNITY_GAIN_FLOAT;// / _activeTracks.size(); // normalize volume by # tracks // FIXME: do paused tracks need to be considered?

        std::vector<Track*> tracksToRemove;
        tracksToRemove.reserve(_activeTracks.size());

        // FOR TESTING BEGIN
//        Track* track = _activeTracks[0];
//
//        AudioBufferProvider::Buffer buffer;
//        buffer.frameCount = _bufferSizeInFrames;
//        status_t r = track->getNextBuffer(&buffer);
////        ALOG_ASSERT(buffer.frameCount == _mixing->size / 2, "buffer.frameCount:%d, _mixing->size/2:%d", buffer.frameCount, _mixing->size/2);
//        if (r == NO_ERROR)
//        {
//            LOGD("getNextBuffer succeed ...");
//            memcpy(_mixing->buf, buffer.raw, _mixing->size);
//        }
//        if (buffer.raw == nullptr)
//        {
//            LOGD("Play over ...");
//            tracksToRemove.push_back(track);
//        }
//        else
//        {
//            track->releaseBuffer(&buffer);
//        }
//
//        _mixing->state = BufferState::FULL;
//        _activeTracksMutex.unlock();
        // FOR TESTING END

        Track::State state;
        // set up the tracks.
        for (Track* track : _activeTracks)
        {
            state = track->getState();

            if (state == Track::State::IDLE)
            {
                uint32_t channelMask = audio_channel_out_mask_from_count(2);
                int32_t name = _mixer->getTrackName(channelMask, AUDIO_FORMAT_PCM_16_BIT,
                                                    AUDIO_SESSION_OUTPUT_MIX);
                ALOG_ASSERT(name >= 0);

                _mixer->setBufferProvider(name, track);
                _mixer->setParameter(name, AudioMixer::TRACK, AudioMixer::MAIN_BUFFER, _mixing->buf);
                _mixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::MIXER_FORMAT,
                        (void *) (uintptr_t) AUDIO_FORMAT_PCM_16_BIT);
                _mixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::FORMAT,
                        (void *) (uintptr_t) AUDIO_FORMAT_PCM_16_BIT);
                _mixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::MIXER_CHANNEL_MASK,
                        (void *) (uintptr_t) channelMask);
                _mixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::CHANNEL_MASK,
                        (void *) (uintptr_t) channelMask);

                _mixer->setParameter(name, AudioMixer::VOLUME, AudioMixer::VOLUME0, &f);
                _mixer->setParameter(name, AudioMixer::VOLUME, AudioMixer::VOLUME1, &f);
                _mixer->enable(name);

                track->setState(Track::State::PLAYING);
                track->setName(name);
            }
            else
            {
                ALOG_ASSERT(track->getName() >= 0);

                if (state == Track::State::PLAYING)
                {
                    _mixer->setParameter(track->getName(), AudioMixer::TRACK, AudioMixer::MAIN_BUFFER, _mixing->buf);
                }
                else if (state == Track::State::RESUMED)
                {
                    _mixer->enable(track->getName());
                    track->setState(Track::State::PLAYING);
                }
                else if (state == Track::State::PAUSED)
                {
                    _mixer->disable(track->getName());
                }
                else if (state == Track::State::STOPPED)
                {
                    _mixer->deleteTrackName(track->getName());
                    tracksToRemove.push_back(track);
                }

                if (track->isPlayOver())
                {
                    LOGD("Play over ...");
                    _mixer->deleteTrackName(track->getName());
                    tracksToRemove.push_back(track);
                }
            }
        }

        bool hasAvailableTracks = _activeTracks.size() - tracksToRemove.size() > 0;
        _activeTracksMutex.unlock();

        if (hasAvailableTracks)
        {
            LOGD("active tracks: %d", (int) _activeTracks.size());
            _mixer->process(AudioBufferProvider::kInvalidPTS);
            _mixing->state = BufferState::FULL;
            LOGD("mixer process end");
        }
        else
        {
            LOGD("Doesn't have enough tracks: %d, %d", (int) _activeTracks.size(), (int) tracksToRemove.size());
        }

        _activeTracksMutex.lock();

        // Remove stopped or playover tracks for active tracks container
        for (Track* track : tracksToRemove)
        {
            removeItemFromVector(_activeTracks, track);
            track->onDestroy();
        }

        _activeTracksMutex.unlock();
    }
}

void AudioFlinger::switchBuffers()
{
    LOGD("AudioFlinger::switchBuffers ...");
    _switchMutex.lock();
    OutputBuffer* tmp = _busy;
    _busy = _current; _busy->state = BufferState::BUSY;
    _current = _next; // Don't change new current state
    _next = tmp; _next->state = BufferState::EMPTY;
    _switchMutex.unlock();

    _mixingCondition.notify_one();
}

void AudioFlinger::destroy()
{
    _isDestroy = true;
    while(_isDestroy)
    {
        _mixingCondition.notify_one();
        usleep(100);
    }
}

bool AudioFlinger::isCurrentBufferFull()
{
    std::lock_guard<std::mutex> lk(_switchMutex);
    return _current->state == BufferState::FULL;
}

bool AudioFlinger::hasActiveTracks()
{
    std::lock_guard<std::mutex> lk(_activeTracksMutex);
    return !_activeTracks.empty();
}

void AudioFlinger::pause()
{
    _isPaused = true;
}

void AudioFlinger::resume()
{
    _isPaused = false;
    _mixingCondition.notify_one();
}

bool AudioFlinger::isAllBuffersFull()
{
    std::lock_guard<std::mutex> lk(_switchMutex);
    return _current->state == BufferState::FULL && _next->state == BufferState::FULL ;
}
} // namespace cocos2d