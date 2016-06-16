//
// Created by James Chen on 6/13/16.
//

#include "AudioPlayer.h"

#include <android/log.h>
#include <math.h>
#include <unistd.h>

#define LOG_TAG "AudioPlayer"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

#define DESTROY(OBJ)    \
    if ((OBJ) != NULL) { \
        (*(OBJ))->Destroy(OBJ); \
        (OBJ) = NULL; \
    }

#define CHECK_SL_RESULT(r) if (r != SL_RESULT_SUCCESS) return false;

class SLAudioPlayerCallbackProxy
{
public:
    static void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
//        LOGD("samplePlayerCallback, context = %p ...", context);
        AudioPlayer* thiz = reinterpret_cast<AudioPlayer*>(context);
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

//        AudioPlayer* thiz = reinterpret_cast<AudioPlayer*>(context);
//        thiz->playProgressCallback(caller, event);
    }
};

AudioPlayer::AudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject)
        : _engineItf(engineItf)
        , _outputMixObj(outputMixObject)
        , _assetFd(0)
        , _playObj(NULL)
        , _playItf(NULL)
        , _seekItf(NULL)
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
    memset(_silenceData, 0, sizeof(_silenceData));
}

AudioPlayer::~AudioPlayer()
{
    LOGD("In the destructor of AudioPlayer %p", this);
    LOGD("~AudioPlayer 01");
    _isDestroyed = true;

    //FIXME: don't use usleep here
    while (_isDestroyed)
    {
        _enqueueCond.notify_one();
        usleep(10 * 1000);
    }
//

    LOGD("~AudioPlayer 02");
    DESTROY(_playObj);
    LOGD("~AudioPlayer 03");
    if(_assetFd > 0)
    {
        LOGD("~AudioPlayer 04");
        close(_assetFd);
        _assetFd = 0;
    }
    LOGD("~AudioPlayer end");
}

void AudioPlayer::samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq)
{
    SLresult res = SL_RESULT_SUCCESS;
//    res = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
//    res = (*_bufferQueueItf)->Clear(_bufferQueueItf);
//    return;
    // FIXME: AudioPlayer instance may be destroyed, we need to find a way to wait...
    // It's in sub thread
//    LOGD("samplePlayerCallback, context = %p ...", this);

    if (!_decResult.pcmBuffer || _currentBufferIndex >= _decResult.pcmBuffer->size())
    {
        std::unique_lock<std::mutex> lk(_enqueueMutex);

        _currentBufferIndex = 0;
        _decResult.reset();
        _isPlaying = false;

        SLuint32 state;
        res = (*_playItf)->GetPlayState(_playItf, &state);

        SLAndroidSimpleBufferQueueState bufState;
        res = (*_bufferQueueItf)->GetState(_bufferQueueItf, &bufState);
        LOGD("AudioPlayer %p is waiting for pcm data, state = %d, bufState.index = %d, bufState.count=%d",
             this, state, bufState.index, bufState.count);


        _enqueueCond.wait(lk);

        if (_isDestroyed)
        {
            stop();
            (*_bufferQueueItf)->Clear(_bufferQueueItf);
            LOGD("AudioPlayer (%p) was destroyed!", this);
            // Reset _isDestroyed to false
            _isDestroyed = false;
            return;
        }
    }

    char* base = _decResult.pcmBuffer->data();
    char* data = base + _currentBufferIndex;
    int remain = _decResult.pcmBuffer->size() - _currentBufferIndex;
    int size = std::min(remain, _bufferSizeInBytes);

//    LOGD("enqueue buffer size: %d, index = %d, totalEnqueueSize: %d", size, _currentBufferIndex, _currentBufferIndex + size);
    res = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, data, size);

    _currentBufferIndex += size;
}

void AudioPlayer::play()
{
    LOGD("AudioPlayer::play ...");
    std::chrono::high_resolution_clock::time_point oldest = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point now = oldest, old = oldest;
    long duration = 0;

    SLresult res;
    res = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - oldest).count());
    LOGD("AudioPlayer::play wastes: %ld us", duration);
}

