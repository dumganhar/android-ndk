//
// Created by James Chen on 6/13/16.
//

#ifndef PCMAUDIOPLAYER_H
#define PCMAUDIOPLAYER_H

#include "AudioDecoder.h"

#include <mutex>
#include <condition_variable>


class PcmAudioPlayer
{
public:
    virtual ~PcmAudioPlayer();

    int play(const AudioDecoder::Result& decResult, float volume, bool loop);

    inline bool isPlaying() { return _isPlaying; };
    void pause();
    void resume();
    void stop();
    void setVolume(float volume);

    inline int getChannelCount() { return _numChannels; };
    inline int getSampleRate() { return _sampleRate; };

private:
    PcmAudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject);
    bool initForPlayPcmData(int numChannels, int sampleRate, int bufferSizeInBytes);

    void wait();
    void wakeup();
    void enqueue();

    void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq);

    inline void setOwnedByPool(bool isOwnedByPool) { _isOwnedByPool = isOwnedByPool; };
    inline bool isOwnedByPool() { return _isOwnedByPool; };
    inline void setPlaying(bool isPlaying) { _isPlaying = isPlaying; };

private:
    SLEngineItf _engineItf;
    SLObjectItf _outputMixObj;

    AudioDecoder::Result _decResult;

    SLObjectItf _playObj;
    SLPlayItf _playItf;
    SLVolumeItf _volumeItf;
    SLAndroidSimpleBufferQueueItf _bufferQueueItf;

    int _numChannels;
    int _sampleRate;
    int _bufferSizeInBytes;

    float _volume;
    bool _isLoop;
    bool _isOwnedByPool;
    bool _isPlaying;
    bool _isDestroyed;

    std::mutex _enqueueMutex;
    std::condition_variable _enqueueCond;

    int _currentBufferIndex;

    friend class SLPcmAudioPlayerCallbackProxy;
    friend class PcmAudioPlayerPool;
};


#endif //HELLO_JNI_AUDIOPLAYER_H
