//
// Created by James Chen on 6/13/16.
//

#ifndef HELLO_JNI_AUDIOPLAYER_H
#define HELLO_JNI_AUDIOPLAYER_H

#include "AudioDecoder.h"

#include <mutex>
#include <condition_variable>

#define AUDIO_PLAYER_BUFFER_COUNT (2)

class AudioPlayer {
public:
    AudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject);
    virtual ~AudioPlayer();

    bool initForPlayPcmData(int numChannels, int sampleRate, int bufferSizeInBytes);

    typedef std::function<int(const std::string&, off_t* start, off_t* length)> FdGetterCallback;
    bool initWithUrl(const std::string& url, float volume, bool loop, const FdGetterCallback& fdGetter);

    void play();
    void playWithPcmData(const AudioDecoder::Result& decResult, float volume, bool loop);
    bool isPlaying();
    void pause();
    void resume();
    void stop();

    inline int getChannelCount() { return _numChannels; };
    inline int getSampleRate() { return _sampleRate; };

private:
    void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq);
    void setOwnedByAudioPlayerPool(bool isOwnedByPool);
private:
    SLEngineItf _engineItf;
    SLObjectItf _outputMixObj;
    int _assetFd;

    AudioDecoder::Result _decResult;

    SLObjectItf _playObj;
    SLPlayItf _playItf;
    SLSeekItf _seekItf;
    SLVolumeItf _volumeItf;
    SLAndroidSimpleBufferQueueItf _bufferQueueItf;

    //FIXME: only used for pcm data player
    int _numChannels;
    int _sampleRate;
    int _bufferSizeInBytes;
    //

    float _volume;
    bool _isLoop;
    bool _isOwnedByPool;
    bool _isPlaying;
    bool _isDestroyed;

    std::mutex _enqueueMutex;
    std::condition_variable _enqueueCond;

    char _silenceData[4096];

    int _currentBufferIndex;

    friend class SLAudioPlayerCallbackProxy;
    friend class AudioPlayerPool;
};


#endif //HELLO_JNI_AUDIOPLAYER_H
