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

#ifndef COCOS_PCMAUDIOPLAYER_H
#define COCOS_PCMAUDIOPLAYER_H

#include "IAudioPlayer.h"
#include "OpenSLHelper.h"
#include "PcmData.h"

#include <mutex>
#include <condition_variable>

class PcmAudioPlayer : public IAudioPlayer
{
public:
    // Override Functions Begin
    virtual int getId() override { return _id; };
    virtual void setId(int id) override { _id = id; };
    virtual std::string getUrl() override { return _url; };

    virtual void play() override;
    virtual bool isPlaying() override { return _isPlaying; };

    virtual void pause() override;
    virtual void resume() override;
    virtual void stop() override;
    virtual void rewind() override;

    virtual void setVolume(float volume) override;
    virtual float getVolume() override;

    virtual void setLoop(bool isLoop) override;
    virtual bool isLoop() override;

    virtual float getDuration() override;
    virtual float getPosition() override;
    virtual bool setPosition(float pos) override;

    virtual void setPlayOverCallback(const PlayOverCallback& playOverCb, void* context) override;

    virtual bool isOwnedByPool() override { return true; };
    // Override Functions End

    inline int getChannelCount() { return _numChannels; };
    inline int getSampleRate() { return _sampleRate; };

private:
    PcmAudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject);
    virtual ~PcmAudioPlayer();

    bool initForPlayPcmData(int numChannels, int sampleRate, int bufferSizeInBytes);

    bool prepare(const std::string& url, const PcmData& decResult);

    void wait();
    void wakeup();
    void enqueue();

    void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq);

    inline void setPlaying(bool isPlaying) { _isPlaying = isPlaying; };

private:
    SLEngineItf _engineItf;
    SLObjectItf _outputMixObj;

    int _id;
    std::string _url;
    PcmData _decResult;

    SLObjectItf _playObj;
    SLPlayItf _playItf;
    SLVolumeItf _volumeItf;
    SLAndroidSimpleBufferQueueItf _bufferQueueItf;

    int _numChannels;
    int _sampleRate;
    int _bufferSizeInBytes;

    float _volume;
    bool _isLoop;
    bool _isPlaying;
    bool _isDestroyed;

    std::mutex _enqueueMutex;
    std::condition_variable _enqueueCond;

    int _currentBufferIndex;
    PlayOverCallback _playOverCallback;
    void* _PlayOverCallbackContext;

    friend class SLPcmAudioPlayerCallbackProxy;
    friend class PcmAudioPlayerPool;
    friend class AudioPlayerProvider;
};


#endif //COCOS_PCMAUDIOPLAYER_H
