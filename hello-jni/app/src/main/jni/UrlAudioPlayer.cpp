//
// Created by James Chen on 6/13/16.
//

#include "UrlAudioPlayer.h"

#include <android/log.h>
#include <math.h>
#include <unistd.h>
#include <SLES/OpenSLES_Android.h>

#define LOG_TAG "AudioPlayer"
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

UrlAudioPlayer::UrlAudioPlayer(SLEngineItf engineItf, SLObjectItf outputMixObject)
        : _engineItf(engineItf)
        , _outputMixObj(outputMixObject)
        , _assetFd(0)
        , _playObj(NULL)
        , _playItf(NULL)
        , _seekItf(NULL)
        , _volumeItf(NULL)
        , _isLoop(false)
        , _volume(0.0f)
        , _duration(0.0f)
        , _isPlaying(false)
        , _isDestroyed(false)
        , _playOverCb(nullptr)
{
}

UrlAudioPlayer::~UrlAudioPlayer()
{
    LOGD("In the destructor of UrlAudioPlayer %p", this);
    LOGD("UrlAudioPlayeryer 01");
    _isDestroyed = true;

    LOGD("UrlAudioPlayeryer 02");
    DESTROY(_playObj);
    LOGD("UrlAudioPlayeryer 03");
    if(_assetFd > 0)
    {
        LOGD("UrlAudioPlayeryer 04");
        close(_assetFd);
        _assetFd = 0;
    }
    LOGD("UrlAudioPlayeryer end");
}

class SLUrlAudioPlayerCallbackProxy {
public:
    static void playEventCallback(SLPlayItf caller, void *context, SLuint32 playEvent)
    {
        UrlAudioPlayer* player = (UrlAudioPlayer*) context;
        player->playEventCallback(caller, playEvent);
    }
};

void UrlAudioPlayer::playEventCallback(SLPlayItf caller, SLuint32 playEvent)
{
    //Note that it's on sub thread, please don't invoke OpenSLES API on sub thread
    if (playEvent == SL_PLAYEVENT_HEADATEND) {
        //fix issue#8965:AudioEngine can't looping audio on Android 2.3.x
        if (_isLoop)
        {
            //FIXME: Don't invoke OpenSLES API here
            (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
        }
        else {
            if (_playOverCb != nullptr)
            {
                _playOverCb();
            }
        }
    }
}

void UrlAudioPlayer::setPlayOverCallback(const std::function<void()>& playOverCb)
{
    _playOverCb = playOverCb;
}

void UrlAudioPlayer::stop()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_STOPPED);
}

void UrlAudioPlayer::pause()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PAUSED);
}

void UrlAudioPlayer::resume()
{
    (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
}

int UrlAudioPlayer::play(const std::string& url, float volume, bool loop, const FdGetterCallback& fdGetter)
{
    int ret = -1;

    do
    {
        SLDataSource audioSrc;

        SLDataLocator_AndroidFD loc_fd;
        SLDataLocator_URI loc_uri;

        SLDataFormat_MIME formatMime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
        audioSrc.pFormat = &formatMime;

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

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, _outputMixObj};
        SLDataSink audioSnk = {&loc_outmix, NULL};

        // create audio player
        const SLInterfaceID ids[3] = {SL_IID_SEEK, SL_IID_PREFETCHSTATUS, SL_IID_VOLUME};
        const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        auto result = (*_engineItf)->CreateAudioPlayer(_engineItf, &_playObj, &audioSrc, &audioSnk, 3, ids, req);
        if(SL_RESULT_SUCCESS != result){ LOGE("create audio player fail"); break; }

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

        (*_playItf)->RegisterCallback(_playItf, SLUrlAudioPlayerCallbackProxy::playEventCallback, this);
        (*_playItf)->SetCallbackEventsMask(_playItf, SL_PLAYEVENT_HEADATEND);

        setVolume(volume);
        result = (*_playItf)->SetPlayState(_playItf, SL_PLAYSTATE_PLAYING);
        if(SL_RESULT_SUCCESS != result){ LOGE("SetPlayState fail"); break; }

        ret = 0;
    } while (0);

    return ret;
}

void UrlAudioPlayer::setVolume(float volume)
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

float UrlAudioPlayer::getDuration()
{
    SLmillisecond duration;
    auto result = (*_playItf)->GetDuration(_playItf, &duration);

    if (duration == SL_TIME_UNKNOWN)
    {
        return -1.0f;
    }
    else
    {
        _duration = duration / 1000.0;

        if (_duration <= 0)
        {
            return -1.0f;
        }
    }
    return _duration;
}

float UrlAudioPlayer::getPosition() {
    SLmillisecond millisecond;
    auto result = (*_playItf)->GetPosition(_playItf, &millisecond);
    return millisecond / 1000.0f;
}

void UrlAudioPlayer::setPosition(float pos)
{
    SLmillisecond millisecond = 1000.0f * pos;
    auto result = (*_seekItf)->SetPosition(_seekItf, millisecond, SL_SEEKMODE_ACCURATE);
    if(SL_RESULT_SUCCESS != result){
        return;
    }
}
