//
// Created by James Chen on 7/5/16.
//

#ifndef HELLO_JNI_PCMSINK_H
#define HELLO_JNI_PCMSINK_H

#include "NBAIO.h"

namespace cocos2d {

class PcmSink : public NBAIO_Sink
{
public:
    virtual size_t framesUnderrun() const override ;
    virtual size_t underruns() const override ;
    virtual ssize_t availableToWrite() const override;
    virtual ssize_t write(const void *buffer, size_t count) override ;
};

} // namespace cocos2d {

#endif //HELLO_JNI_PCMSINK_H
