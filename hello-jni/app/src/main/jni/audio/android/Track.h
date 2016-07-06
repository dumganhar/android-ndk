//
// Created by James Chen on 7/5/16.
//

#ifndef HELLO_JNI_TRACK_H
#define HELLO_JNI_TRACK_H

#include <stdint.h>
#include "PcmData.h"
#include "FastMixerState.h"

namespace cocos2d {

class AudioFlinger;

class Track : public ExtendedAudioBufferProvider, public RefBase, public VolumeProvider
{
public:

    enum track_state {
        IDLE,
        FLUSHED,
        STOPPED,
        // next 2 states are currently used for fast tracks
        // and offloaded tracks only
                STOPPING_1,     // waiting for first underrun
        STOPPING_2,     // waiting for presentation complete
        RESUMING,
        ACTIVE,
        PAUSING,
        PAUSED,
        STARTING_1,     // for RecordTrack only
        STARTING_2,     // for RecordTrack only
    };


    Track(AudioFlinger* flinger, const PcmData& pcmData, int uid);
    virtual ~Track();

    int uid();

    bool isFastTrack() { return true; };

    bool isPausing() const { return mState == PAUSING; }
    bool isPaused() const { return mState == PAUSED; }
    bool isResuming() const { return mState == RESUMING; }
    bool isReady() const;
    void setPaused() { mState = PAUSED; }

    bool isStopped() const {
        return (mState == STOPPED || mState == FLUSHED);
    }

    // for fast tracks and offloaded tracks only
    bool isStopping() const {
        return mState == STOPPING_1 || mState == STOPPING_2;
    }
    bool isStopping_1() const {
        return mState == STOPPING_1;
    }
    bool isStopping_2() const {
        return mState == STOPPING_2;
    }

    bool isTerminated() const {
        return mTerminated;
    }

    void terminate() {
        mTerminated = true;
    }


    audio_format_t format() const { return mFormat; }

    uint32_t channelCount() const { return mChannelCount; }

    audio_channel_mask_t channelMask() const { return mChannelMask; }

    virtual uint32_t sampleRate() const { return mSampleRate; }

    void reset() {};

private:
    // FILLED state is used for suppressing volume ramp at begin of playing
    enum {FS_INVALID, FS_FILLING, FS_FILLED, FS_ACTIVE};
    mutable uint8_t     mFillingUpStatus;

    const PcmData& sharedBuffer();

    const PcmData& _pcmData;
    int _uid;

    // The following fields are only for fast tracks, and should be in a subclass
    int mFastIndex; // index within FastMixerState::mFastTracks[];
    // either mFastIndex == -1 if not isFastTrack()
    // or 0 < mFastIndex < FastMixerState::kMaxFast because
    // index 0 is reserved for normal mixer's submix;
    // index is allocated statically at track creation time
    // but the slot is only used if track is active

    // we don't really need a lock for these
    track_state         mState;

    bool                mTerminated;

    const uint32_t      mSampleRate;    // initial sample rate only; for tracks which
    // support dynamic rates, the current value is in control block
    const audio_format_t mFormat;
    const audio_channel_mask_t mChannelMask;
    const uint32_t      mChannelCount;
    const size_t        mFrameSize; // AudioFlinger's view of frame size in shared memory,
    // where for AudioTrack (but not AudioRecord),
    // 8-bit PCM samples are stored as 16-bit
    const size_t        mFrameCount;// size of track buffer given at createTrack() or
    // openRecord(), and then adjusted as needed

    friend class AudioFlinger;
};

} // namespace cocos2d {

#endif //HELLO_JNI_TRACK_H
