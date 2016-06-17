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

#ifndef COCOS_IAUDIOPLAYER_H
#define COCOS_IAUDIOPLAYER_H

#include <functional>

class IAudioPlayer
{
public:
    virtual ~IAudioPlayer() {};

    virtual int getId() = 0;
    virtual void setId(int id) = 0;

    virtual std::string getUrl() = 0;

    virtual void play() = 0;
    virtual bool isPlaying() = 0;

    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    virtual void rewind() = 0;

    virtual void setVolume(float volume) = 0;
    virtual float getVolume() = 0;

    virtual void setLoop(bool isLoop) = 0;
    virtual bool isLoop() = 0;

    virtual float getDuration() = 0;
    virtual float getPosition() = 0;
    virtual bool setPosition(float pos) = 0;

    typedef std::function<void(IAudioPlayer*, void*)> PlayOverCallback;

    // @note: Play over callback will be invoked in sub thread
    virtual void setPlayOverCallback(const PlayOverCallback& playOverCallback, void* context) = 0;

    virtual bool isOwnedByPool() = 0;
};

#endif //COCOS_IAUDIOPLAYER_H
