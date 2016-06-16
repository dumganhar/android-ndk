/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIO_RESAMPLER_H
#define ANDROID_AUDIO_RESAMPLER_H

#include <stdint.h>
#include <sys/types.h>
#include <android/log.h>
#include <sys/system_properties.h>

#include "AudioBufferProvider.h"

//#include <cutils/compiler.h>
//#include <utils/Compat.h>

//#include <media/AudioBufferProvider.h>
//#include <system/audio.h>
#include <assert.h>

#ifndef LOG_TAG
#define LOG_TAG "cjh"
#endif

#define ALOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define ALOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)
#define ALOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG,__VA_ARGS__)
#define LOG_ALWAYS_FATAL_IF(conf, ...) if (conf) { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__); }
#define LOG_ALWAYS_FATAL(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)
#define ALOG_ASSERT assert

#define PROPERTY_VALUE_MAX 256
#define CONSTEXPR constexpr


#ifdef __cplusplus
#   define CC_LIKELY( exp )    (__builtin_expect( !!(exp), true ))
#   define CC_UNLIKELY( exp )  (__builtin_expect( !!(exp), false ))
#else
#   define CC_LIKELY( exp )    (__builtin_expect( !!(exp), 1 ))
#   define CC_UNLIKELY( exp )  (__builtin_expect( !!(exp), 0 ))
#endif

namespace cocos2d {
// ----------------------------------------------------------------------------

    /* Convert a single-precision floating point value to a U4.12 integer value.
 * Rounds to nearest, ties away from 0.
 *
 * Values outside the range [0, 16.0) are properly clamped to [0, 65535]
 * including -Inf and +Inf. NaN values are considered undefined, and behavior may change
 * depending on hardware and future implementation of this function.
 */
    static inline uint16_t u4_12_from_float(float f)
    {
        static const float scale = (float)(1 << 12);
        static const float limpos = 0xffff / scale;
        if (f <= 0.) {
            return 0;
        } else if (f >= limpos) {
            return 0xffff;
        }
        /* integer conversion is through truncation (though int to float is not).
         * ensure that we round to nearest, ties away from 0.
         */
        return f * scale + 0.5;
    }

