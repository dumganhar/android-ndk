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
#ifndef COCOS_AUDIOPLAYERPOOL_H
#define COCOS_AUDIOPLAYERPOOL_H

#include "audio/android/OpenSLHelper.h"

#include <vector>

class PcmAudioPlayer;
class ThreadPool;

class PcmAudioPlayerPool
{
public:
    PcmAudioPlayerPool(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int deviceBufferSizeInFrames);
    virtual ~PcmAudioPlayerPool();

    PcmAudioPlayer* findAvailablePlayer(int numChannels);
    void prepareEnoughPlayers();
    void releaseUnusedPlayers();

private:
    PcmAudioPlayer* createPlayer(int numChannels);

private:
    SLEngineItf _engineItf;
    SLObjectItf _outputMixObject;
    int _deviceSampleRate;
    int _deviceBufferSizeInFrames;
    std::vector<PcmAudioPlayer*> _audioPlayerPool;
    ThreadPool* _threadPool;
};


#endif //COCOS_AUDIOPLAYERPOOL_H
