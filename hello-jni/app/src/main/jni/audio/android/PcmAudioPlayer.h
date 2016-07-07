//
// Created by James Chen on 7/4/16.
//

#ifndef HELLO_JNI_PCMAUDIOPLAYER_H
#define HELLO_JNI_PCMAUDIOPLAYER_H


#include <mutex>
#include "audio/android/IAudioPlayer.h"
#include "audio/android/PcmData.h"
#include "audio/android/Track.h"

namespace cocos2d {

class AudioFinger;

class PcmAudioPlayer : public IAudioPlayer
{
public:
    PcmAudioPlayer(AudioFlinger* flinger);
    virtual ~PcmAudioPlayer();

    bool prepare(const std::string &url, const PcmData &decResult);

    // Override Functions Begin
    virtual int getId() const override { return _id; };

    virtual void setId(int id) override { _id = id; };

    virtual std::string getUrl() const override { return _url; };

    virtual State getState() const override { return _state; };

    virtual void play() override;

    virtual void pause() override;

    virtual void resume() override;

    virtual void stop() override;

    virtual void rewind() override;

    virtual void setVolume(float volume) override;

    virtual float getVolume() const override;

    virtual void setLoop(bool isLoop) override;

    virtual bool isLoop() const override;

    virtual float getDuration() const override;

    virtual float getPosition() const override;

    virtual bool setPosition(float pos) override;

    virtual void setPlayEventCallback(const PlayEventCallback &playEventCallback) override;

    // Override Functions End



private:

    void setState(State state);

private:
    int _id;
    std::string _url;
    PcmData _decResult;

    float _volume;
    bool _isLoop;
    State _state;

    std::mutex _stateMutex;

    Track* _track;

    PlayEventCallback _playEventCallback;

    AudioFlinger* _audioFlinger;
};

} //namespace cocos2d {

#endif //HELLO_JNI_PCMAUDIOPLAYER_H
