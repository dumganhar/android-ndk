//
// Created by James Chen on 7/5/16.
//

#ifndef HELLO_JNI_PCMSOURCE_H
#define HELLO_JNI_PCMSOURCE_H

#include "NBAIO.h"

namespace cocos2d {

class PcmSource : public NBAIO_Source
{
public:
    PcmSource(const NBAIO_Format& format = Format_Invalid);
    virtual size_t framesOverrun() /*const*/ override ;
    virtual size_t overruns() /*const*/ override ;
    virtual ssize_t availableToRead() override ;
    virtual ssize_t read(void *buffer, size_t count, int64_t readPTS) override ;
};

} // namespace cocos2d

#endif //HELLO_JNI_PCMSOURCE_H
