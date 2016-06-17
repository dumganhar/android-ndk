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

#ifndef COCOS_PCMDATA_H
#define COCOS_PCMDATA_H

#include <stdio.h>
#include <string>
#include <vector>
#include <memory>

struct PcmData
{
    std::shared_ptr<std::vector<char>> pcmBuffer;
    int numChannels;
    int sampleRate;
    int bitsPerSample;
    int containerSize;
    int channelMask;
    int endianness;
    int numFrames;

    PcmData()
    {
        reset();
    }

    inline void reset()
    {
        numChannels = -1;
        sampleRate = -1;
        bitsPerSample = -1;
        containerSize = -1;
        channelMask = -1;
        endianness = -1;
        numFrames = -1;
        pcmBuffer = nullptr;
    }

    inline bool isValid()
    {
        return numChannels > 0 && sampleRate > 0 && bitsPerSample > 0 && containerSize > 0 && numFrames > 0 && pcmBuffer != nullptr;
    }

    inline std::string toString()
    {
        std::string ret;
        char buf[256] = {0};

        snprintf(buf, sizeof(buf),
                 "numChannels: %d, sampleRate: %d, bitPerSample: %d, containerSize: %d, channelMask: %d, endianness: %d, numFrames: %d",
                 numChannels, sampleRate, bitsPerSample, containerSize, channelMask, endianness, numFrames
        );

        ret = buf;
        return ret;
    }
};

#endif //COCOS_PCMDATA_H