void AudioPlayer::playWithPcmData(const AudioDecoder::Result &decResult, float volume, bool loop)
{
    if (isPlaying())
    {
        LOGE("AudioPlayer is playing, ignore play ...");
    }
    else
    {
        std::chrono::high_resolution_clock::time_point oldest = std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point now = oldest, old = oldest;
        long duration = 0;

        _isPlaying = true;
        _decResult = decResult;
        _currentBufferIndex = 0;

        if (_decResult.sampleRate != _sampleRate)
        {
            LOGE("Wrong sampleRate %d , expected %d", _decResult.sampleRate, _sampleRate);
        }

        if (_decResult.numChannels != _numChannels)
        {
            LOGE("Wrong channel count %d , expected %d", _decResult.numChannels, _numChannels);
        }

        _enqueueCond.notify_one();

        now = std::chrono::high_resolution_clock::now();
        duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
        old = now;
        LOGD("AudioPlayer::playWithPcmData wastes: %ld us", duration);
    }
}

void AudioPlayer::stop()
{
    LOGD("AudioPlayer::stop begin...");
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
    LOGD("AudioPlayer::stop end...");
}

void AudioPlayer::pause()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PAUSED);
}

void AudioPlayer::resume()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
}

bool AudioPlayer::initForPlayPcmData(int numChannels, int sampleRate, int bufferSizeInBytes)
{
    std::chrono::high_resolution_clock::time_point oldest = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point now = oldest, old = oldest;
    long duration = 0;

    _numChannels = numChannels;
    _sampleRate = sampleRate;
    _bufferSizeInBytes = bufferSizeInBytes;

    SLuint32 channelMask = SL_SPEAKER_FRONT_CENTER;

    if (numChannels > 1) {
        channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    }

    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            (SLuint32) numChannels,
            (SLuint32) sampleRate * 1000,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            channelMask,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            AUDIO_PLAYER_BUFFER_COUNT
    };
    SLDataSource source = {&loc_bufq, &format_pcm};

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("Init audio source location waste: %ld us", duration);

    SLDataLocator_OutputMix loc_outmix = {
            SL_DATALOCATOR_OUTPUTMIX,
            _outputMixObj
    };
    SLDataSink sink = {&loc_outmix, NULL};

#undef NUM
#define NUM (3)

    const SLInterfaceID ids[NUM] = {
            SL_IID_PLAY,
            SL_IID_VOLUME,
            SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
    };
    const SLboolean req[NUM] = {
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
    };

    SLresult res;

    res = (*_engineItf)->CreateAudioPlayer(_engineItf, &_playObj, &source, &sink, NUM, ids, req); CHECK_SL_RESULT(res);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("CreateAudioPlayer waste: %ld us", duration);

    res = (*_playObj)->Realize(_playObj, SL_BOOLEAN_FALSE);CHECK_SL_RESULT(res);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("Realize waste: %ld us", duration);

    res = (*_playObj)->GetInterface(_playObj, SL_IID_PLAY, &_playItf);CHECK_SL_RESULT(res);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_VOLUME, &_volumeItf);CHECK_SL_RESULT(res);
    res = (*_playObj)->GetInterface(_playObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &_bufferQueueItf);CHECK_SL_RESULT(res);

    res = (*_playItf)->SetCallbackEventsMask(_playItf,
                                             SL_PLAYEVENT_HEADATMARKER | SL_PLAYEVENT_HEADATNEWPOS | SL_PLAYEVENT_HEADATEND);CHECK_SL_RESULT(res);

    res = (*_playItf)->RegisterCallback(_playItf, SLAudioPlayerCallbackProxy::playProgressCallback, this);CHECK_SL_RESULT(res);

    res = (*_bufferQueueItf)->RegisterCallback(_bufferQueueItf, SLAudioPlayerCallbackProxy::samplePlayerCallback, this);CHECK_SL_RESULT(res);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("GetInterface waste: %ld us", duration);

    SLuint32 state;
    res = (*_playItf)->GetPlayState(_playItf, &state);CHECK_SL_RESULT(res);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("GetPlayState wastes: %ld us, state: %d", duration, state);

    res = (*_bufferQueueItf)->Enqueue(_bufferQueueItf, _silenceData, _bufferSizeInBytes);CHECK_SL_RESULT(res);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("Enqueue waste: %ld us", duration);

    res = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);CHECK_SL_RESULT(res);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("SetPlayState SL_PLAYSTATE_PLAYING wastes: %ld us", duration);

//    res = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
    old = now;
    LOGD("SetPlayState2 SL_PLAYSTATE_PLAYING wastes: %ld us", duration);

    now = std::chrono::high_resolution_clock::now();
    duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - oldest).count());
    LOGD("AudioPlayer::initWithPcmData waste: %ld us", duration);

    return true;
}

