#include <sys/types.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "audio.h"

#include "audio/android/AudioPlayerProvider.h"

#include <chrono>
#include <audio/android/ICallerThreadUtils.h>
#include <mutex>

#define LOG_TAG "audio.cpp"

#define AUDIO_FUNC(NAME)    Java_com_example_hellojni_HelloJni_##NAME

using namespace cocos2d::experimental;

static AudioEngine engine;
static AudioOutputMix output;
static AAssetManager* __assetManager;
static AudioPlayerProvider* __audioPlayerProvider = nullptr;

static std::vector<std::function<void()>> __functionsToPerform;
static std::mutex __performMutex;

static int __fileIndex = 0;
static std::string __currentFilePath;

#define clockNow() std::chrono::high_resolution_clock::now()
#define intervalInMS(oldTime, newTime) (static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>((newTime) - (oldTime)).count()) / 1000.f)

static int fdGetter(const std::string& url, off_t* start, off_t* length)
{
    ALOGV("in the callback of fdgetter, url: %s", url.c_str());
    int ret = 0;
    AAsset* asset = AAssetManager_open(__assetManager, url.c_str(), AASSET_MODE_UNKNOWN);
    ALOG_ASSERT(asset != nullptr, "AAssetManager_open (%s) return nullptr!", url.c_str());
    // open asset as file descriptor
    ret = AAsset_openFileDescriptor(asset, start, length);
    ALOG_ASSERT(ret > 0, "AAset_openFileDescriptor (%s) failed!", url.c_str());
    AAsset_close(asset);

    return ret;
}



extern "C" {

jboolean loadSample(JNIEnv *, AAssetManager *, jstring);
void destroyOutputMix(AudioOutputMix *);
void destroyEngine(AudioEngine *);

JNIEXPORT void JNICALL
Java_com_example_hellojni_HelloJni_jniOnPause(JNIEnv *env, jobject instance) {
    if (__audioPlayerProvider != nullptr)
    {
        __audioPlayerProvider->pause();
    }
}

JNIEXPORT void JNICALL
Java_com_example_hellojni_HelloJni_jniOnResume(JNIEnv *env, jobject instance) {
    if (__audioPlayerProvider != nullptr)
    {
        __audioPlayerProvider->resume();
    }
};

JNIEXPORT void JNICALL
Java_com_example_hellojni_HelloJni_jniOnUpdate(JNIEnv *env, jobject instance) {
// Testing size is faster than locking / unlocking.
    // And almost never there will be functions scheduled to be called.
    if( !__functionsToPerform.empty() ) {
        __performMutex.lock();
        // fixed #4123: Save the callback functions, they must be invoked after '_performMutex.unlock()', otherwise if new functions are added in callback, it will cause thread deadlock.
        auto temp = __functionsToPerform;
        __functionsToPerform.clear();
        __performMutex.unlock();
        for( const auto &function : temp ) {
            function();
        }
    }
};

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniCreate)(JNIEnv *env, jclass clazz, jobject asset_man, jint sampleRate, jint bufferSizeInFrames) {
    __assetManager = AAssetManager_fromJava(env, asset_man);

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

    class CallerThreadUtils : public ICallerThreadUtils
    {
    public:
        virtual void performFunctionInCallerThread(const std::function<void()>& func) {
            __performMutex.lock();

            __functionsToPerform.push_back(func);

            __performMutex.unlock();
        };
    };
    static CallerThreadUtils __callerThreadUtils;

    __functionsToPerform.reserve(30);
    __audioPlayerProvider = new (std::nothrow) AudioPlayerProvider(engine.engine, output.object, sampleRate, bufferSizeInFrames, fdGetter, &__callerThreadUtils);

    return JNI_TRUE;
}

JNIEXPORT
void
JNICALL
AUDIO_FUNC(jniShutdown)(JNIEnv *env, jclass clazz) {
    delete __audioPlayerProvider;
    __audioPlayerProvider = nullptr;

    destroyOutputMix(&output);
    destroyEngine(&engine);
}

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniLoadSamples)(JNIEnv *env, jclass clazz, jobjectArray files) {

    int len = env->GetArrayLength(files);

    static int __counter = 0;
    auto oldTime = clockNow();

    for (int i = 0; i < len; ++i)
    {
        jstring filename = (jstring) env->GetObjectArrayElement(files, i);
        jboolean loaded = loadSample(env, __assetManager, filename);

        //test begin
//        std::string url = "test/A1-Guitar1-1.mp3";
//        off_t start, length;
//        int ret;
//
//        ALOGV("AAssetManager_open url: %s, %d times", url.c_str(), __counter);
//        AAsset* asset = AAssetManager_open(__assetManager, url.c_str(), AASSET_MODE_UNKNOWN);
//        ALOG_ASSERT(asset != nullptr, "AAssetManager_open (%s) return nullptr!", url.c_str());
//        // open asset as file descriptor
//        ret = AAsset_openFileDescriptor(asset, &start, &length);
//        ALOG_ASSERT(ret > 0, "AAset_openFileDescriptor (%s) failed!", url.c_str());
//        AAsset_close(asset);
//        ::close(ret);
        //test end

        env->DeleteLocalRef(filename);
        if (!loaded) {
            ALOGE("jniLoadSamples failed");
            return JNI_FALSE;
        }
        __counter++;
    }

    auto nowTime = clockNow();

    ALOGV("Preloading all samples wastes: %fms", intervalInMS(oldTime, nowTime));

    return JNI_TRUE;
}

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniPlaySample)(JNIEnv *env, jclass clazz, jint index, jboolean play_state) {

    if (__fileIndex > 28)
    {
        __fileIndex = 0;
    }
    char filePath[256] = {0};
    sprintf(filePath, "%02d.mp3", __fileIndex);
    __currentFilePath = filePath;

    ++__fileIndex;

    std::string p = __currentFilePath;
//    p = "test/B9-Vilolins-8.mp3";

    for (int i = 0; i < 10; ++i)
    {
        __audioPlayerProvider->preloadEffect(p, [=](bool succeed, PcmData data) {
            ALOGV("%d, preload (%s), succeed: %d, isValid: %d", i, p.c_str(), succeed, data.isValid());
        });
    }

    auto oldTime = clockNow();
    auto player = __audioPlayerProvider->getAudioPlayer(__currentFilePath);
    if (player != nullptr) {
        player->play();
    } else {
        ALOGE("Oops, player is null ...");
    }

    auto newTime = clockNow();
    ALOGV("play waste: %fms", intervalInMS(oldTime, newTime));
    return 1;
}

jboolean loadSample(JNIEnv *env, AAssetManager *amgr, jstring filename) {
    const char *utf8 = env->GetStringUTFChars(filename, NULL);

    ALOGV("loadSample: %s", utf8);

    std::string filePath = utf8;

    static int totalSuccessCount = 0;
    static std::chrono::high_resolution_clock::time_point oldTime;

    if (totalSuccessCount == 0)
    {
        oldTime = clockNow();
    }

    __audioPlayerProvider->preloadEffect(utf8, [filePath](bool succeed, PcmData data){
        ALOGD("preload (%s) return: isSucceed: %d, valid: %d, successcount: %d", filePath.c_str(), succeed, data.isValid(), totalSuccessCount);
        totalSuccessCount++;

        if (totalSuccessCount == 312)
        {
            auto nowTime = clockNow();
            ALOGV("preloading all files wastes %fms", intervalInMS(oldTime, nowTime));
            totalSuccessCount = 0;
            oldTime = nowTime;
        }
    });

    env->ReleaseStringUTFChars(filename, utf8);

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
