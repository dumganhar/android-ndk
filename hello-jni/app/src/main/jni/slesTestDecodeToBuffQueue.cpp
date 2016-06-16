/*
 * Copyright (C) 2011 The Android Open Source Project
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

/* Audio Decode Test

First run the program from shell:
  # slesTest_decodeToBuffQueue /sdcard/myFile.mp3 4

These use adb on host to retrieve the decoded file:
  % adb pull /sdcard/myFile.mp3.raw myFile.raw

How to examine the output with Audacity:
 Project / Import raw data
 Select myFile.raw file, then click Open button
 Choose these options:
  Signed 16-bit PCM
  Little-endian
  1 Channel (Mono) / 2 Channels (Stereo) based on the selected file
  Sample rate same as the selected file
 Click Import button

*/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <android/log.h>
#include "AudioDecoder.h"

#include "audio.h"
#include "UrlAudioPlayer.h"

#define LOG_TAG "cjh"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)

/* Explicitly requesting SL_IID_ANDROIDSIMPLEBUFFERQUEUE and SL_IID_PREFETCHSTATUS
 * on the UrlAudioPlayer object for decoding, SL_IID_METADATAEXTRACTION for retrieving the
 * format of the decoded audio */
#define NUM_EXPLICIT_INTERFACES_FOR_PLAYER 3

/* Size of the decode buffer queue */
#define NB_BUFFERS_IN_QUEUE 4
/* Size of each buffer in the queue */
#define BUFFER_SIZE_IN_SAMPLES 2048 // number of samples per MP3 frame
#define BUFFER_SIZE_IN_BYTES   (BUFFER_SIZE_IN_SAMPLES)

/* Local storage for decoded audio data */
int8_t pcmData[NB_BUFFERS_IN_QUEUE * BUFFER_SIZE_IN_BYTES];

/* destination for decoded data */
static FILE* gFp;

/* to display the number of decode iterations */
static int counter=0;

/* metadata key index for the PCM format information we want to retrieve */
static int channelCountKeyIndex = -1;
static int sampleRateKeyIndex = -1;

int channelCount = -1;
int sampleRate = -1;

/* size of the struct to retrieve the PCM format metadata values: the values we're interested in
 * are SLuint32, but it is saved in the data field of a SLMetadataInfo, hence the larger size.
 * Nate that this size is queried and displayed at l.452 for demonstration/test purposes.
 *  */
#define PCM_METADATA_VALUE_SIZE 32
/* used to query metadata values */
static SLMetadataInfo *pcmMetaData = NULL;
/* we only want to query / display the PCM format once */
static bool formatQueried = false;

/* to signal to the test app the end of the stream to decode has been reached */
static bool eos = false;
static std::mutex eosLock;
static std::condition_variable eosCondition;

static std::vector<char> __pcmBuffer;

/* used to detect errors likely to have occured when the OpenSL ES framework fails to open
 * a resource, for instance because a file URI is invalid, or an HTTP server doesn't respond.
 */
#define PREFETCHEVENT_ERROR_CANDIDATE \
        (SL_PREFETCHEVENT_STATUSCHANGE | SL_PREFETCHEVENT_FILLLEVELCHANGE)

//-----------------------------------------------------------------
/* Exits the application if an error is encountered */
#define ExitOnError(x) ExitOnErrorFunc(x,__LINE__)

void ExitOnErrorFunc( SLresult result , int line)
{
    if (SL_RESULT_SUCCESS != result) {
        LOGE("Error code %u encountered at line %d, exiting\n", result, line);
        exit(EXIT_FAILURE);
    }
}

/* Used to signal prefetching failures */
bool prefetchError = false;

//-----------------------------------------------------------------
/* Structure for passing information to callback function */
typedef struct CallbackCntxt_ {
    SLPlayItf playItf;
    SLMetadataExtractionItf metaItf;
    SLuint32  size;
    SLint8*   pDataBase;    // Base address of local audio data storage
    SLint8*   pData;        // Current address of local audio data storage
} CallbackCntxt;

//-----------------------------------------------------------------
void SignalEos() {
    std::unique_lock<std::mutex> autoLock(eosLock);
    eos = true;
    eosCondition.notify_one();
}

