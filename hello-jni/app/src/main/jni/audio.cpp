#include <sys/types.h>
#include <assert.h>
#include <memory.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "wav.h"
#include "audio.h"
#include "AudioDecoder.h"
#include "AudioPlayer.h"
#include "AudioResampler.h"
#include "BufferProvider.h"
#include "AudioPlayerPool.h"

#include <android/log.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <map>
#include <unistd.h>

#define LOG_TAG "cjh"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

#define AUDIO_FUNC(NAME)    Java_com_example_hellojni_HelloJni_##NAME

using namespace cocos2d;

static AudioEngine engine;
static AudioOutputMix output;
static AAssetManager* __assetManager;
static int __deviceSampleRate = -1;

#define MAX_SAMPLES             (32)

static std::unordered_map<std::string, AudioDecoder::Result> __pcmCache;

extern "C" {

jboolean loadSample(JNIEnv *, AAssetManager *, jstring);
void destroyOutputMix(AudioOutputMix *);
void destroyEngine(AudioEngine *);

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniCreate)(JNIEnv *env, jclass clazz, jint sampleRate, jint bufferSizeInFrames) {
    __deviceSampleRate = sampleRate;
    SLresult res;
    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    res = slCreateEngine(&engine.object, 0, NULL, 0, NULL, NULL);
    // for brevity we omit checking 'res' and other errors.
    // a full implementation checks for errors throughout.
    res = (*engine.object)->Realize(engine.object, SL_BOOLEAN_FALSE);
    res = (*engine.object)->GetInterface(engine.object, SL_IID_ENGINE, &engine.engine);
    res = (*engine.engine)->CreateOutputMix(engine.engine, &output.object, 0, NULL, NULL);
    res = (*output.object)->Realize(output.object, SL_BOOLEAN_FALSE);

    AudioPlayerPool::init(engine.engine, output.object, sampleRate, bufferSizeInFrames);

    return JNI_TRUE;
}

JNIEXPORT
void
JNICALL
AUDIO_FUNC(jniShutdown)(JNIEnv *env, jclass clazz) {
    AudioPlayerPool::destroy();

    destroyOutputMix(&output);
    destroyEngine(&engine);

    __pcmCache.clear();
}

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniLoadSamples)(JNIEnv *env, jclass clazz, jobject asset_man, jobjectArray files) {
    AAssetManager *amgr = AAssetManager_fromJava(env, asset_man);
    __assetManager = amgr;
    int len = env->GetArrayLength(files);
    // feel free to implement your own sample management layer,
    // here i simply unload all samples beforehand.
//FIXME:    unloadSamples();
    for (int i = 0; i < len; ++i) {
        LOGD("i:%d, len=%d", i, len);
        jstring filename = (jstring) env->GetObjectArrayElement(files, i);
        jboolean loaded = loadSample(env, amgr, filename);
        if (!loaded) {
            LOGE("jniLoadSamples failed");
            return JNI_FALSE;
        }
    }
    return JNI_TRUE;
}


static int __fileIndex = 0;
static std::string __currentFilePath;

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniPlaySample)(JNIEnv *env, jclass clazz, jint index, jboolean play_state) {
//    return playSample(index, play_state);

    if (__fileIndex > 28) {
        __fileIndex = 0;
    }
    char filePath[256] = {0};
    sprintf(filePath, "/sdcard/%02d.mp3", __fileIndex);
    __currentFilePath = filePath;
    ++__fileIndex;

    AudioDecoder::Result pcmData;

    auto iter = __pcmCache.find(__currentFilePath);
    if (iter != __pcmCache.end())
    {
        pcmData = iter->second;
    }
    else
    {
        auto decoder = new AudioDecoder(engine.engine, __currentFilePath, __deviceSampleRate);
        decoder->start();
        pcmData = decoder->getResult();
        __pcmCache.insert(std::make_pair(__currentFilePath, pcmData));
        delete decoder;
    }

    auto player = AudioPlayerPool::findAvailableAudioPlayer(pcmData.numChannels);
    if (player != NULL)
    {
        player->playWithPcmData(pcmData, 1, false);
    }

//    usleep(300 * 1000);
//    player->stop();

//    usleep(100 * 1000);

//    delete player;

//    auto player1 = new AudioPlayer(engine.engine, output.object);
//    // test play with url
//
//    auto fdGetter = [](const std::string& url, off_t* start, off_t* length) -> int{
//        LOGD("in the callback of fdgetter ...");
//        int ret = 0;
//        auto asset = AAssetManager_open(__assetManager, url.c_str(), AASSET_MODE_UNKNOWN);
//        // open asset as file descriptor
//        ret = AAsset_openFileDescriptor(asset, start, length);
//        AAsset_close(asset);
//
//        return ret;
//    };
//
////    player1->initWithUrl("01.mp3", 1, false, fdGetter);
////    player1->play();
//
//    // test play with pcm data
//    auto decoder = new AudioDecoder(engine.engine, __currentFilePath);
//    decoder->start();
//
//    auto player2 = new AudioPlayer(engine.engine, output.object);
//    player2->initWithPcmData(decoder->getResult(), 1, false);
//    player2->play();
//
//    delete decoder;

    return 1;
}

jboolean loadSample(JNIEnv *env, AAssetManager *amgr, jstring filename) {
    const char *utf8 = env->GetStringUTFChars(filename, NULL);
//    AAsset *asset = AAssetManager_open(amgr, utf8, AASSET_MODE_UNKNOWN);

    auto decoder = new AudioDecoder(engine.engine, utf8, __deviceSampleRate);
    decoder->start();
    LOGD("preload %s", utf8);

    __pcmCache.insert(std::make_pair(utf8, decoder->getResult()));

    delete decoder;

    env->ReleaseStringUTFChars(filename, utf8);

//    AAsset_close(asset);
    return JNI_TRUE;
}

void destroyOutputMix(AudioOutputMix *out) {
    DESTROY(out->object);
}

void destroyEngine(AudioEngine *eng) {
    DESTROY(eng->object);
    eng->engine = NULL;
}

}
