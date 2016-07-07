//
// Created by James Chen on 7/6/16.
//

#ifndef HELLO_JNI_IVOLUMEPROVIDER_H
#define HELLO_JNI_IVOLUMEPROVIDER_H

#include "audio_utils/minifloat.h"

class IVolumeProvider {
public:
    // The provider implementation is responsible for validating that the return value is in range.
    virtual gain_minifloat_packed_t getVolumeLR() = 0;
protected:
    IVolumeProvider() { }
    virtual ~IVolumeProvider() { }
};

#endif //HELLO_JNI_IVOLUMEPROVIDER_H
