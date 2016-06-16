//
// Created by James Chen on 6/15/16.
//

#include <android/log.h>
#include "BufferProvider.h"

#define LOG_TAG "cjh"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

namespace cocos2d {

    static int gVerbose = 1;

    BufferProvider::BufferProvider(const void *addr, size_t frames, size_t frameSize)
            : mAddr(addr),
              mNumFrames(frames),
              mFrameSize(frameSize),
              mNextFrame(0), mUnrel(0)
    {
    }

    status_t BufferProvider::getNextBuffer(Buffer *buffer,
                                   int64_t pts/* = kInvalidPTS*/) {
        (void) pts; // suppress warning
        size_t requestedFrames = buffer->frameCount;
        if (requestedFrames > mNumFrames - mNextFrame) {
            buffer->frameCount = mNumFrames - mNextFrame;
        }

        if (gVerbose) {
            LOGD("getNextBuffer() requested %zu frames out of %zu frames available,"
                           " and returned %zu frames\n",
                   requestedFrames, (size_t) (mNumFrames - mNextFrame), buffer->frameCount);
        }
        mUnrel = buffer->frameCount;
        if (buffer->frameCount > 0) {
            buffer->raw = (char *) mAddr + mFrameSize * mNextFrame;
            return NO_ERROR;
        } else {
            buffer->raw = NULL;
            return NOT_ENOUGH_DATA;
        }
    }

    void BufferProvider::releaseBuffer(Buffer *buffer) {
        if (buffer->frameCount > mUnrel) {
            LOGE("ERROR releaseBuffer() released %zu frames but only %zu available "
                    "to release\n", buffer->frameCount, mUnrel);
            mNextFrame += mUnrel;
            mUnrel = 0;
        } else {
            if (gVerbose) {
                LOGD("releaseBuffer() released %zu frames out of %zu frames available "
                               "to release\n", buffer->frameCount, mUnrel);
            }
            mNextFrame += buffer->frameCount;
            mUnrel -= buffer->frameCount;
        }
        buffer->frameCount = 0;
        buffer->raw = NULL;
    }

    void BufferProvider::reset() {
        mNextFrame = 0;
    }

}