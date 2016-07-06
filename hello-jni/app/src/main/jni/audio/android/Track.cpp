//
// Created by James Chen on 7/5/16.
//

#include "Track.h"
#include "AudioFlinger.h"


namespace cocos2d {

Track::Track(AudioFlinger* flinger, const PcmData &pcmData, int uid)
        : _pcmData(pcmData)
        , _uid(uid)
        , mFormat(AUDIO_FORMAT_PCM_16_BIT)
        , mChannelMask(AUDIO_CHANNEL_OUT_STEREO)
        , mChannelCount(pcmData.numChannels)
        , mFrameSize(pcmData.bitsPerSample * 8)
        , mSampleRate(pcmData.sampleRate)
        , mFrameCount(pcmData.numFrames)
{
// FIXME: Not calling framesReadyIsCalledByMultipleThreads() exposes a potential
    // race with setSyncEvent(). However, if we call it, we cannot properly start
    // static fast tracks (SoundPool) immediately after stopping.
    //mAudioTrackServerProxy->framesReadyIsCalledByMultipleThreads();
    ALOG_ASSERT(flinger->mFastTrackAvailMask != 0);
    int i = __builtin_ctz(flinger->mFastTrackAvailMask);
    ALOG_ASSERT(0 < i && i < (int)FastMixerState::kMaxFastTracks);
    // FIXME This is too eager.  We allocate a fast track index before the
    //       fast track becomes active.  Since fast tracks are a scarce resource,
    //       this means we are potentially denying other more important fast tracks from
    //       being created.  It would be better to allocate the index dynamically.
    mFastIndex = i;
    flinger->mFastTrackAvailMask &= ~(1 << i);
}

Track::~Track()
{

}

const PcmData& Track::sharedBuffer()
{
    return _pcmData;
}

int Track::uid()
{
    return _uid;
}


}