bool AudioPlayer::initWithUrl(const std::string& url, float volume, bool loop, const FdGetterCallback& fdGetter)
{
    std::chrono::high_resolution_clock::time_point oldest = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point now = oldest, old = oldest;
    long duration = 0;

    bool ret = false;

    do
    {
        SLDataSource audioSrc;

        SLDataLocator_AndroidFD loc_fd;
        SLDataLocator_URI loc_uri;

        SLDataFormat_MIME format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
        audioSrc.pFormat = &format_mime;

        if (url[0] != '/') {
            off_t start = 0, length = 0;
            std::string relativePath;
            size_t position = url.find("assets/");

            if (0 == position) {
                // "assets/" is at the beginning of the path and we don't want it
                relativePath = url.substr(strlen("assets/"));
            } else {
                relativePath = url;
            }

            _assetFd = fdGetter(relativePath, &start, &length);

            if (_assetFd <= 0) {
                LOGE("Failed to open file descriptor for '%s'", url.c_str());
                break;
            }

            // configure audio source
            loc_fd = {SL_DATALOCATOR_ANDROIDFD, _assetFd, start, length};

            audioSrc.pLocator = &loc_fd;
        }
        else{
            loc_uri = {SL_DATALOCATOR_URI , (SLchar*)url.c_str()};
            audioSrc.pLocator = &loc_uri;
        }

        now = std::chrono::high_resolution_clock::now();
        duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
        old = now;
        LOGD("Init audio source location waste: %ld us", duration);

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, _outputMixObj};
        SLDataSink audioSnk = {&loc_outmix, NULL};

        // create audio player
        const SLInterfaceID ids[3] = {SL_IID_SEEK, SL_IID_PREFETCHSTATUS, SL_IID_VOLUME};
        const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        auto result = (*_engineItf)->CreateAudioPlayer(_engineItf, &_playObj, &audioSrc, &audioSnk, 3, ids, req);
        if(SL_RESULT_SUCCESS != result){ LOGE("create audio player fail"); break; }

        now = std::chrono::high_resolution_clock::now();
        duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
        old = now;
        LOGD("CreateAudioPlayer waste: %ld us", duration);

        // realize the player
        result = (*_playObj)->Realize(_playObj, SL_BOOLEAN_FALSE);
        if(SL_RESULT_SUCCESS != result){ LOGE("realize the player fail"); break; }

        // get the play interface
        result = (*_playObj)->GetInterface(_playObj, SL_IID_PLAY, &_playItf);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the play interface fail"); break; }

        // get the seek interface
        result = (*_playObj)->GetInterface(_playObj, SL_IID_SEEK, &_seekItf);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the seek interface fail"); break; }

        // get the volume interface
        result = (*_playObj)->GetInterface(_playObj, SL_IID_VOLUME, &_volumeItf);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the volume interface fail"); break; }

        _isLoop = loop;
        if (_isLoop){
            (*_seekItf)->SetLoop(_seekItf, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);
        }

        int dbVolume = 2000 * log10(volume);
        if(dbVolume < SL_MILLIBEL_MIN){
            dbVolume = SL_MILLIBEL_MIN;
        }
        (*_volumeItf)->SetVolumeLevel(_volumeItf, dbVolume);

        now = std::chrono::high_resolution_clock::now();
        duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
        old = now;
        LOGD("Realize waste: %ld us", duration);

        result = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
        if(SL_RESULT_SUCCESS != result){ LOGE("SetPlayState fail"); break; }

        now = std::chrono::high_resolution_clock::now();
        duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
        old = now;
        LOGD("setPlayState wastes: %ld us", duration);

        SLuint32 state;
        (*_playItf)->GetPlayState(_playItf, &state);

        now = std::chrono::high_resolution_clock::now();
        duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - old).count());
        old = now;
        LOGD("getPlayState wastes: %ld us", duration);

        now = std::chrono::high_resolution_clock::now();
        duration = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now - oldest).count());
        LOGD("AudioPlayer::initWithUrl waste: %ld us", duration);

        ret = true;
    } while (0);

    return ret;
}

void AudioPlayer::setOwnedByAudioPlayerPool(bool isOwnedByPool) {
    _isOwnedByPool = true;
}

bool AudioPlayer::isPlaying() {
    return _isPlaying;
}
