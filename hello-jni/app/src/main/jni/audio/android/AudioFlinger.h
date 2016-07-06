//
// Created by James Chen on 7/5/16.
//

#ifndef COCOS_AUDIOFLINGER_H
#define COCOS_AUDIOFLINGER_H

#include "utils/StrongPointer.h"
#include "utils/SortedVector.h"

#include "FastMixer.h"
#include "Track.h"

namespace cocos2d {

class AudioFlinger
{
public:
    AudioFlinger();

    ~AudioFlinger();

    bool init(const NBAIO_Format& format);

    enum mixer_state {
        MIXER_IDLE,             // no active tracks
        MIXER_TRACKS_ENABLED,   // at least one active track, but no track has any data ready
        MIXER_TRACKS_READY,      // at least one active track, and at least one track has data
        MIXER_DRAIN_TRACK,      // drain currently playing track
        MIXER_DRAIN_ALL,        // fully drain the hardware
        // standby mode does not have an enum value
        // suspend by audio policy manager is orthogonal to mixer state
    };

    mixer_state prepareTracks(Vector<sp<Track>>* tracksToRemove);

    status_t addTrack(const sp<Track>& track);
    void removeTrack(const sp<Track>& track);
    void removeTracks(const Vector< sp<Track> >& tracksToRemove);

private:

    sp<FastMixer> mFastMixer;     // non-0 if there is also a fast mixer

    // accessible only within the threadLoop(), no locks required
    //          mFastMixer->sq()    // for mutating and pushing state
    int32_t     mFastMixerFutex;    // for cold idle
// contents are not guaranteed to be consistent, no locks required
    FastMixerDumpState mFastMixerDumpState;


    uint32_t                mSampleRate;
    size_t                  mFrameCount;       // output HAL, direct output, record
    audio_channel_mask_t    mChannelMask;
    uint32_t                mChannelCount;
    size_t                  mFrameSize;
    // not HAL frame size, this is for output sink (to pipe to fast mixer)
    audio_format_t          mFormat;           // Source format for Recording and
    // Sink format for Playback.
    // Sink format may be different than
    // HAL format if Fastmixer is used.


    // The HAL output sink is treated as non-blocking, but current implementation is blocking
    sp<NBAIO_Sink>          mOutputSink;

    SortedVector< wp<Track> >       mActiveTracks;  // FIXME check if this could be sp<>
    SortedVector<int>               mWakeLockUids;
    int                             mActiveTracksGeneration;
    wp<Track>                       mLatestActiveTrack; // latest track added to mActiveTracks

    unsigned    mFastTrackAvailMask;    // bit i set if fast track [i] is available

    friend class Track;
};

} // namespace cocos2d {

#endif //COCOS_AUDIOFLINGER_H
