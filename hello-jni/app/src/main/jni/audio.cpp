#include <sys/types.h>
#include <assert.h>
#include <memory.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "wav.h"
#include "audio.h"
#include "AudioDecoder.h"
#include "AudioPlayer.h"
#include <android/log.h>
#include <stdio.h>
#include <string>

#define LOG_TAG "cjh"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

#define AUDIO_FUNC(NAME)    Java_com_example_hellojni_HelloJni_##NAME

static AudioEngine engine;
static AudioOutputMix output;

#define MAX_SAMPLE_PLAYERS      (16)
static SamplePlayer sounds[MAX_SAMPLE_PLAYERS];
static unsigned int round_robin_player_index = 0;

#define MAX_SAMPLES             (32)
static Sample samples[MAX_SAMPLES];
static unsigned int num_samples_loaded = 0;

extern "C" {

jboolean loadSample(JNIEnv *, AAssetManager *, jstring, Sample *);
jboolean parseWav(AAsset *, Sample *);
SamplePlayer *findAvailablePlayer();
void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
void unloadSamplePlayers();
void unloadSamples();
void destroySamplePlayer(SamplePlayer *);
void destroySample(Sample *);
void destroyOutputMix(AudioOutputMix *);
void destroyEngine(AudioEngine *);

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniCreate)(JNIEnv *env, jclass clazz) {
    SLresult res;
    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    res = slCreateEngine(&engine.object, 1, EngineOption, 0, NULL, NULL);
    // for brevity we omit checking 'res' and other errors.
    // a full implementation checks for errors throughout.
    res = (*engine.object)->Realize(engine.object, SL_BOOLEAN_FALSE);
    res = (*engine.object)->GetInterface(engine.object, SL_IID_ENGINE, &engine.engine);
    res = (*engine.engine)->CreateOutputMix(engine.engine, &output.object, 0, NULL, NULL);
    res = (*output.object)->Realize(output.object, SL_BOOLEAN_FALSE);
    return JNI_TRUE;
}

JNIEXPORT
void
JNICALL
AUDIO_FUNC(jniShutdown)(JNIEnv *env, jclass clazz) {
    unloadSamplePlayers();
    unloadSamples();
    destroyOutputMix(&output);
    destroyEngine(&engine);
}

JNIEXPORT
jboolean
JNICALL
AUDIO_FUNC(jniLoadSamples)(JNIEnv *env, jclass clazz, jobject asset_man, jobjectArray files) {
    AAssetManager *amgr = AAssetManager_fromJava(env, asset_man);
    int len = env->GetArrayLength(files);
    // feel free to implement your own sample management layer,
    // here i simply unload all samples beforehand.
    unloadSamples();
    for (int i = 0; i < len; ++i) {
        jstring filename = (jstring) env->GetObjectArrayElement(files, i);
        Sample *sample = &samples[num_samples_loaded];
        jboolean loaded = loadSample(env, amgr, filename, sample);
        if (!loaded) {
            return JNI_FALSE;
        }
        ++num_samples_loaded;
    }
    return JNI_TRUE;
}

static int playSample(int index, int state) {
    if (index < 0/* || index >= num_samples_loaded*/) {
        return 0;
    }
    Sample *sample = &samples[index];

//    typedef struct SLDataFormat_PCM_ {
//        SLuint32 		formatType;
//        SLuint32 		numChannels;
//        SLuint32 		samplesPerSec;
//        SLuint32 		bitsPerSample;
//        SLuint32 		containerSize;
//        SLuint32 		channelMask;
//        SLuint32		endianness;
//    } SLDataFormat_PCM;

    SLuint32 channelMask = SL_SPEAKER_FRONT_CENTER;

    if (sample->num_channels == 2) {
        channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    }

    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            sample->num_channels,
            sample->sample_rate * 1000,           // implement these!
            (SLuint32) sample->bytes_per_sample * 8,   //
            16,
            channelMask,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            1
    };
    SLDataSource source = {&loc_bufq, &format_pcm};

    SLDataLocator_OutputMix loc_outmix = {
            SL_DATALOCATOR_OUTPUTMIX,
            output.object
    };
    SLDataSink sink = {&loc_outmix, NULL};

