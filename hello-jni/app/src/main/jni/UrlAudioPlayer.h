//
// Created by James Chen on 6/13/16.
//

#ifndef HELLO_JNI_AUDIOPLAYER_H
#define HELLO_JNI_AUDIOPLAYER_H

#include <sys/types.h>
#include <string>
#include <SLES/OpenSLES.h>
#include <functional>

class UrlAudioPlayer {
public:
    UrlAudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject);
    virtual ~UrlAudioPlayer();

    typedef std::function<int(const std::string&, off_t* start, off_t* length)> FdGetterCallback;

    int play(const std::string& url, float volume, bool loop, const FdGetterCallback& fdGetter);

    inline bool isPlaying() { return _isPlaying; };
    void pause();
    void resume();
    void stop();
    void setVolume(float volume);

    float getDuration();
    float getPosition();
    void setPosition(float pos);

    void setPlayOverCallback(const std::function<void()>& playOverCb);

private:
    inline void setPlaying(bool isPlaying) { _isPlaying = isPlaying; };

    void playEventCallback(SLPlayItf caller, SLuint32 playEvent);
private:
    SLEngineItf _engineItf;
    SLObjectItf _outputMixObj;
    int _assetFd;

    SLObjectItf _playObj;
    SLPlayItf _playItf;
    SLSeekItf _seekItf;
    SLVolumeItf _volumeItf;

    float _volume;
    float _duration;
    bool _isLoop;
    bool _isPlaying;
    bool _isDestroyed;

    std::function<void()> _playOverCb;

    friend class SLUrlAudioPlayerCallbackProxy;
};


#endif //HELLO_JNI_AUDIOPLAYER_H