//-----------------------------------------------------------------
/* Callback for "prefetch" events, here used to detect audio resource opening errors */
void PrefetchEventCallback( SLPrefetchStatusItf caller,  void *pContext __unused, SLuint32 event)
{
    SLpermille level = 0;
    SLresult result;
    result = (*caller)->GetFillLevel(caller, &level);
    ExitOnError(result);
    SLuint32 status;
    //LOGD("PrefetchEventCallback: received event %u\n", event);
    result = (*caller)->GetPrefetchStatus(caller, &status);
    ExitOnError(result);
    if ((PREFETCHEVENT_ERROR_CANDIDATE == (event & PREFETCHEVENT_ERROR_CANDIDATE))
            && (level == 0) && (status == SL_PREFETCHSTATUS_UNDERFLOW)) {
        LOGD("PrefetchEventCallback: Error while prefetching data, exiting\n");
        prefetchError = true;
        SignalEos();
    }
}

/* Callback for "playback" events, i.e. event happening during decoding */
void DecProgressCallback(
        SLPlayItf caller,
        void *pContext __unused,
        SLuint32 event)
{
    SLresult result;
    SLmillisecond msec;
    result = (*caller)->GetPosition(caller, &msec);
    ExitOnError(result);

    if (SL_PLAYEVENT_HEADATEND & event) {
        LOGD("SL_PLAYEVENT_HEADATEND current position=%u ms\n", msec);
        SignalEos();
    }

    if (SL_PLAYEVENT_HEADATNEWPOS & event) {
        LOGD("SL_PLAYEVENT_HEADATNEWPOS current position=%u ms\n", msec);
    }

    if (SL_PLAYEVENT_HEADATMARKER & event) {
        LOGD("SL_PLAYEVENT_HEADATMARKER current position=%u ms\n", msec);
    }
}

//-----------------------------------------------------------------
/* Callback for decoding buffer queue events */
void DecPlayCallback(
        SLAndroidSimpleBufferQueueItf queueItf,
        void *pContext)
{
    counter++;

    CallbackCntxt *pCntxt = (CallbackCntxt*)pContext;

    if (counter % 1000 == 0) {
        SLmillisecond msec;
        SLresult result = (*pCntxt->playItf)->GetPosition(pCntxt->playItf, &msec);
        ExitOnError(result);
        LOGD("DecPlayCallback called (iteration %d): current position=%u ms\n", counter, msec);
    }

    __pcmBuffer.insert(__pcmBuffer.end(), pCntxt->pData, pCntxt->pData + BUFFER_SIZE_IN_BYTES);

    /* Save the decoded data  */
    if (fwrite(pCntxt->pData, 1, BUFFER_SIZE_IN_BYTES, gFp) < BUFFER_SIZE_IN_BYTES) {
        LOGE("Error writing to output file, signaling EOS\n");
        SignalEos();
        return;
    }


    ExitOnError( (*queueItf)->Enqueue(queueItf, pCntxt->pData, BUFFER_SIZE_IN_BYTES) );

    /* Increase data pointer by buffer size */
    pCntxt->pData += BUFFER_SIZE_IN_BYTES;

    if (pCntxt->pData >= pCntxt->pDataBase + (NB_BUFFERS_IN_QUEUE * BUFFER_SIZE_IN_BYTES)) {
        pCntxt->pData = pCntxt->pDataBase;
    }

    // Note: adding a sleep here or any sync point is a way to slow down the decoding, or
    //  synchronize it with some other event, as the OpenSL ES framework will block until the
    //  buffer queue callback return to proceed with the decoding.

#if 1
    /* Example: buffer queue state display */
    SLAndroidSimpleBufferQueueState decQueueState;
    ExitOnError( (*queueItf)->GetState(queueItf, &decQueueState) );

    LOGD("DecBufferQueueCallback now has pCntxt->pData=%p, pCntxt->pDataBase=%p, queue: "
            "count=%u playIndex=%u, count: %d\n",
            pCntxt->pData, pCntxt->pDataBase, decQueueState.count, decQueueState.index, counter);
#endif

#if 0
    /* Example: display duration in callback where we use the callback context for the SLPlayItf*/
    SLmillisecond durationInMsec = SL_TIME_UNKNOWN;
    SLresult result = (*pCntxt->playItf)->GetDuration(pCntxt->playItf, &durationInMsec);
    ExitOnError(result);
    if (durationInMsec == SL_TIME_UNKNOWN) {
        LOGD("Content duration is unknown (in dec callback)\n");
    } else {
        LOGD("Content duration is %ums (in dec callback)\n",
                durationInMsec);
    }
#endif

#if 0
    /* Example: display position in callback where we use the callback context for the SLPlayItf*/
    SLmillisecond posMsec = SL_TIME_UNKNOWN;
    result = (*pCntxt->playItf)->GetPosition(pCntxt->playItf, &posMsec);
    ExitOnError(result);
    if (posMsec == SL_TIME_UNKNOWN) {
        LOGD("Content position is unknown (in dec callback)\n");
    } else {
        LOGD("Content position is %ums (in dec callback)\n",
                posMsec);
    }
#endif

    /* Example: query of the decoded PCM format */
    if (formatQueried) {
        return;
    }
    SLresult res = (*pCntxt->metaItf)->GetValue(pCntxt->metaItf, sampleRateKeyIndex,
            PCM_METADATA_VALUE_SIZE, pcmMetaData);  ExitOnError(res);
    // Note: here we could verify the following:
    //         pcmMetaData->encoding == SL_CHARACTERENCODING_BINARY
    //         pcmMetaData->size == sizeof(SLuint32)
    //       but the call was successful for the PCM format keys, so those conditions are implied
    LOGD("sample rate = %dHz, ", *((SLuint32*)pcmMetaData->data));
    sampleRate = *((SLuint32*)pcmMetaData->data);
    res = (*pCntxt->metaItf)->GetValue(pCntxt->metaItf, channelCountKeyIndex,
            PCM_METADATA_VALUE_SIZE, pcmMetaData);  ExitOnError(res);
    LOGD(" channel count = %d\n", *((SLuint32*)pcmMetaData->data));
    channelCount = *((SLuint32*)pcmMetaData->data);
    formatQueried = true;
}