#undef NUM
#define NUM (2)

    const SLInterfaceID ids[NUM] = {
            SL_IID_VOLUME,
            SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
    };
    const SLboolean req[NUM] = {
            SL_BOOLEAN_TRUE,
            SL_BOOLEAN_TRUE,
    };

    SamplePlayer *sp = findAvailablePlayer();
    SLresult res;

    LOGD("audio.cpp, before CreateAudioPlayer ...");
    res = (*engine.engine)->CreateAudioPlayer(engine.engine, &sp->object, &source, &sink, NUM, ids,
                                              req);
    LOGD("audio.cpp, after CreateAudioPlayer ...");

    res = (*sp->object)->Realize(sp->object, SL_BOOLEAN_FALSE);
    res = (*sp->object)->GetInterface(sp->object, SL_IID_PLAY, &sp->play);
    res = (*sp->object)->GetInterface(sp->object, SL_IID_VOLUME, &sp->volume);
    res = (*sp->object)->GetInterface(sp->object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                      &sp->buffer_queue);
    res = (*sp->buffer_queue)->RegisterCallback(sp->buffer_queue, samplePlayerCallback,
                                                (void *) sp);
    res = (*sp->buffer_queue)->Enqueue(sp->buffer_queue, sample->buffer, byteSize(sample));
    res = (*sp->play)->SetPlayState(sp->play, state ? SL_PLAYSTATE_PLAYING : SL_PLAYSTATE_PAUSED);
    return 1;
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

    auto decoder = new AudioDecoder(engine.engine, __currentFilePath);
    decoder->start();

    const AudioDecoder::Result& r = decoder->getResult();

    auto player = new AudioPlayer(engine.engine, output.object, r);
    player->play();

    delete player;

    delete decoder;

    return 1;
}

jboolean loadSample(JNIEnv *env, AAssetManager *amgr, jstring filename, Sample *sample) {
    assert(num_samples_loaded < MAX_SAMPLES);
    const char *utf8 = env->GetStringUTFChars(filename, NULL);
    AAsset *asset = AAssetManager_open(amgr, utf8, AASSET_MODE_UNKNOWN);
    env->ReleaseStringUTFChars(filename, utf8);
    jboolean parsed = parseWav(asset, sample);
    AAsset_close(asset);
    return parsed ? JNI_TRUE : JNI_FALSE;
}

jboolean parseWav(AAsset *asset, Sample *sample) {
    assert(asset && sample);

    wavHeader wav_hdr;
    int is_ok;
    int bytes_read;
    char *read_ptr = (char *) &wav_hdr;

    bytes_read = AAsset_read(asset, read_ptr, sizeof(wav_hdr.riff) + sizeof(wav_hdr.wave));
    is_ok = (bytes_read > 0);
    is_ok &= checkTag(RIFF_TAG, (char *) &wav_hdr.riff.id);
    is_ok &= checkTag(WAVE_TAG, (char *) &wav_hdr.wave);
    if (!is_ok) {
        return JNI_FALSE;
    }
    read_ptr += bytes_read;

    bytes_read = AAsset_read(asset, read_ptr, sizeof(wav_hdr.fmt_));
    is_ok = (bytes_read > 0);
    is_ok &= checkTag(FMT__TAG, (char *) &wav_hdr.fmt_.hdr.id);
    is_ok &= (wav_hdr.fmt_.hdr.size == 0x10);
    is_ok &= (wav_hdr.fmt_.audioFormat == 1);
    if (!is_ok) {
        return JNI_FALSE;
    }
    read_ptr += bytes_read;

    bytes_read = AAsset_read(asset, read_ptr, sizeof(wav_hdr.data));
    while (!checkTag(DATA_TAG, (char *) &wav_hdr.data.id)) {
        off_t pos = AAsset_seek(asset, wav_hdr.data.size, 1);   // SEEK_CUR in stdio.h
        bytes_read = AAsset_read(asset, read_ptr, sizeof(wav_hdr.data));
    }
    is_ok = (bytes_read > 0);
    if (!is_ok) {
        return JNI_FALSE;
    }
    read_ptr += bytes_read;

    // *** required data has been successfully copied to the wavHeader ***

    if (wav_hdr.data.size & 0x1) {
        wav_hdr.data.size += 1;
    }

    sample->buffer = (char *) malloc(wav_hdr.data.size);
    if (!sample->buffer) {
        return JNI_FALSE;
    }

    bytes_read = AAsset_read(asset, sample->buffer, wav_hdr.data.size);
    is_ok = (bytes_read == wav_hdr.data.size);
    if (!is_ok) {
        destroySample(sample);
        return JNI_FALSE;
    }

    sample->bytes_per_sample = wav_hdr.fmt_.bitsPerSample >> 3; // bits to bytes
    sample->num_channels = wav_hdr.fmt_.numChannels;
    sample->num_samples = bytes_read / sample->num_channels / sample->bytes_per_sample;
    sample->sample_rate = wav_hdr.fmt_.sampleRate;

    return JNI_TRUE;
}