    /* Convert a single-precision floating point value to a U4.28 integer value.
 * Rounds to nearest, ties away from 0.
 *
 * Values outside the range [0, 16.0] are properly clamped to [0, 4294967295]
 * including -Inf and +Inf. NaN values are considered undefined, and behavior may change
 * depending on hardware and future implementation of this function.
 */
    static inline uint32_t u4_28_from_float(float f)
    {
        static const float scale = (float)(1 << 28);
        static const float limpos = 0xffffffffUL / scale;
        if (f <= 0.) {
            return 0;
        } else if (f >= limpos) {
            return 0xffffffff;
        }
        /* integer conversion is through truncation (though int to float is not).
         * ensure that we round to nearest, ties away from 0.
         */
        return f * scale + 0.5;
    }

/* Audio format consists of a main format field (upper 8 bits) and a sub format
 * field (lower 24 bits).
 *
 * The main format indicates the main codec type. The sub format field
 * indicates options and parameters for each format. The sub format is mainly
 * used for record to indicate for instance the requested bitrate or profile.
 * It can also be used for certain formats to give informations not present in
 * the encoded audio stream (e.g. octet alignement for AMR).
 */
    typedef enum {
        AUDIO_FORMAT_INVALID             = 0xFFFFFFFFUL,
        AUDIO_FORMAT_DEFAULT             = 0,
        AUDIO_FORMAT_PCM                 = 0x00000000UL, /* DO NOT CHANGE */
        AUDIO_FORMAT_PCM_16_BIT
    } audio_format_t;
/* For the channel mask for position assignment representation */
    enum {
/* These can be a complete audio_channel_mask_t. */
                AUDIO_CHANNEL_NONE                      = 0x0,
        AUDIO_CHANNEL_INVALID                   = 0xC0000000,
/* These can be the bits portion of an audio_channel_mask_t
 * with representation AUDIO_CHANNEL_REPRESENTATION_POSITION.
 * Using these bits as a complete audio_channel_mask_t is deprecated.
 */
        /* output channels */
                AUDIO_CHANNEL_OUT_FRONT_LEFT            = 0x1,
        AUDIO_CHANNEL_OUT_FRONT_RIGHT           = 0x2,
        AUDIO_CHANNEL_OUT_FRONT_CENTER          = 0x4,
        AUDIO_CHANNEL_OUT_LOW_FREQUENCY         = 0x8,
        AUDIO_CHANNEL_OUT_BACK_LEFT             = 0x10,
        AUDIO_CHANNEL_OUT_BACK_RIGHT            = 0x20,
        AUDIO_CHANNEL_OUT_FRONT_LEFT_OF_CENTER  = 0x40,
        AUDIO_CHANNEL_OUT_FRONT_RIGHT_OF_CENTER = 0x80,
        AUDIO_CHANNEL_OUT_BACK_CENTER           = 0x100,
        AUDIO_CHANNEL_OUT_SIDE_LEFT             = 0x200,
        AUDIO_CHANNEL_OUT_SIDE_RIGHT            = 0x400,
        AUDIO_CHANNEL_OUT_TOP_CENTER            = 0x800,
        AUDIO_CHANNEL_OUT_TOP_FRONT_LEFT        = 0x1000,
        AUDIO_CHANNEL_OUT_TOP_FRONT_CENTER      = 0x2000,
        AUDIO_CHANNEL_OUT_TOP_FRONT_RIGHT       = 0x4000,
        AUDIO_CHANNEL_OUT_TOP_BACK_LEFT         = 0x8000,
        AUDIO_CHANNEL_OUT_TOP_BACK_CENTER       = 0x10000,
        AUDIO_CHANNEL_OUT_TOP_BACK_RIGHT        = 0x20000,
/* TODO: should these be considered complete channel masks, or only bits? */
                AUDIO_CHANNEL_OUT_MONO     = AUDIO_CHANNEL_OUT_FRONT_LEFT,
        AUDIO_CHANNEL_OUT_STEREO   = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                      AUDIO_CHANNEL_OUT_FRONT_RIGHT),
        AUDIO_CHANNEL_OUT_QUAD     = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                      AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                      AUDIO_CHANNEL_OUT_BACK_LEFT |
                                      AUDIO_CHANNEL_OUT_BACK_RIGHT),
        AUDIO_CHANNEL_OUT_QUAD_BACK = AUDIO_CHANNEL_OUT_QUAD,
        /* like AUDIO_CHANNEL_OUT_QUAD_BACK with *_SIDE_* instead of *_BACK_* */
                AUDIO_CHANNEL_OUT_QUAD_SIDE = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                               AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                               AUDIO_CHANNEL_OUT_SIDE_LEFT |
                                               AUDIO_CHANNEL_OUT_SIDE_RIGHT),
        AUDIO_CHANNEL_OUT_5POINT1  = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                      AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                      AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                      AUDIO_CHANNEL_OUT_LOW_FREQUENCY |
                                      AUDIO_CHANNEL_OUT_BACK_LEFT |
                                      AUDIO_CHANNEL_OUT_BACK_RIGHT),
        AUDIO_CHANNEL_OUT_5POINT1_BACK = AUDIO_CHANNEL_OUT_5POINT1,
        /* like AUDIO_CHANNEL_OUT_5POINT1_BACK with *_SIDE_* instead of *_BACK_* */
                AUDIO_CHANNEL_OUT_5POINT1_SIDE = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                                  AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                                  AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                                  AUDIO_CHANNEL_OUT_LOW_FREQUENCY |
                                                  AUDIO_CHANNEL_OUT_SIDE_LEFT |
                                                  AUDIO_CHANNEL_OUT_SIDE_RIGHT),
        // matches the correct AudioFormat.CHANNEL_OUT_7POINT1_SURROUND definition for 7.1
                AUDIO_CHANNEL_OUT_7POINT1  = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                              AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                              AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                              AUDIO_CHANNEL_OUT_LOW_FREQUENCY |
                                              AUDIO_CHANNEL_OUT_BACK_LEFT |
                                              AUDIO_CHANNEL_OUT_BACK_RIGHT |
                                              AUDIO_CHANNEL_OUT_SIDE_LEFT |
                                              AUDIO_CHANNEL_OUT_SIDE_RIGHT),
        AUDIO_CHANNEL_OUT_ALL      = (AUDIO_CHANNEL_OUT_FRONT_LEFT |
                                      AUDIO_CHANNEL_OUT_FRONT_RIGHT |
                                      AUDIO_CHANNEL_OUT_FRONT_CENTER |
                                      AUDIO_CHANNEL_OUT_LOW_FREQUENCY |
                                      AUDIO_CHANNEL_OUT_BACK_LEFT |
                                      AUDIO_CHANNEL_OUT_BACK_RIGHT |
                                      AUDIO_CHANNEL_OUT_FRONT_LEFT_OF_CENTER |
                                      AUDIO_CHANNEL_OUT_FRONT_RIGHT_OF_CENTER |
                                      AUDIO_CHANNEL_OUT_BACK_CENTER|
                                      AUDIO_CHANNEL_OUT_SIDE_LEFT|
                                      AUDIO_CHANNEL_OUT_SIDE_RIGHT|
                                      AUDIO_CHANNEL_OUT_TOP_CENTER|
                                      AUDIO_CHANNEL_OUT_TOP_FRONT_LEFT|
                                      AUDIO_CHANNEL_OUT_TOP_FRONT_CENTER|
                                      AUDIO_CHANNEL_OUT_TOP_FRONT_RIGHT|
                                      AUDIO_CHANNEL_OUT_TOP_BACK_LEFT|
                                      AUDIO_CHANNEL_OUT_TOP_BACK_CENTER|
                                      AUDIO_CHANNEL_OUT_TOP_BACK_RIGHT),
