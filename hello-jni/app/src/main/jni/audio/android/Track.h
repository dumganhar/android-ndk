//
// Created by James Chen on 7/5/16.
//

#ifndef HELLO_JNI_TRACK_H
#define HELLO_JNI_TRACK_H

#include "audio/android/PcmData.h"
#include "audio/android/IVolumeProvider.h"
#include "audio/android/PcmBufferProvider.h"

namespace cocos2d {

class AudioFlinger;

class Track : public PcmBufferProvider, public IVolumeProvider
{
public:
    enum class State
    {
        IDLE,
        PLAYING,
        RESUMED,
        PAUSED,
        STOPPED,
        OVER,
        DESTROYED
    };

    Track(const PcmData &pcmData);
    virtual ~Track();

    inline State getState() const { return _state; };
    inline void setState(State state)
    {
        if (_state != state)
        {
            _state = state;
            onStateChanged(_state);
        }
    };

    inline bool isPlayOver() const { return _state == State::PLAYING && mNextFrame >= mNumFrames;};
    inline void setName(int name) { _name = name; };
    inline int getName() const { return _name; };

    virtual gain_minifloat_packed_t getVolumeLR() override ;

    std::function<void(State)> onStateChanged;

private:
    const PcmData& _pcmData;
    State _state;
    int _name;

    friend class AudioFlinger;
};

} // namespace cocos2d {

#endif //HELLO_JNI_TRACK_H
