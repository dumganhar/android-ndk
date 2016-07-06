//
// Created by James Chen on 7/5/16.
//

#include "PcmSource.h"

namespace cocos2d {

PcmSource::PcmSource(const NBAIO_Format &format)
: NBAIO_Source(format)
{

}
size_t PcmSource::framesOverrun()
{
    return 0;
}

size_t PcmSource::overruns()
{
    return 0;
}

ssize_t PcmSource::availableToRead()
{
    return 0;
}

ssize_t PcmSource::read(void *buffer, size_t count, int64_t readPTS)
{
    return 0;
}

} // namespace cocos2d {