SamplePlayer *findAvailablePlayer() {
    // rotating index, basically 'kill oldest'...
    round_robin_player_index %= MAX_SAMPLE_PLAYERS;
    SamplePlayer *sp = &sounds[round_robin_player_index++];
    destroySamplePlayer(sp);
    return sp;
}

void samplePlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    LOGD("samplePlayerCallback, context = %p ...", context);
    SamplePlayer *sp = (SamplePlayer *) context;
    SLresult res;
    res = (*sp->play)->SetPlayState(sp->play, SL_PLAYSTATE_STOPPED);
    res = (*sp->buffer_queue)->Clear(sp->buffer_queue);
}

void unloadSamplePlayers() {
    for (int i = 0; i < MAX_SAMPLE_PLAYERS; ++i) {
        destroySamplePlayer(&sounds[i]);
    }
    round_robin_player_index = 0;
}

void unloadSamples() {
    for (int i = 0; i < MAX_SAMPLES; ++i) {
        destroySample(&samples[i]);
    }
    num_samples_loaded = 0;
}

void destroySamplePlayer(SamplePlayer *sp) {
    DESTROY(sp->object);
    sp->play = NULL;
    sp->volume = NULL;
    sp->buffer_queue = NULL;
}

void destroySample(Sample *s) {
    free(s->buffer);
    s->buffer = NULL;
    s->num_samples = 0;
    s->sample_rate = 0;
    s->num_channels = 0;
    s->bytes_per_sample = 0;
}

void destroyOutputMix(AudioOutputMix *out) {
    DESTROY(out->object);
}

void destroyEngine(AudioEngine *eng) {
    DESTROY(eng->object);
    eng->engine = NULL;
}

}

void playPcm(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        return;
    }

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buf = (char*)malloc(fileSize);
    fread(buf, 1, fileSize, fp);
    fclose(fp);

    LOGD("playPcm ( %s )..., buf size=%d", path, (int) fileSize);
//    playPcmBuffer(buf, fileSize);
}

void playPcmBuffer(const char* buf, int bufSize, int channelCount, int sampleRate)
{
    char* newBuf = (char*) malloc(bufSize);
    memcpy(newBuf, buf, bufSize);

    LOGD("bufSize: %d", bufSize);
    if (num_samples_loaded >= MAX_SAMPLES) {
        num_samples_loaded = 0;
    }

    Sample* sample = &samples[num_samples_loaded];
    sample->buffer = (char*)newBuf;

    sample->bytes_per_sample = 2;
    sample->num_channels = channelCount;
    sample->num_samples = bufSize / sample->num_channels / sample->bytes_per_sample;
    sample->sample_rate = sampleRate;

    playSample(num_samples_loaded, 1);
    num_samples_loaded++;
}