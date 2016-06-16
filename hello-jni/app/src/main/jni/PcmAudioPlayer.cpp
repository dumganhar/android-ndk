//
// Created by James Chen on 6/16/16.
//

#include "PcmAudioPlayer.h"

#include <android/log.h>
#include <math.h>
#include <unistd.h>

#define LOG_TAG "PcmAudioPlayer"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

#define DESTROY(OBJ)    \
    if ((OBJ) != NULL) { \
        (*(OBJ))->Destroy(OBJ); \
        (OBJ) = NULL; \
    }

#define CHECK_SL_RESULT(r, line) \
    if (r != SL_RESULT_SUCCESS) {\
        LOGE("SL result %d is wrong, line: %d", r, line); \
        return false; \
    }

//FIXME: a lot of return values of SL functions need to be check

#define AUDIO_PLAYER_BUFFER_COUNT (2)

static std::vector<char> __silenceData;

class SLPcmAudioPlayerCallbackProxy
{
public:
    static void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
        PcmAudioPlayer* thiz = reinterpret_cast<PcmAudioPlayer*>(context);
        thiz->samplePlayerCallback(bq);
    }

    static void playProgressCallback(SLPlayItf caller, void* context, SLuint32 event)
    {
        // ITS IN SUB THREAD, BE CAREFUL
        SLresult result;
        SLmillisecond msec = 0;
        //DONT INVOKE GetPosition here , it will cause deadlock.
//        result = (*caller)->GetPosition(caller, &msec);

        if (SL_PLAYEVENT_HEADATEND & event) {
            LOGD("SL_PLAYEVENT_HEADATEND current position=%u ms\n", msec);
        }

        if (SL_PLAYEVENT_HEADATNEWPOS & event) {
            LOGD("SL_PLAYEVENT_HEADATNEWPOS current position=%u ms\n", msec);
        }

        if (SL_PLAYEVENT_HEADATMARKER & event) {
            LOGD("SL_PLAYEVENT_HEADATMARKER current position=%u ms\n", msec);
        }

//        PcmAudioPlayer* thiz = reinterpret_cast<PcmAudioPlayer*>(context);
//        thiz->playProgressCallback(caller, event);
    }
};

PcmAudioPlayer::PcmAudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject)
        : _engineItf(engineItf)
        , _outputMixObj(outputMixObject)
        , _playObj(NULL)
        , _playItf(NULL)
        , _volumeItf(NULL)
        , _bufferQueueItf(NULL)
        , _numChannels(-1)
        , _sampleRate(-1)
        , _bufferSizeInBytes(0)
        , _isLoop(false)
        , _volume(0.0f)
        , _isOwnedByPool(false)
        , _isPlaying(false)
        , _isDestroyed(false)
        , _currentBufferIndex(0)
{
}

PcmAudioPlayer::~PcmAudioPlayer()
{
    LOGD("In the destructor of UrlAudioPlayer %p", this);
    LOGD("PcmAudioPlayer 01");
    _isDestroyed = true;

    while (_isDestroyed)
    {
        _enqueueCond.notify_one();
        usleep(10 * 1000);
    }

    LOGD("PcmAudioPlayer 02");
    DESTROY(_playObj);
    LOGD("PcmAudioPlayer end");
}

void PcmAudioPlayer::wait()
{
    LOGD("PcmAudioPlayer %p is waiting ...", this);
    std::unique_lock<std::mutex> lk(_enqueueMutex);

    _currentBufferIndex = 0;
    _decResult.reset();
    setPlaying(false);

    _enqueueCond.wait(lk);
}

void PcmAudioPlayer::wakeup()
{
    setVolume(_volume);
}

void PcmAudioPlayer::enqueue()
{
    SLresult res = SL_RESULT_SUCCESS;
    char* base = _decResult.pcmBuffer->data();
    char* data = base + _currentBufferIndex;
    int remain = _decResult.pcmBuffer->size() - _currentBufferIndex;
    int size = std::min(remain, _bufferSizeInBytes);

//    LOGD("enqueue buffer size: %d, index = %d, totalEnqueueSize: %d", size, _currentBufferIndex, _currentBufferIndex + size);
    res = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, data, size);

    _currentBufferIndex += size;
}

void PcmAudioPlayer::samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq)
{
    // FIXME: UrlAudioPlayer instance may be destroyed, we need to find a way to wait...
    // It's in sub thread
    int pcmDataSize = _decResult.pcmBuffer ? _decResult.pcmBuffer->size() : 0;

    if (_currentBufferIndex >= pcmDataSize)
    {
        if (_isLoop)
        {
            _currentBufferIndex = 0;
        }
        else
        {
            wait();

            if (_isDestroyed)
            {
                stop();
                (*_bufferQueueItf)->Clear(_bufferQueueItf);
                LOGD("UrlAudioPlayer (%p) was destroyed!", this);
                // Reset _isDestroyed to false
                _isDestroyed = false;
                return;
            }

            wakeup();
        }
    }

    enqueue();
}

