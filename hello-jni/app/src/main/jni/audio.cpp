#include <sys/types.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "audio.h"

#include "AudioPlayerProvider.h"

#include <set>
#include <android/log.h>

#define LOG_TAG "cjh"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

#define AUDIO_FUNC(NAME)    Java_com_example_hellojni_HelloJni_##NAME

static AudioEngine engine;
static AudioOutputMix output;
static AAssetManager* __assetManager;
static AudioPlayerProvider* __audioPlayerProvider = nullptr;

static std::set<IAudioPlayer*> __audioPlayers;

static int __fileIndex = 0;
static std::string __currentFilePath;

static int fdGetter(const std::string& url, off_t* start, off_t* length)
{
    LOGD("in the callback of fdgetter ...");
    int ret = 0;
    auto asset = AAssetManager_open(__assetManager, url.c_str(), AASSET_MODE_UNKNOWN);
    // open asset as file descriptor
    ret = AAsset_openFileDescriptor(asset, start, length);
    AAsset_close(asset);

    return ret;
};

extern "C" {

jboolean loadSample(JNIEnv *, AAssetManager *, jstring);
void destroyOutputMix(AudioOutputMix *);
void destroyEngine(AudioEngine *);

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniCreate)(JNIEnv *env, jclass clazz, jint sampleRate, jint bufferSizeInFrames) {
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

    __audioPlayerProvider = new AudioPlayerProvider(engine.engine, output.object, sampleRate, bufferSizeInFrames, fdGetter);

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

static void clearNotValidUrlAudioPlayers()
{
    std::vector<IAudioPlayer*> toRemovePlayers;
    for (auto player : __audioPlayers)
    {
        LOGD("delete player: %p", player);
        toRemovePlayers.push_back(player);
        delete player;
    }

    for (auto toRemovePlayer : toRemovePlayers)
    {
        __audioPlayers.erase(toRemovePlayer);
    }
}

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniPlaySample)(JNIEnv *env, jclass clazz, jint index, jboolean play_state) {

    if (__fileIndex > 28) {
        __fileIndex = 0;
    }
    char filePath[256] = {0};
    sprintf(filePath, "/sdcard/%02d.mp3", __fileIndex);
    __currentFilePath = filePath;
//    __currentFilePath = "doorOpen.ogg";//filePath;

    ++__fileIndex;

    clearNotValidUrlAudioPlayers();

    auto player = __audioPlayerProvider->getAudioPlayer(__currentFilePath);
    if (player != nullptr) {
        if (!player->isOwnedByPool()) {
            __audioPlayers.insert(player);
        }
        player->play();
    } else {
        LOGE("Oops, player is null ...");
    }
    return 1;
}

jboolean loadSample(JNIEnv *env, AAssetManager *amgr, jstring filename) {
    const char *utf8 = env->GetStringUTFChars(filename, NULL);
//    AAsset *asset = AAssetManager_open(amgr, utf8, AASSET_MODE_UNKNOWN);

    __audioPlayerProvider->preloadEffect(utf8);

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
