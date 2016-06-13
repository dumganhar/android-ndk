#define RIFF_TAG    "RIFF"
#define WAVE_TAG    "WAVE"
#define FMT__TAG    "fmt "
#define DATA_TAG    "data"
 
// generic chunk header...
typedef struct {
    unsigned int id;
    unsigned int size;
} chunkHeader;
 
// format chunk...
typedef struct {
    chunkHeader hdr;
    unsigned short audioFormat;
    unsigned short numChannels;
    unsigned int sampleRate;
    unsigned int byteRate;
    unsigned short blockAlign;
    unsigned short bitsPerSample;
} fmtChunk;
 
// wav header...
typedef struct {
    chunkHeader riff;
    unsigned int wave;
    fmtChunk fmt_;
    chunkHeader data;
} wavHeader;
 
// for checking 4cc tags...
static inline int checkTag(const char* tag, const char* data) {
    return tag[0] == data[0] 
            && tag[1] == data[1] 
            && tag[2] == data[2] 
            && tag[3] == data[3];
}