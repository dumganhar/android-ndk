//
// Created by James Chen on 7/5/16.
//


#include "AudioFlinger.h"
#include "SourceAudioBufferProvider.h"
#include "PcmSource.h"
#include "PcmSink.h"

#include <linux/futex.h>
#include <asm/unistd.h>
#include <sys/syscall.h>
#include <algorithm>

namespace cocos2d {

AudioFlinger::AudioFlinger()
: mFastTrackAvailMask(((1 << FastMixerState::kMaxFastTracks) - 1) & ~1) // index 0 is reserved for normal mixer's submix
{

}

AudioFlinger::~AudioFlinger()
{
    if (mFastMixer != 0) {
        FastMixerStateQueue *sq = mFastMixer->sq();
        FastMixerState *state = sq->begin();
        if (state->mCommand == FastMixerState::COLD_IDLE) {
            int32_t old = android_atomic_inc(&mFastMixerFutex);
            if (old == -1) {
                (void) syscall(__NR_futex, &mFastMixerFutex, FUTEX_WAKE_PRIVATE, 1);
            }
        }
        state->mCommand = FastMixerState::EXIT;
        sq->end();
        sq->push(FastMixerStateQueue::BLOCK_UNTIL_PUSHED);
        mFastMixer->join();
        // Though the fast mixer thread has exited, it's state queue is still valid.
        // We'll use that extract the final state which contains one remaining fast track
        // corresponding to our sub-mix.
        state = sq->begin();
        ALOG_ASSERT(state->mTrackMask == 1);
        FastTrack *fastTrack = &state->mFastTracks[0];
        ALOG_ASSERT(fastTrack->mBufferProvider != NULL);
        delete fastTrack->mBufferProvider;
        sq->end(false /*didModify*/);
        delete mFastMixer;
#ifdef AUDIO_WATCHDOG
        if (mAudioWatchdog != 0) {
            mAudioWatchdog->requestExit();
            mAudioWatchdog->requestExitAndWait();
            mAudioWatchdog.clear();
        }
#endif
    }
}

bool AudioFlinger::init(const NBAIO_Format& format)
{
    // create an NBAIO sink for the HAL output stream, and negotiate
    mOutputSink = std::make_shared<PcmSink>();

    // create fast mixer and configure it initially with just one fast track for our submix
    mFastMixer = new FastMixer();
    FastMixerStateQueue *sq = mFastMixer->sq();
#ifdef STATE_QUEUE_DUMP
    sq->setObserverDump(&mStateQueueObserverDump);
        sq->setMutatorDump(&mStateQueueMutatorDump);
#endif
    FastMixerState *state = sq->begin();
    FastTrack *fastTrack = &state->mFastTracks[0];
    // wrap the source side of the MonoPipe to make it an AudioBufferProvider
    auto source = std::make_shared<PcmSource>(format);
    fastTrack->mBufferProvider = new SourceAudioBufferProvider(source);
    fastTrack->mVolumeProvider = NULL;
    fastTrack->mChannelMask = mChannelMask; // mPipeSink channel mask for audio to FastMixer
    fastTrack->mFormat = mFormat; // mPipeSink format for audio to FastMixer
    fastTrack->mGeneration++;
    state->mFastTracksGen++;
    state->mTrackMask = 1;
    // fast mixer will use the HAL output sink
    state->mOutputSink = mOutputSink.get();
    state->mOutputSinkGen++;
    state->mFrameCount = mFrameCount;
    state->mCommand = FastMixerState::COLD_IDLE;
    // already done in constructor initialization list
    //mFastMixerFutex = 0;
    state->mColdFutexAddr = &mFastMixerFutex;
    state->mColdGen++;
    state->mDumpState = &mFastMixerDumpState;
#ifdef TEE_SINK
    state->mTeeSink = mTeeSink.get();
#endif
//cjh    mFastMixerNBLogWriter = audioFlinger->newWriter_l(kFastMixerLogSize, "FastMixer");
//    state->mNBLogWriter = mFastMixerNBLogWriter.get();
    sq->end();
    sq->push(FastMixerStateQueue::BLOCK_UNTIL_PUSHED);

    // start the fast mixer
    mFastMixer->run();
//    pid_t tid = mFastMixer->getTid();
//    sendPrioConfigEvent(getpid_cached, tid, kPriorityFastMixer);

    return true;
}

AudioFlinger::mixer_state AudioFlinger::prepareTracks(std::vector<Track*>* tracksToRemove)
{
    mixer_state mixerStatus = MIXER_IDLE;
    size_t count = mActiveTracks.size();

    uint32_t resetMask = 0; // bit mask of fast tracks that need to be reset

    // prepare a new state to push
    FastMixerStateQueue *sq = NULL;
    FastMixerState *state = NULL;
    bool didModify = false;
    FastMixerStateQueue::block_t block = FastMixerStateQueue::BLOCK_UNTIL_PUSHED;
    if (mFastMixer != 0) {
        sq = mFastMixer->sq();
        state = sq->begin();
    }

    for (size_t i=0 ; i<count ; i++) {
        Track* t = mActiveTracks[i];
        if (t == nullptr) {
            continue;
        }

        // this const just means the local variable doesn't change
        Track* track = t;

        // process fast tracks
        if (track->isFastTrack()) {

            // It's theoretically possible (though unlikely) for a fast track to be created
            // and then removed within the same normal mix cycle.  This is not a problem, as
            // the track never becomes active so it's fast mixer slot is never touched.
            // The converse, of removing an (active) track and then creating a new track
            // at the identical fast mixer slot within the same normal mix cycle,
            // is impossible because the slot isn't marked available until the end of each cycle.
            int j = track->mFastIndex;
            ALOG_ASSERT(0 < j && j < (int)FastMixerState::kMaxFastTracks);
            ALOG_ASSERT(!(mFastTrackAvailMask & (1 << j)));
            FastTrack *fastTrack = &state->mFastTracks[j];

            // Determine whether the track is currently in underrun condition,
            // and whether it had a recent underrun.
            FastTrackDump *ftDump = &mFastMixerDumpState.mTracks[j];
            FastTrackUnderruns underruns = ftDump->mUnderruns;
            uint32_t recentFull = 1;
            uint32_t recentPartial = 0;
            uint32_t recentEmpty = 0;

            uint32_t recentUnderruns = recentPartial + recentEmpty;

            // This is similar to the state machine for normal tracks,
            // with a few modifications for fast tracks.
            bool isActive = true;
            switch (track->mState) {
                case Track::STOPPING_1:
                    // track stays active in STOPPING_1 state until first underrun
                    if (recentUnderruns > 0 || track->isTerminated()) {
                        track->mState = Track::STOPPING_2;
                    }
                    break;
                case Track::PAUSING:
                    // ramp down is not yet implemented
                    track->setPaused();
                    break;
                case Track::RESUMING:
                    // ramp up is not yet implemented
                    track->mState = Track::ACTIVE;
                    break;
                case Track::ACTIVE:
                    if (recentUnderruns == 0) {
                        // no recent underruns: stay active
                        break;
                    }
                    // fall through
                case Track::STOPPING_2:
                case Track::PAUSED:
                case Track::STOPPED:
                case Track::FLUSHED:   // flush() while active
                    // Check for presentation complete if track is inactive
                    // We have consumed all the buffers of this track.
                    // This would be incomplete if we auto-paused on underrun
                {

                }
                    if (track->isStopping_2()) {
                        track->mState = Track::STOPPED;
                    }
                    if (track->isStopped()) {
                        // Can't reset directly, as fast mixer is still polling this track
                        //   track->reset();
                        // So instead mark this track as needing to be reset after push with ack
                        resetMask |= 1 << i;
                    }
                    isActive = false;
                    break;
                case Track::IDLE:
                default:
                    LOG_ALWAYS_FATAL("unexpected track state %d", track->mState);
            }

            if (isActive) {
                // was it previously inactive?
                if (!(state->mTrackMask & (1 << j))) {
                    ExtendedAudioBufferProvider *eabp = track;
                    VolumeProvider *vp = track;
                    fastTrack->mBufferProvider = eabp;
                    fastTrack->mVolumeProvider = vp;
                    fastTrack->mChannelMask = track->mChannelMask;
                    fastTrack->mFormat = track->mFormat;
                    fastTrack->mGeneration++;
                    state->mTrackMask |= 1 << j;
                    didModify = true;
                    // no acknowledgement required for newly active tracks
                }
            } else {
                // was it previously active?
                if (state->mTrackMask & (1 << j)) {
                    fastTrack->mBufferProvider = NULL;
                    fastTrack->mGeneration++;
                    state->mTrackMask &= ~(1 << j);
                    didModify = true;
                    // If any fast tracks were removed, we must wait for acknowledgement
                    // because we're about to decrement the last sp<> on those tracks.
                    block = FastMixerStateQueue::BLOCK_UNTIL_ACKED;
                } else {
                    LOG_ALWAYS_FATAL("fast track %d should have been active", j);
                }
                tracksToRemove->push_back(track);
            }
            continue;
        }


        track_is_ready: ;

    }

    // Push the new FastMixer state if necessary
    bool pauseAudioWatchdog = false;
    if (didModify) {
        state->mFastTracksGen++;
        // if the fast mixer was active, but now there are no fast tracks, then put it in cold idle
//cjh        if (kUseFastMixer == FastMixer_Dynamic &&
//            state->mCommand == FastMixerState::MIX_WRITE && state->mTrackMask <= 1) {
//            state->mCommand = FastMixerState::COLD_IDLE;
//            state->mColdFutexAddr = &mFastMixerFutex;
//            state->mColdGen++;
//            mFastMixerFutex = 0;
//            // If we go into cold idle, need to wait for acknowledgement
//            // so that fast mixer stops doing I/O.
//            block = FastMixerStateQueue::BLOCK_UNTIL_ACKED;
//            pauseAudioWatchdog = true;
//        }
    }
    if (sq != NULL) {
        sq->end(didModify);
        sq->push(block);
    }
#ifdef AUDIO_WATCHDOG
    if (pauseAudioWatchdog && mAudioWatchdog != 0) {
        mAudioWatchdog->pause();
    }
#endif

    // Now perform the deferred reset on fast tracks that have stopped
    while (resetMask != 0) {
        size_t i = __builtin_ctz(resetMask);
        ALOG_ASSERT(i < count);
        resetMask &= ~(1 << i);
        Track* t = mActiveTracks[i];
        if (t == nullptr) {
            continue;
        }
        Track* track = t;
        ALOG_ASSERT(track->isFastTrack() && track->isStopped());
        track->reset();
    }

    // remove all the tracks that need to be...
    removeTracks(*tracksToRemove);

    return mixerStatus;
}

status_t AudioFlinger::addTrack(Track* track)
{
    status_t status = ALREADY_EXISTS;

    auto iter = std::find(mActiveTracks.begin(), mActiveTracks.end(), track);
    if (iter == mActiveTracks.end())
    {
        mActiveTracks.push_back(track);
        mWakeLockUids.push_back(track->uid());
        mActiveTracksGeneration++;
        mLatestActiveTrack = track;

        status = NO_ERROR;
    }

//cjh    onAddNewTrack_l();

    return status;
}

template <typename T>
static void removeItemFromVector(std::vector<T>& v, T item)
{
    auto iter = std::find(v.begin(), v.end(), item);
    if (iter != v.end())
    {
        v.erase(iter);
    }
}

// removeTracks_l() must be called with ThreadBase::mLock held
void AudioFlinger::removeTracks(const std::vector<Track*>& tracksToRemove)
{
    size_t count = tracksToRemove.size();
    if (count > 0)
    {
        for (size_t i=0 ; i<count ; i++) {
            Track* track = tracksToRemove.at(i);
            removeItemFromVector(mActiveTracks, track);
            removeItemFromVector(mWakeLockUids, track->uid());
            mActiveTracksGeneration++;
            ALOGV("removeTracks_l removing track");
            if (track->isTerminated()) {
                removeTrack(track);
            }
        }
    }
}

void AudioFlinger::removeTrack(Track* track)
{
    if (track->isFastTrack()) {
        int index = track->mFastIndex;
        ALOG_ASSERT(0 < index && index < (int)FastMixerState::kMaxFastTracks);
        ALOG_ASSERT(!(mFastTrackAvailMask & (1 << index)));
        mFastTrackAvailMask |= 1 << index;
        // redundant as track is about to be destroyed, for dumpsys only
        track->mFastIndex = -1;
    }
}


} // namespace cocos2d