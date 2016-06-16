//
// Created by James Chen on 6/15/16.
//

#ifndef HELLO_JNI_BUFFERPROVIDER_H
#define HELLO_JNI_BUFFERPROVIDER_H

#include "AudioBufferProvider.h"

#include <stddef.h>
#include <stdio.h>

namespace cocos2d {

    class BufferProvider : public AudioBufferProvider {

    public:
        BufferProvider(const void *addr, size_t frames, size_t frameSize);
        virtual status_t getNextBuffer(Buffer *buffer, int64_t pts = kInvalidPTS);
        virtual void releaseBuffer(Buffer *buffer);
        void reset();

    private:
        const void *mAddr;      // base address
        const size_t mNumFrames; // total frames
        const size_t mFrameSize; // size of each frame in bytes
        size_t mNextFrame; // index of next frame to provide
        size_t mUnrel;     // number of frames not yet released
    };
}

#endif //HELLO_JNI_BUFFERPROVIDER_H