/* These are bits only, not complete values */
        /* input channels */
                AUDIO_CHANNEL_IN_LEFT            = 0x4,
        AUDIO_CHANNEL_IN_RIGHT           = 0x8,
        AUDIO_CHANNEL_IN_FRONT           = 0x10,
        AUDIO_CHANNEL_IN_BACK            = 0x20,
        AUDIO_CHANNEL_IN_LEFT_PROCESSED  = 0x40,
        AUDIO_CHANNEL_IN_RIGHT_PROCESSED = 0x80,
        AUDIO_CHANNEL_IN_FRONT_PROCESSED = 0x100,
        AUDIO_CHANNEL_IN_BACK_PROCESSED  = 0x200,
        AUDIO_CHANNEL_IN_PRESSURE        = 0x400,
        AUDIO_CHANNEL_IN_X_AXIS          = 0x800,
        AUDIO_CHANNEL_IN_Y_AXIS          = 0x1000,
        AUDIO_CHANNEL_IN_Z_AXIS          = 0x2000,
        AUDIO_CHANNEL_IN_VOICE_UPLINK    = 0x4000,
        AUDIO_CHANNEL_IN_VOICE_DNLINK    = 0x8000,
/* TODO: should these be considered complete channel masks, or only bits, or deprecated? */
                AUDIO_CHANNEL_IN_MONO   = AUDIO_CHANNEL_IN_FRONT,
        AUDIO_CHANNEL_IN_STEREO = (AUDIO_CHANNEL_IN_LEFT | AUDIO_CHANNEL_IN_RIGHT),
        AUDIO_CHANNEL_IN_FRONT_BACK = (AUDIO_CHANNEL_IN_FRONT | AUDIO_CHANNEL_IN_BACK),
        AUDIO_CHANNEL_IN_ALL    = (AUDIO_CHANNEL_IN_LEFT |
                                   AUDIO_CHANNEL_IN_RIGHT |
                                   AUDIO_CHANNEL_IN_FRONT |
                                   AUDIO_CHANNEL_IN_BACK|
                                   AUDIO_CHANNEL_IN_LEFT_PROCESSED |
                                   AUDIO_CHANNEL_IN_RIGHT_PROCESSED |
                                   AUDIO_CHANNEL_IN_FRONT_PROCESSED |
                                   AUDIO_CHANNEL_IN_BACK_PROCESSED|
                                   AUDIO_CHANNEL_IN_PRESSURE |
                                   AUDIO_CHANNEL_IN_X_AXIS |
                                   AUDIO_CHANNEL_IN_Y_AXIS |
                                   AUDIO_CHANNEL_IN_Z_AXIS |
                                   AUDIO_CHANNEL_IN_VOICE_UPLINK |
                                   AUDIO_CHANNEL_IN_VOICE_DNLINK),
    };
