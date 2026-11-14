
// Shared sound-file resource, looked up by normalized filename.


#ifndef SOUND_SOUNDENGINE_SOUNDFILE_H
#define SOUND_SOUNDENGINE_SOUNDFILE_H

#include "WavFileFormat.h"
#include "BfOpenAL.h"

#include <string>

#include <vorbis/vorbisfile.h>

namespace dice {
namespace hfe {

// The three seconds the client reads through ISound::getSampleInfo (SoundFile +0x70).
struct SampleInfo
{
    float introSeconds;
    float loopSeconds;
    float outroSeconds;
};

// Loaded sound data and provider upload state; x86 layout is 140 bytes.
class SoundFile
{
public:
    explicit SoundFile(const std::string& name);
    virtual int  addRef();                          // +0x00
    virtual int  release();                         // +0x04 the last release deletes;
                                                    // the second-to-last clears the usage count
    virtual ~SoundFile();                           // +0x08

    void  setIsUsed()   { ++m_usageCount; }
    void  clearUsage()  { m_usageCount = 0; }
    bool  isUsed() const;
    bool  isWantedAtQuality() const;                // the current quality's bit
    bool  load(int sampleRate, int bits);           // (the two arguments are unused)
    void  unloadIfUnused(bool unloadStatic, bool unloadStreams);
    void  unload();
    void  releaseBuffers();
    bool  upload();                                 // the PCM into OpenAL buffers
    bool  loadWav(int* loopStart, int* loopEnd);
    bool  loadOgg(int* loopStart, int* loopEnd);
    bool  extensionIs(const char* ext) const;       // the last three letters, lower-cased

    // the decoder block StreamingSample opens on this file's in-memory .ogg -
    // an OggVorbis_File followed by the memory reader's {data, size, position}. 0x2E8 bytes in the
    // image; here whatever upstream's OggVorbis_File measures plus twelve.
    struct MemoryFile
    {
        OggVorbis_File vf;
        char* data;
        int   size;
        int   position;
    };
    MemoryFile*  createMemoryFile() const;
    static void  destroyMemoryFile(OggVorbis_File* vf);

    void*        m_oggLoopInfo;     // +0x04 dice's cached decoder tables; always null here
                                    // (see OpenALSample.cpp's header block)
    int          m_failCount;       // +0x08 "Failed to open/parse" is logged once per file
    std::string  m_name;            // +0x0C
    bool         m_loaded;          // +0x28
    int          m_refCount;        // +0x2C
    int          m_usageCount;      // +0x30
    int          m_type;            // +0x34 0 unloaded, 1 static (.wav), 2 streaming (.ogg)
    int          m_numSamples;      // +0x38
    int          m_blockAlign;      // +0x3C bytes per sample frame
    int          m_loopStart;       // +0x40 sample index
    int          m_loopEnd;         // +0x44 sample index, -1 until loaded
    WavFormat    m_format;          // +0x48
    int          m_loadedBytes;     // +0x6C what upload() handed OpenAL
    SampleInfo   m_sampleInfo;      // +0x70
    ALuint       m_bufferIntro;     // +0x7C
    ALuint       m_bufferLoop;      // +0x80
    ALuint       m_bufferOutro;     // +0x84
    unsigned int m_qualityMask;     // +0x88 bit q: quality level q wants this file
    unsigned char m_flags2;         // +0x8A bit 0: never below 22 kHz (the template had flags)
};

// libvorbis' OV_* codes by name, "unknown" otherwise.
const char* ovErrorString(int code);

// ds:the stats block reports all four.
extern int g_loadedSampleBytes;
extern int g_alBufferCount;
extern int g_streamedBytes;
extern int g_loadedFileCount;

// .rdata the sample rate each quality level decimates .wav files to (0 = high 44100,
// 1 = medium 22050, 2 = low 11025; the fourth entry is 0).
extern const int g_qualityRates[4];

} // namespace hfe
} // namespace dice

#endif // SOUND_SOUNDENGINE_SOUNDFILE_H
