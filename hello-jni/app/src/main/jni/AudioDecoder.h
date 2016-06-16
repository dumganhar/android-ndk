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
* on the UrlAudioPlayer object for decoding, SL_IID_METADATAEXTRACTION for retrieving the
* format of the decoded audio */
#define NUM_EXPLICIT_INTERFACES_FOR_PLAYER 3

/* Size of the decode buffer queue */
#define NB_BUFFERS_IN_QUEUE 4
/* Size of each buffer in the queue */
#define BUFFER_SIZE_IN_SAMPLES 1024 // number of samples per MP3 frame
#define BUFFER_SIZE_IN_BYTES   (BUFFER_SIZE_IN_SAMPLES)


class AudioDecoder {
public:

    struct Result
    {
        std::shared_ptr<std::vector<char>> pcmBuffer;
        int numChannels;
        int sampleRate;
        int bitsPerSample;
        int containerSize;
        int channelMask;
        int endianness;
        int numFrames;

        Result()
        {
            reset();
        }

        void reset()
        {
            numChannels = -1;
            sampleRate = -1;
            bitsPerSample = -1;
            containerSize = -1;
            channelMask = -1;
            endianness = -1;
            numFrames = -1;
            pcmBuffer = NULL;
        }

        std::string toString()
        {
            std::string ret;
            char buf[256] = {0};

            snprintf(buf, sizeof(buf),
                     "numChannels: %d, sampleRate: %d, bitPerSample: %d, containerSize: %d, channelMask: %d, endianness: %d, numFrames: %d",
                     numChannels, sampleRate, bitsPerSample, containerSize, channelMask, endianness, numFrames
            );

            ret = buf;
            return ret;
        }
    };

    AudioDecoder(SLEngineItf engineItf, const std::string& url, int sampleRate);
    virtual ~AudioDecoder();

    void start();

    inline Result getResult() { return _result; };

private:
    void resample();
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
    int _numChannelsKeyIndex;
    int _sampleRateKeyIndex;
    int _bitsPerSampleKeyIndex;
    int _containerSizeKeyIndex;
    int _channelMaskKeyIndex;
    int _endiannessKeyIndex;

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
    int _sampleRate;

    friend class SLAudioDecoderCallbackProxy;
};


#endif //HELLO_JNI_AUDIODECODER_H