/* A channel mask per se only defines the presence or absence of a channel, not the order.
 * But see AUDIO_INTERLEAVE_* below for the platform convention of order.
 *
 * audio_channel_mask_t is an opaque type and its internal layout should not
 * be assumed as it may change in the future.
 * Instead, always use the functions declared in this header to examine.
 *
 * These are the current representations:
 *
 *   AUDIO_CHANNEL_REPRESENTATION_POSITION
 *     is a channel mask representation for position assignment.
 *     Each low-order bit corresponds to the spatial position of a transducer (output),
 *     or interpretation of channel (input).
 *     The user of a channel mask needs to know the context of whether it is for output or input.
 *     The constants AUDIO_CHANNEL_OUT_* or AUDIO_CHANNEL_IN_* apply to the bits portion.
 *     It is not permitted for no bits to be set.
 *
 *   AUDIO_CHANNEL_REPRESENTATION_INDEX
 *     is a channel mask representation for index assignment.
 *     Each low-order bit corresponds to a selected channel.
 *     There is no platform interpretation of the various bits.
 *     There is no concept of output or input.
 *     It is not permitted for no bits to be set.
 *
 * All other representations are reserved for future use.
 *
 * Warning: current representation distinguishes between input and output, but this will not the be
 * case in future revisions of the platform. Wherever there is an ambiguity between input and output
 * that is currently resolved by checking the channel mask, the implementer should look for ways to
 * fix it with additional information outside of the mask.
 */
    typedef uint32_t audio_channel_mask_t;

class AudioResampler {
public:
    // Determines quality of SRC.
    //  LOW_QUALITY: linear interpolator (1st order)
    //  MED_QUALITY: cubic interpolator (3rd order)
    //  HIGH_QUALITY: fixed multi-tap FIR (e.g. 48KHz->44.1KHz)
    // NOTE: high quality SRC will only be supported for
    // certain fixed rate conversions. Sample rate cannot be
    // changed dynamically.
    enum src_quality {
        DEFAULT_QUALITY=0,
        LOW_QUALITY=1,
        MED_QUALITY=2,
        HIGH_QUALITY=3,
        VERY_HIGH_QUALITY=4,
    };

    static const CONSTEXPR float UNITY_GAIN_FLOAT = 1.0f;

    static AudioResampler* create(audio_format_t format, int inChannelCount,
            int32_t sampleRate, src_quality quality=DEFAULT_QUALITY);

    virtual ~AudioResampler();

    virtual void init() = 0;
    virtual void setSampleRate(int32_t inSampleRate);
    virtual void setVolume(float left, float right);
    virtual void setLocalTimeFreq(uint64_t freq);

    // set the PTS of the next buffer output by the resampler
    virtual void setPTS(int64_t pts);

    // Resample int16_t samples from provider and accumulate into 'out'.
    // A mono provider delivers a sequence of samples.
    // A stereo provider delivers a sequence of interleaved pairs of samples.
    //
    // In either case, 'out' holds interleaved pairs of fixed-point Q4.27.
    // That is, for a mono provider, there is an implicit up-channeling.
    // Since this method accumulates, the caller is responsible for clearing 'out' initially.
    //
    // For a float resampler, 'out' holds interleaved pairs of float samples.
    //
    // Multichannel interleaved frames for n > 2 is supported for quality DYN_LOW_QUALITY,
    // DYN_MED_QUALITY, and DYN_HIGH_QUALITY.
    //
    // Returns the number of frames resampled into the out buffer.
    virtual size_t resample(int32_t* out, size_t outFrameCount,
            AudioBufferProvider* provider) = 0;

