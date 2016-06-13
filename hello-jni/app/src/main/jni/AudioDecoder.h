//
// Created by James Chen on 6/13/16.
//

#ifndef HELLO_JNI_AUDIODECODER_H
#define HELLO_JNI_AUDIODECODER_H

#include <vector>
#include <string>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <mutex>
#include <condition_variable>


/* Explicitly requesting SL_IID_ANDROIDSIMPLEBUFFERQUEUE and SL_IID_PREFETCHSTATUS
* on the AudioPlayer object for decoding, SL_IID_METADATAEXTRACTION for retrieving the
* format of the decoded audio */
#define NUM_EXPLICIT_INTERFACES_FOR_PLAYER 3

/* Size of the decode buffer queue */
#define NB_BUFFERS_IN_QUEUE 4
/* Size of each buffer in the queue */
#define BUFFER_SIZE_IN_SAMPLES 2048 // number of samples per MP3 frame
#define BUFFER_SIZE_IN_BYTES   (BUFFER_SIZE_IN_SAMPLES)


class AudioDecoder {
public:

    struct Result
    {
        Result()
        : channelCount(-1)
        , sampleRate(-1)
        {
        }

        std::shared_ptr<std::vector<char>> pcmBuffer;
        int channelCount;
        int sampleRate;
    };

    AudioDecoder(SLEngineItf engineItf, const std::string& url);
    virtual ~AudioDecoder();

    void start();

    inline const Result& getResult() { return _result; };

private:
    void signalEos();
    void decodeToPcmCallback(SLAndroidSimpleBufferQueueItf queueItf);
    void prefetchCallback( SLPrefetchStatusItf caller, SLuint32 event);
    void decodeProgressCallback(SLPlayItf caller, SLuint32 event);

private:
    SLEngineItf _engineItf;
    std::string _url;
    Result _result;

    /* Local storage for decoded audio data */
    char _pcmData[NB_BUFFERS_IN_QUEUE * BUFFER_SIZE_IN_BYTES];

    /* used to query metadata values */
    SLMetadataInfo* _pcmMetaData;
    /* we only want to query / display the PCM format once */
    bool _formatQueried;
    /* Used to signal prefetching failures */
    bool _prefetchError;

    /* to display the number of decode iterations */
    int _counter;

    /* metadata key index for the PCM format information we want to retrieve */
    int _channelCountKeyIndex;
    int _sampleRateKeyIndex;

    /* to signal to the test app the end of the stream to decode has been reached */
    bool _eos;
    std::mutex _eosLock;
    std::condition_variable _eosCondition;

    /* Structure for passing information to callback function */
    typedef struct CallbackCntxt_ {
        SLPlayItf playItf;
        SLMetadataExtractionItf metaItf;
        SLuint32  size;
        SLint8*   pDataBase;    // Base address of local audio data storage
        SLint8*   pData;        // Current address of local audio data storage
    } CallbackCntxt;

    CallbackCntxt _decContext;

    friend class SLAudioDecoderCallbackProxy;
};


#endif //HELLO_JNI_AUDIODECODER_H
