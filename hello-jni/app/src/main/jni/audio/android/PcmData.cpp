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

#define LOG_TAG "PcmData"

#include "audio/android/OpenSLHelper.h"

#include "audio/android/PcmData.h"

PcmData::PcmData()
{
//    LOGD("In the constructor of PcmData (%p)", this);
    reset();
}

PcmData::~PcmData()
{
//    LOGD("In the destructor of PcmData (%p)", this);
}

PcmData::PcmData(const PcmData& o)
{
//    LOGD("In the copy constructor of PcmData (%p)", this);
    numChannels = o.numChannels;
    sampleRate = o.sampleRate;
    bitsPerSample = o.bitsPerSample;
    containerSize = o.containerSize;
    channelMask = o.channelMask;
    endianness = o.endianness;
    numFrames = o.numFrames;
    pcmBuffer = std::move(o.pcmBuffer);
}

PcmData::PcmData(PcmData&& o)
{
//    LOGD("In the move constructor of PcmData (%p)", this);
    numChannels = o.numChannels;
    sampleRate = o.sampleRate;
    bitsPerSample = o.bitsPerSample;
    containerSize = o.containerSize;
    channelMask = o.channelMask;
    endianness = o.endianness;
    numFrames = o.numFrames;
    pcmBuffer = std::move(o.pcmBuffer);
    o.reset();
}

PcmData& PcmData::operator= (const PcmData& o)
{
//    LOGD("In the copy assignment of PcmData");
    numChannels = o.numChannels;
    sampleRate = o.sampleRate;
    bitsPerSample = o.bitsPerSample;
    containerSize = o.containerSize;
    channelMask = o.channelMask;
    endianness = o.endianness;
    numFrames = o.numFrames;
    pcmBuffer = o.pcmBuffer;
    return *this;
}

PcmData& PcmData::operator= (PcmData&& o)
{
//    LOGD("In the move assignment of PcmData");
    numChannels = o.numChannels;
    sampleRate = o.sampleRate;
    bitsPerSample = o.bitsPerSample;
    containerSize = o.containerSize;
    channelMask = o.channelMask;
    endianness = o.endianness;
    numFrames = o.numFrames;
    pcmBuffer = o.pcmBuffer;
    o.reset();
    return *this;
}

void PcmData::reset()
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

bool PcmData::isValid() const
{
    return numChannels > 0 && sampleRate > 0 && bitsPerSample > 0 && containerSize > 0 && numFrames > 0 && pcmBuffer != nullptr;
}

std::string PcmData::toString() const
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