//-----------------------------------------------------------------

/* Decode an audio path by opening a file descriptor on that path  */
void TestDecToBuffQueue( SLObjectItf sl, const char* path)
{
    size_t len = strlen((const char *) path);
    char* outputPath = (char*) malloc(len + 4 + 1); // save room to concatenate ".raw"

    if (NULL == outputPath) {
        ExitOnError(SL_RESULT_RESOURCE_ERROR);
    }
    memcpy(outputPath, path, len + 1);
    strcat(outputPath, ".raw");

    LOGD("outputPath: %s, len=%d", outputPath, len);

    gFp = fopen(outputPath, "wb");
    if (NULL == gFp) {
        ExitOnError(SL_RESULT_RESOURCE_ERROR);
    }

    SLresult  result;
    SLEngineItf EngineItf;

    /* Objects this application uses: one audio player */
    SLObjectItf  player;

    /* Interfaces for the audio player */
    SLAndroidSimpleBufferQueueItf decBuffQueueItf;
    SLPrefetchStatusItf           prefetchItf;
    SLPlayItf                     playItf;
    SLMetadataExtractionItf       mdExtrItf;

    /* Source of audio data for the decoding */
    SLDataSource      decSource;
    SLDataLocator_URI decUri;
    SLDataFormat_MIME decMime;

    /* Data sink for decoded audio */
    SLDataSink                decDest;
    SLDataLocator_AndroidSimpleBufferQueue decBuffQueue;
    SLDataFormat_PCM          pcm;

    SLboolean required[NUM_EXPLICIT_INTERFACES_FOR_PLAYER];
    SLInterfaceID iidArray[NUM_EXPLICIT_INTERFACES_FOR_PLAYER];

    /* Get the SL Engine Interface which is implicit */
    result = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void*)&EngineItf);
    ExitOnError(result);

    /* Initialize arrays required[] and iidArray[] */
    for (int i=0 ; i < NUM_EXPLICIT_INTERFACES_FOR_PLAYER ; i++) {
        required[i] = SL_BOOLEAN_FALSE;
        iidArray[i] = SL_IID_NULL;
    }

    /* allocate memory to receive the PCM format metadata */
    if (!pcmMetaData) {
        pcmMetaData = (SLMetadataInfo*) malloc(PCM_METADATA_VALUE_SIZE);
    }

    formatQueried = false;

    /* ------------------------------------------------------ */
    /* Configuration of the player  */

    /* Request the AndroidSimpleBufferQueue interface */
    required[0] = SL_BOOLEAN_TRUE;
    iidArray[0] = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    /* Request the PrefetchStatus interface */
    required[1] = SL_BOOLEAN_TRUE;
    iidArray[1] = SL_IID_PREFETCHSTATUS;
    /* Request the PrefetchStatus interface */
    required[2] = SL_BOOLEAN_TRUE;
    iidArray[2] = SL_IID_METADATAEXTRACTION;

    /* Setup the data source */
    decUri.locatorType = SL_DATALOCATOR_URI;
    decUri.URI = (SLchar*)path;
    decMime.formatType = SL_DATAFORMAT_MIME;
    /*     this is how ignored mime information is specified, according to OpenSL ES spec
     *     in 9.1.6 SLDataFormat_MIME and 8.23 SLMetadataTraversalItf GetChildInfo */
    decMime.mimeType      = (SLchar*)NULL;
    decMime.containerType = SL_CONTAINERTYPE_UNSPECIFIED;
    decSource.pLocator = (void *) &decUri;
    decSource.pFormat  = (void *) &decMime;

    /* Setup the data sink */
    decBuffQueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    decBuffQueue.numBuffers = NB_BUFFERS_IN_QUEUE;
    /*    set up the format of the data in the buffer queue */
    pcm.formatType = SL_DATAFORMAT_PCM;
    // FIXME valid value required but currently ignored
    pcm.numChannels = 1;
    pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
    pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.containerSize = 16;
    pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

    decDest.pLocator = (void *) &decBuffQueue;
    decDest.pFormat = (void * ) &pcm;

    LOGD("before player created!");
    /* Create the audio player */
    result = (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &decSource, &decDest,
                                             NUM_EXPLICIT_INTERFACES_FOR_PLAYER, iidArray, required);
    ExitOnError(result);
    LOGD("Player created\n");

    /* Realize the player in synchronous mode. */
    result = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    ExitOnError(result);
    LOGD("Player realized\n");

    /* Get the play interface which is implicit */
    result = (*player)->GetInterface(player, SL_IID_PLAY, (void*)&playItf);
    ExitOnError(result);

    /* Set up the player callback to get events during the decoding */
    // FIXME currently ignored
    result = (*playItf)->SetMarkerPosition(playItf, 2000);
    ExitOnError(result);
    result = (*playItf)->SetPositionUpdatePeriod(playItf, 500);
    ExitOnError(result);
    result = (*playItf)->SetCallbackEventsMask(playItf,
            SL_PLAYEVENT_HEADATMARKER | SL_PLAYEVENT_HEADATNEWPOS | SL_PLAYEVENT_HEADATEND);
    ExitOnError(result);
    result = (*playItf)->RegisterCallback(playItf, DecProgressCallback, NULL);
    ExitOnError(result);
    LOGD("Play callback registered\n");

    /* Get the buffer queue interface which was explicitly requested */
    result = (*player)->GetInterface(player, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            (void*)&decBuffQueueItf);
    ExitOnError(result);

    /* Get the prefetch status interface which was explicitly requested */
    result = (*player)->GetInterface(player, SL_IID_PREFETCHSTATUS, (void*)&prefetchItf);
    ExitOnError(result);

    /* Get the metadata extraction interface which was explicitly requested */
    result = (*player)->GetInterface(player, SL_IID_METADATAEXTRACTION, (void*)&mdExtrItf);
    ExitOnError(result);

    /* ------------------------------------------------------ */
    /* Initialize the callback and its context for the decoding buffer queue */
    CallbackCntxt cntxt;
    cntxt.playItf = playItf;
    cntxt.metaItf = mdExtrItf;
    cntxt.pDataBase = (int8_t*)&pcmData;
    cntxt.pData = cntxt.pDataBase;
    cntxt.size = sizeof(pcmData);
    result = (*decBuffQueueItf)->RegisterCallback(decBuffQueueItf, DecPlayCallback, &cntxt);
    ExitOnError(result);

    /* Enqueue buffers to map the region of memory allocated to store the decoded data */
    LOGD("Enqueueing buffer ");
    for(int i = 0 ; i < NB_BUFFERS_IN_QUEUE ; i++) {
        LOGD("%d ", i);
        result = (*decBuffQueueItf)->Enqueue(decBuffQueueItf, cntxt.pData, BUFFER_SIZE_IN_BYTES);
        ExitOnError(result);
        cntxt.pData += BUFFER_SIZE_IN_BYTES;
    }
    LOGD("\n");
    cntxt.pData = cntxt.pDataBase;

    /* ------------------------------------------------------ */
    /* Initialize the callback for prefetch errors, if we can't open the resource to decode */
    result = (*prefetchItf)->RegisterCallback(prefetchItf, PrefetchEventCallback, &prefetchItf);
    ExitOnError(result);
    result = (*prefetchItf)->SetCallbackEventsMask(prefetchItf, PREFETCHEVENT_ERROR_CANDIDATE);
    ExitOnError(result);

    /* ------------------------------------------------------ */
    /* Prefetch the data so we can get information about the format before starting to decode */
    /*     1/ cause the player to prefetch the data */
    result = (*playItf)->SetPlayState( playItf, SL_PLAYSTATE_PAUSED );
    ExitOnError(result);
    /*     2/ block until data has been prefetched */
    SLuint32 prefetchStatus = SL_PREFETCHSTATUS_UNDERFLOW;
    SLuint32 timeOutIndex = 500; // time out prefetching after 5s
    while ((prefetchStatus != SL_PREFETCHSTATUS_SUFFICIENTDATA) && (timeOutIndex > 0) &&
            !prefetchError) {
        usleep(10 * 1000);
        (*prefetchItf)->GetPrefetchStatus(prefetchItf, &prefetchStatus);
        timeOutIndex--;
    }
    if (timeOutIndex == 0 || prefetchError) {
        LOGE("Failure to prefetch data in time, exiting\n");
        ExitOnError(SL_RESULT_CONTENT_NOT_FOUND);
    }

    /* ------------------------------------------------------ */
    /* Display duration */
    SLmillisecond durationInMsec = SL_TIME_UNKNOWN;
    result = (*playItf)->GetDuration(playItf, &durationInMsec);
    ExitOnError(result);
    if (durationInMsec == SL_TIME_UNKNOWN) {
        LOGD("Content duration is unknown\n");
    } else {
        LOGD("Content duration is %ums\n", durationInMsec);
    }

    /* ------------------------------------------------------ */
    /* Display the metadata obtained from the decoder */
    //   This is for test / demonstration purposes only where we discover the key and value sizes
    //   of a PCM decoder. An application that would want to directly get access to those values
    //   can make assumptions about the size of the keys and their matching values (all SLuint32)
    SLuint32 itemCount;
    result = (*mdExtrItf)->GetItemCount(mdExtrItf, &itemCount);
    SLuint32 i, keySize, valueSize;
    SLMetadataInfo *keyInfo, *value;
    for(i=0 ; i<itemCount ; i++) {
        keyInfo = NULL; keySize = 0;
        value = NULL;   valueSize = 0;
        result = (*mdExtrItf)->GetKeySize(mdExtrItf, i, &keySize);
        ExitOnError(result);
        result = (*mdExtrItf)->GetValueSize(mdExtrItf, i, &valueSize);
        ExitOnError(result);
        keyInfo = (SLMetadataInfo*) malloc(keySize);
        if (NULL != keyInfo) {
            result = (*mdExtrItf)->GetKey(mdExtrItf, i, keySize, keyInfo);
            ExitOnError(result);
            LOGD("key[%d] size=%d, name=%s \tvalue size=%d \n",
                    i, keyInfo->size, keyInfo->data, valueSize);
            /* find out the key index of the metadata we're interested in */
            if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_NUMCHANNELS)) {
                channelCountKeyIndex = i;
            } else if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_SAMPLERATE)) {
                sampleRateKeyIndex = i;
            }
            free(keyInfo);
        }
    }
    if (channelCountKeyIndex != -1) {
        LOGD("Key %s is at index %d\n",
                ANDROID_KEY_PCMFORMAT_NUMCHANNELS, channelCountKeyIndex);
    } else {
        LOGE("Unable to find key %s\n", ANDROID_KEY_PCMFORMAT_NUMCHANNELS);
    }
    if (sampleRateKeyIndex != -1) {
        LOGD("Key %s is at index %d\n",
                ANDROID_KEY_PCMFORMAT_SAMPLERATE, sampleRateKeyIndex);
    } else {
        LOGE("Unable to find key %s\n", ANDROID_KEY_PCMFORMAT_SAMPLERATE);
    }

    /* ------------------------------------------------------ */
    /* Start decoding */
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
    ExitOnError(result);
    LOGD("Starting to decode\n");

    /* Decode until the end of the stream is reached */
    {
        std::unique_lock<std::mutex> autoLock(eosLock);
        while (!eos) {
            eosCondition.wait(autoLock);
        }
    }
    LOGD("EOS signaled\n");

    /* ------------------------------------------------------ */
    /* End of decoding */

    /* Stop decoding */
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
    ExitOnError(result);
    LOGD("Stopped decoding\n");

    /* Destroy the UrlAudioPlayer object */
    (*player)->Destroy(player);

    fclose(gFp);

    free(pcmMetaData);
    pcmMetaData = NULL;
}

