//
// Created by James Chen on 7/5/16.
//

#include "Track.h"

namespace cocos2d {

Track::Track(const PcmData &pcmData)
        : onDestroy(nullptr)
        , _pcmData(pcmData)
        , _state(State::IDLE)
        , _name(-1)
{
    init(_pcmData.pcmBuffer->data(), _pcmData.numFrames, _pcmData.bitsPerSample / 8 * _pcmData.numChannels);
}

Track::~Track()
{

}

gain_minifloat_packed_t Track::getVolumeLR()
{
    return GAIN_MINIFLOAT_PACKED_UNITY;
}

}