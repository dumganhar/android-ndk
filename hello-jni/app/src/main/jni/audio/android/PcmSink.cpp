//
// Created by James Chen on 7/5/16.
//

#include "PcmSink.h"

namespace cocos2d {


size_t PcmSink::framesUnderrun() const
{
    return 0;
}

size_t PcmSink::underruns() const
{
    return 0;
}

ssize_t PcmSink::availableToWrite() const
{
    return 0;
}

ssize_t PcmSink::write(const void *buffer, size_t count)
{
    return 0;
}

} // namespace cocos2d {