/*
 * 00.mp3 播放有问题，感觉播放时间太短
 */
#define TEST_FILENAME "/sdcard/09.mp3"

static int __fileIndex = 0;
static std::string __currentFilePath;

void testPlayPCM()
{
    std::string rawPath = __currentFilePath + ".raw";
//    playPcm(rawPath.c_str());
    char* buf = (char*) malloc(__pcmBuffer.size());
    memcpy(buf, __pcmBuffer.data(), __pcmBuffer.size());
//    playPcmBuffer(buf, __pcmBuffer.size());
}

//-----------------------------------------------------------------
int sles_main()
{
    __pcmBuffer.clear();
    memset(pcmData, 0, sizeof(pcmData));
    gFp = NULL;
    counter=0;
    channelCountKeyIndex = -1;
    sampleRateKeyIndex = -1;
    pcmMetaData = NULL;
    formatQueried = false;
    eos = false;

    SLresult    result;
    SLObjectItf sl;

//    LOGD("OpenSL ES test %s: exercises SLPlayItf and SLAndroidSimpleBufferQueueItf ",
//            argv[0]);
//    LOGD("on an UrlAudioPlayer object to decode a URI to PCM\n");
//
//    if (argc != 2) {
//        LOGD("Usage: \t%s source_file\n", argv[0]);
//        LOGD("Example: \"%s /sdcard/myFile.mp3\n", argv[0]);
//        exit(EXIT_FAILURE);
//    }

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    result = slCreateEngine( &sl, 1, EngineOption, 0, NULL, NULL);
    ExitOnError(result);

    /* Realizing the SL Engine in synchronous mode. */
    result = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
    ExitOnError(result);

    // for brevity we omit checking 'res' and other errors.
    // a full implementation checks for errors throughout.
    SLEngineItf engine;
    SLObjectItf outputMix;
    result = (*sl)->GetInterface(sl, SL_IID_ENGINE, &engine);
    result = (*engine)->CreateOutputMix(engine, &outputMix, 0, NULL, NULL);
    result = (*outputMix)->Realize(outputMix, SL_BOOLEAN_FALSE);

    if (__fileIndex > 28) {
        __fileIndex = 0;
    }
    char filePath[256] = {0};
    sprintf(filePath, "/sdcard/%02d.mp3", __fileIndex);
    __currentFilePath = filePath;
    ++__fileIndex;

    //TestDecToBuffQueue(sl, __currentFilePath.c_str());

//    auto decoder = new AudioDecoder(sl, __currentFilePath);
//    decoder->start();
//
//    const AudioDecoder::Result& r = decoder->getResult();
//
////    playPcmBuffer(r.pcmBuffer->data(), r.pcmBuffer->size(), r.channelCount, r.sampleRate);
//
//    auto player = new UrlAudioPlayer(engine, outputMix, r);
//    player->play();
//
////    delete player;
//    delete decoder;

    /* Shutdown OpenSL ES */
    (*sl)->Destroy(sl);


//    testPlayPCM();

    return EXIT_SUCCESS;
}
