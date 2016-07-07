//
// Created by James Chen on 7/5/16.
//

#ifndef COCOS_AUDIOFLINGER_H
#define COCOS_AUDIOFLINGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <audio/android/utils/Errors.h>

namespace cocos2d {

class Track;
class AudioMixer;

class AudioFlinger
{
public:
    AudioFlinger(int bufferSizeInFrames, int sampleRate, int channelCount);

    ~AudioFlinger();

    bool init();

    bool addTrack(Track *track);
    void switchBuffers();
    bool hasActiveTracks();
    bool isCurrentBufferFull();
    bool isAllBuffersFull();
    bool hasPlayingTacks();

    void pause();
    void resume();
    inline bool isPaused() const { return _isPaused; };

    enum class BufferState
    {
        BUSY,
        EMPTY,
        FULL
    };

    struct OutputBuffer
    {
        void* buf;
        size_t size;
        BufferState state;
    };

    inline OutputBuffer* current() { return _current; }

private:
    void destroy();
    void mixingThreadLoop();

private:
    int _bufferSizeInFrames;
    int _sampleRate;
    int _channelCount;

    std::thread* _mixingThread;

    std::mutex _mixingMutex;
    std::condition_variable _mixingCondition;

    AudioMixer* _mixer;

    std::mutex _activeTracksMutex;
    std::vector<Track*> _activeTracks;
    std::mutex _switchMutex;

    OutputBuffer _buffers[4];
    OutputBuffer* _busy;
    OutputBuffer* _current;
    OutputBuffer* _next;
    OutputBuffer* _afterNext;

    OutputBuffer* _mixing;

    std::atomic_bool _isDestroy;
    std::atomic_bool _isPaused;
};

} // namespace cocos2d {

#endif //COCOS_AUDIOFLINGER_H