int PcmAudioPlayer::play(const AudioDecoder::Result &decResult, float volume, bool loop)
{
    LOGD("playWithPcmData, volume: %f, loop: %d", volume, loop);
    if (isPlaying())
    {
        LOGE("UrlAudioPlayer is playing, ignore play ...");
    }
    else
    {
        setPlaying(true);
        _decResult = decResult;
        _currentBufferIndex = 0;
        _volume = volume;
        _isLoop = loop;

        if (_decResult.sampleRate != _sampleRate)
        {
            LOGE("Wrong sampleRate %d , expected %d", _decResult.sampleRate, _sampleRate);
        }

        if (_decResult.numChannels != _numChannels)
        {
            LOGE("Wrong channel count %d , expected %d", _decResult.numChannels, _numChannels);
        }

        _enqueueCond.notify_one();
    }
    LOGD("playWithPcmData end ..");

    return 1;
}

void PcmAudioPlayer::stop()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
}

void PcmAudioPlayer::pause()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PAUSED);
}

void PcmAudioPlayer::resume()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
}

bool PcmAudioPlayer::initForPlayPcmData(int numChannels, int sampleRate, int bufferSizeInBytes)
{
    _numChannels = numChannels;
    _sampleRate = sampleRate;
    _bufferSizeInBytes = bufferSizeInBytes;

    if (__silenceData.empty())
    {
        __silenceData.resize(_bufferSizeInBytes * AUDIO_PLAYER_BUFFER_COUNT, 0);
    }

    SLuint32 channelMask = SL_SPEAKER_FRONT_CENTER;

    if (numChannels > 1) {
        channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    }

    SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            (SLuint32) numChannels,
            (SLuint32) sampleRate * 1000,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            channelMask,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataLocator_AndroidSimpleBufferQueue locBufQueue = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            AUDIO_PLAYER_BUFFER_COUNT
    };
    SLDataSource source = {&locBufQueue, &formatPcm};

    SLDataLocator_OutputMix locOutmix = {
            SL_DATALOCATOR_OUTPUTMIX,
            _outputMixObj
    };
    SLDataSink sink = {&locOutmix, NULL};

    const SLInterfaceID ids[] = {
            SL_IID_PLAY,
            SL_IID_VOLUME,
            SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
    };

    const SLboolean req[] = {
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
    };

    SLresult res;

    res = (*_engineItf)->CreateAudioPlayer(_engineItf, &_playObj, &source, &sink, sizeof(ids) / sizeof(ids[0]), ids, req); CHECK_SL_RESULT(res, __LINE__);

    res = (*_playObj)->Realize(_playObj, SL_BOOLEAN_FALSE);CHECK_SL_RESULT(res, __LINE__);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_PLAY, &_playItf);CHECK_SL_RESULT(res, __LINE__);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_VOLUME, &_volumeItf);CHECK_SL_RESULT(res, __LINE__);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &_bufferQueueItf);CHECK_SL_RESULT(res, __LINE__);

    res = (*_playItf)->SetCallbackEventsMask(_playItf, SL_PLAYEVENT_HEADATMARKER | SL_PLAYEVENT_HEADATNEWPOS | SL_PLAYEVENT_HEADATEND);CHECK_SL_RESULT(res, __LINE__);
    res = (*_playItf)->RegisterCallback(_playItf, SLPcmAudioPlayerCallbackProxy::playProgressCallback, this);CHECK_SL_RESULT(res, __LINE__);

    res = (*_bufferQueueItf)->RegisterCallback(_bufferQueueItf, SLPcmAudioPlayerCallbackProxy::samplePlayerCallback, this);CHECK_SL_RESULT(res, __LINE__);
    res = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, __silenceData.data(), _bufferSizeInBytes);CHECK_SL_RESULT(res, __LINE__);

    res = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);CHECK_SL_RESULT(res, __LINE__);

    return true;
}

void PcmAudioPlayer::setVolume(float volume)
{
    int dbVolume = 2000 * log10(volume);
    if(dbVolume < SL_MILLIBEL_MIN){
        dbVolume = SL_MILLIBEL_MIN;
    }
    SLresult result = (*_volumeItf)->SetVolumeLevel(_volumeItf, dbVolume);
    if(SL_RESULT_SUCCESS != result){
        LOGD("%s error:%lu", __func__, result);
    }
}
