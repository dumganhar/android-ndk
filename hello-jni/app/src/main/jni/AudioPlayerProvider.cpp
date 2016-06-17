/****************************************************************************
Copyright (c) 2016 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include <android/log.h>
#include <sys/system_properties.h>
#include <stdlib.h>
#include "AudioPlayerProvider.h"
#include "UrlAudioPlayer.h"
#include "AudioDecoder.h"
#include "PcmAudioPlayer.h"
#include "PcmAudioPlayerPool.h"

#define LOG_TAG "AudioPlayerProvider"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

static int getSystemAPILevel()
{
    static int __systemApiLevel = -1;
    if (__systemApiLevel > 0)
    {
        return __systemApiLevel;
    }

    int apiLevel = -1;
    char sdk_ver_str[PROP_VALUE_MAX] = {0};
    auto len = __system_property_get("ro.build.version.sdk", sdk_ver_str);
    if (len > 0)
    {
        apiLevel = atoi(sdk_ver_str);
        LOGD("android build version:%d", apiLevel);
    }
    else
    {
        LOGD("Fail to get android build version.");
    }
    __systemApiLevel = apiLevel;
    return apiLevel;
}

AudioPlayerProvider::AudioPlayerProvider(SLEngineItf engineItf, SLObjectItf outputMixObject, int deviceSampleRate, int bufferSizeInFrames, const FdGetterCallback& fdGetterCallback)
        : _engineItf(engineItf)
        , _outputMixObject(outputMixObject)
        , _deviceSampleRate(deviceSampleRate)
        , _bufferSizeInFrames(bufferSizeInFrames)
        , _fdGetterCallback(fdGetterCallback)
        , _pcmAudioPlayerPool(nullptr)
{
    LOGD("deviceSampleRate: %d, bufferSizeInFrames: %d", _deviceSampleRate, _bufferSizeInFrames);
    if (getSystemAPILevel() >= 17)
    {
        _pcmAudioPlayerPool = new PcmAudioPlayerPool(engineItf, outputMixObject, deviceSampleRate,
                                                     bufferSizeInFrames);
    }
}

AudioPlayerProvider::~AudioPlayerProvider()
{
    delete _pcmAudioPlayerPool;
}

IAudioPlayer *AudioPlayerProvider::getAudioPlayer(const std::string &audioFilePath)
{
    IAudioPlayer* player = nullptr;

    // Pcm data decoding by OpenSLES API only supports in API level 17 and later.
    if (getSystemAPILevel() < 17)
    {
        AudioFileInfo info  = getFileInfo(audioFilePath);
        SLuint32 locatorType = info.assetFd > 0 ? SL_DATALOCATOR_ANDROIDFD : SL_DATALOCATOR_URI;
        if (info.length <= 0)
        {
            return nullptr;
        }

        auto urlPlayer = new UrlAudioPlayer(_engineItf, _outputMixObject);
        bool ret = urlPlayer->prepare(audioFilePath, locatorType, info.assetFd, info.start, info.length);
        if (ret)
        {
            player = urlPlayer;
        }
        else
        {
            delete urlPlayer;
        }
        return player;
    }

    PcmData pcmData;
    auto iter = _pcmCache.find(audioFilePath);
    if (iter != _pcmCache.end())
    {// Found pcm cache means it was used to be a PcmAudioPlayer
        pcmData = iter->second;
        auto pcmPlayer = _pcmAudioPlayerPool->findAvailablePlayer(pcmData.numChannels);
        if (pcmPlayer != nullptr)
        {
            pcmPlayer->prepare(audioFilePath, pcmData);
            player = pcmPlayer;
        }
    }
    else
    {
        // Check audio file size to determine to use a PcmAudioPlayer or UrlAudioPlayer,
        // generally PcmAudioPlayer is used for playing short audio like game effects while
        // playing background music uses UrlAudioPlayer
        AudioFileInfo info  = getFileInfo(audioFilePath);
        SLuint32 locatorType = info.assetFd > 0 ? SL_DATALOCATOR_ANDROIDFD : SL_DATALOCATOR_URI;
        if (info.length <= 0)
        {
            return nullptr;
        }

        if (isSmallFile(info.length))
        {
            pcmData = preloadEffect(audioFilePath);
            auto pcmPlayer = _pcmAudioPlayerPool->findAvailablePlayer(pcmData.numChannels);
            if (pcmPlayer != nullptr)
            {
                player = pcmPlayer->prepare(audioFilePath, pcmData) ? pcmPlayer : nullptr;
            }
        }
        else
        {
            auto urlPlayer = new UrlAudioPlayer(_engineItf, _outputMixObject);
            bool ret = urlPlayer->prepare(audioFilePath, locatorType, info.assetFd, info.start, info.length);
            if (ret)
            {
                player = urlPlayer;
            }
            else
            {
                delete urlPlayer;
            }
        }
    }

    return player;
}

PcmData AudioPlayerProvider::preloadEffect(const std::string &audioFilePath)
{
    PcmData pcmData;
    // Pcm data decoding by OpenSLES API only supports in API level 17 and later.
    if (getSystemAPILevel() < 17)
    {
        return pcmData;
    }

    auto info = getFileInfo(audioFilePath);
    if (isSmallFile(info.length))
    {
        LOGD("AudioPlayerProvider::preloadEffect: %s", audioFilePath.c_str());
        auto decoder = new AudioDecoder(_engineItf, audioFilePath, _deviceSampleRate);
        decoder->start(_fdGetterCallback);
        pcmData = decoder->getResult();
        _pcmCache.insert(std::make_pair(audioFilePath, pcmData));
        delete decoder;
    }
    else
    {
        LOGD("File (%s) is too large, ignore preload!", audioFilePath.c_str());
    }
    return pcmData;
}

AudioPlayerProvider::AudioFileInfo AudioPlayerProvider::getFileInfo(const std::string& audioFilePath)
{
    AudioFileInfo info;
    long fileSize = 0;
    off_t start = 0, length = 0;
    int assetFd = 0;

    if (audioFilePath[0] != '/')
    {
        std::string relativePath;
        size_t position = audioFilePath.find("assets/");

        if (0 == position)
        {
            // "assets/" is at the beginning of the path and we don't want it
            relativePath = audioFilePath.substr(strlen("assets/"));
        }
        else
        {
            relativePath = audioFilePath;
        }

        assetFd = _fdGetterCallback(relativePath, &start, &length);

        if (assetFd <= 0) {
            LOGE("Failed to open file descriptor for '%s'", audioFilePath.c_str());
            return info;
        }

        fileSize = length;
    }
    else
    {
        FILE* fp = fopen(audioFilePath.c_str(), "rb");
        if (fp != nullptr)
        {
            fseek(fp, 0, SEEK_END);
            fileSize = ftell(fp);
            fclose(fp);
        }
        else
        {
            return info;
        }
    }

    info.assetFd = assetFd;
    info.start = start;
    info.length = fileSize;

    LOGD("(%s) file size: %ld", audioFilePath.c_str(), fileSize);

    return info;
}

bool AudioPlayerProvider::isSmallFile(long fileSize)
{
    //TODO: If file size is smaller than 500k, we think it's a small file. This value should be set by developers.
    return fileSize < 500000;
}