    virtual void reset();
    virtual size_t getUnreleasedFrames() const { return mInputIndex; }

    // called from destructor, so must not be virtual
    src_quality getQuality() const { return mQuality; }

protected:
    // number of bits for phase fraction - 30 bits allows nearly 2x downsampling
    static const int kNumPhaseBits = 30;

    // phase mask for fraction
    static const uint32_t kPhaseMask = (1LU<<kNumPhaseBits)-1;

    // multiplier to calculate fixed point phase increment
    static const double kPhaseMultiplier;

    AudioResampler(int inChannelCount, int32_t sampleRate, src_quality quality);

    // prevent copying
    AudioResampler(const AudioResampler&);
    AudioResampler& operator=(const AudioResampler&);

    int64_t calculateOutputPTS(int outputFrameIndex);

    const int32_t mChannelCount;
    const int32_t mSampleRate;
    int32_t mInSampleRate;
    AudioBufferProvider::Buffer mBuffer;
    union {
        int16_t mVolume[2];
        uint32_t mVolumeRL;
    };
    int16_t mTargetVolume[2];
    size_t mInputIndex;
    int32_t mPhaseIncrement;
    uint32_t mPhaseFraction;
    uint64_t mLocalTimeFreq;
    int64_t mPTS;

    // returns the inFrameCount required to generate outFrameCount frames.
    //
    // Placed here to be a consistent for all resamplers.
    //
    // Right now, we use the upper bound without regards to the current state of the
    // input buffer using integer arithmetic, as follows:
    //
    // (static_cast<uint64_t>(outFrameCount)*mInSampleRate + (mSampleRate - 1))/mSampleRate;
    //
    // The double precision equivalent (float may not be precise enough):
    // ceil(static_cast<double>(outFrameCount) * mInSampleRate / mSampleRate);
    //
    // this relies on the fact that the mPhaseIncrement is rounded down from
    // #phases * mInSampleRate/mSampleRate and the fact that Sum(Floor(x)) <= Floor(Sum(x)).
    // http://www.proofwiki.org/wiki/Sum_of_Floors_Not_Greater_Than_Floor_of_Sums
    //
    // (so long as double precision is computed accurately enough to be considered
    // greater than or equal to the Floor(x) value in int32_t arithmetic; thus this
    // will not necessarily hold for floats).
    //
    // TODO:
    // Greater accuracy and a tight bound is obtained by:
    // 1) subtract and adjust for the current state of the AudioBufferProvider buffer.
    // 2) using the exact integer formula where (ignoring 64b casting)
    //  inFrameCount = (mPhaseIncrement * (outFrameCount - 1) + mPhaseFraction) / phaseWrapLimit;
    //  phaseWrapLimit is the wraparound (1 << kNumPhaseBits), if not specified explicitly.
    //
    inline size_t getInFrameCountRequired(size_t outFrameCount) {
        return (static_cast<uint64_t>(outFrameCount)*mInSampleRate
                + (mSampleRate - 1))/mSampleRate;
    }

    inline float clampFloatVol(float volume) {
        if (volume > UNITY_GAIN_FLOAT) {
            return UNITY_GAIN_FLOAT;
        } else if (volume >= 0.) {
            return volume;
        }
        return 0.;  // NaN or negative volume maps to 0.
    }

private:
    const src_quality mQuality;

    // Return 'true' if the quality level is supported without explicit request
    static bool qualityIsSupported(src_quality quality);

    // For pthread_once()
    static void init_routine();

    // Return the estimated CPU load for specific resampler in MHz.
    // The absolute number is irrelevant, it's the relative values that matter.
    static uint32_t qualityMHz(src_quality quality);
};

// ----------------------------------------------------------------------------
} // namespace android

#endif // ANDROID_AUDIO_RESAMPLER_H
