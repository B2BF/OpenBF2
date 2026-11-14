
// BF2Audio.dll only. Without BF2_IMAGE_BF2AUDIO this file compiles to nothing: the recursive
// sweeps that build BF2.exe and bf2_w32ded.exe (CMake's bf2_client_objects glob, build.cmd's
// all-TU census) must not carry the DLL's engine: its DllMain, its two exports and its own null
// Sound/SoundTemplate pair would collide with the host's, which loads this image at run time.
// Same convention as
// Code/BF2/RendDX9/BF2Render.cpp's BF2_IMAGE_RENDDX9 guard. Build with Code\Build\bf2audio.cmd.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "SoundFile.h"
#include "OpenALSample.h"
#include "BfSoundEngine.h"
#include "SoundTrace.h"

#include "../../../RendDX9Abi/BfMemory.h"

#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace dice {
namespace hfe {

namespace {
const char* const kFile = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\SoundFile.cpp";
const char* const kVendor = "Xiph.Org libVorbis I 20030909";
}

int g_loadedSampleBytes = 0;    // ds:10048E38
int g_alBufferCount     = 0;    // ds:10048E3C
int g_streamedBytes     = 0;    // ds:10048E40
int g_loadedFileCount   = 0;    // ds:10048E44

const int g_qualityRates[4] = { 44100, 22050, 11025, 0 };

const char* ovErrorString(int code)
{
    switch (code)
    {
    case -138: return "OV_ENOSEEK";
    case -137: return "OV_EBADLINK";
    case -136: return "OV_EBADPACKET";
    case -135: return "OV_ENOTAUDIO";
    case -134: return "OV_EVERSION";
    case -133: return "OV_EBADHEADER";
    case -132: return "OV_ENOTVORBIS";
    case -131: return "OV_EINVAL";
    case -130: return "OV_EIMPL";
    case -129: return "OV_EFAULT";
    case -128: return "OV_EREAD";
    case -3:   return "OV_HOLE";
    case -2:   return "OV_EOF";
    case -1:   return "OV_FALSE";
    default:   return "unknown";
    }
}

SoundFile::SoundFile(const std::string& name)
    : m_oggLoopInfo(0), m_failCount(0), m_name(name), m_loaded(false), m_refCount(0), m_usageCount(0),
      m_type(0), m_numSamples(0), m_blockAlign(0), m_loopStart(0), m_loopEnd(-1), m_loadedBytes(0),
      m_bufferIntro(0), m_bufferLoop(0), m_bufferOutro(0), m_qualityMask(0), m_flags2(0)
{
    m_format.formatTag = 0; m_format.data = 0; m_format.dataBytes = 0; m_format.samplesPerSecond = 0;
    m_format.bitsPerSample = 0; m_format.channels = 0; m_format.numSamples = 0; m_format.blockAlign = 0;
    m_format.reserved = 0;
    m_sampleInfo.introSeconds = 0.0f; m_sampleInfo.loopSeconds = 0.0f; m_sampleInfo.outroSeconds = 0.0f;
}

SoundFile::~SoundFile()
{
    unload();
}

int SoundFile::addRef()
{
    return ++m_refCount;
}

int SoundFile::release()
{
    const int r = --m_refCount;
    if (r <= 0)
    {
        delete this;
        return 0;
    }
    if (r == 1)
        m_usageCount = 0;
    return r;
}

// "used" is either a usage count or a reference beyond the engine's own.
bool SoundFile::isUsed() const
{
    if (g_soundProperties->get(SoundProperties::SoundfileUseUsageRefcount) <= 0.0f)
        return m_refCount > 1;
    return m_usageCount > 0;
}

bool SoundFile::isWantedAtQuality() const
{
    return (m_qualityMask & (1u << g_soundEngineInstance->getQualityLevel())) != 0;
}

// at least five characters, and the last three match `ext` case-insensitively.
bool SoundFile::extensionIs(const char* ext) const
{
    const unsigned int n = (unsigned int) m_name.size();
    if (n < 5)
        return false;
    for (unsigned int i = 0; i < 3; ++i)
        if (ext[i] != std::tolower((unsigned char) m_name[n - 3 + i]))
            return false;
    return true;
}

SoundFile::MemoryFile* SoundFile::createMemoryFile() const
{
    MemoryFile* m = new MemoryFile;
    std::memset(m, 0, sizeof(MemoryFile));
    m->data     = m_format.data;
    m->size     = m_format.dataBytes;
    m->position = 0;
    return m;
}

void SoundFile::destroyMemoryFile(OggVorbis_File* vf)
{
    delete (MemoryFile*) vf;
}

// See the header: quality wanted, used, not yet loaded, then by extension.
// A file whose loop block is shorter than half a second behind a non-zero intro is refused
// with an error and left unloaded - and since that path returns before the fail counter moves,
// the error repeats on every attempt.
bool SoundFile::load(int sampleRate, int bits)
{
    (void) sampleRate; (void) bits;
    if (!g_soundEngineInstance->isInitialized())
        return false;
    const int quality = g_soundEngineInstance->getQualityLevel();
    if (!((1u << quality) & m_qualityMask))
    {
        m_failCount = 0;
        return quality != 0;
    }
    const bool unused = g_soundProperties->get(SoundProperties::SoundfileUseUsageRefcount) <= 0.0f
                      ? m_refCount <= 1 : m_usageCount <= 0;
    if (unused || m_loaded)
    {
        m_failCount = 0;
        return !unused;
    }
    int loopStart = 0;
    int loopEnd   = 0;
    bool ok;
    if (extensionIs("wav"))
        ok = loadWav(&loopStart, &loopEnd);
    else if (extensionIs("ogg"))
        ok = loadOgg(&loopStart, &loopEnd);
    else
        ok = false;
    if (!ok)
    {
        if (!m_failCount)
        {
            ok = m_name != "";
            if (ok)
            {
                g_soundTrace->error(sndFormat("Failed to open/parse %s.", m_name.c_str()), kFile, 316);
                ++m_failCount;
            }
        }
        return ok;
    }
    m_blockAlign = m_format.blockAlign;
    m_numSamples = m_format.numSamples;
    m_loopStart  = loopStart;
    m_loopEnd    = loopEnd;
    int end = loopEnd;
    if (end < 0)
        end = m_numSamples;
    const float rate = (float) (unsigned int) m_format.samplesPerSecond;
    m_sampleInfo.introSeconds = (float) loopStart / rate;
    m_sampleInfo.loopSeconds  = (float) (end - loopStart) / rate;
    m_sampleInfo.outroSeconds = (float) (m_numSamples - end) / rate;
    if (m_sampleInfo.introSeconds > 0.0f && m_sampleInfo.loopSeconds < 0.5f)
    {
        g_soundTrace->warning(sndFormat("%s has a loop-block which is shorter than %f seconds. This is not allowed if the sample has a start-block (i.e. the first loop-point is > 0).",
                                      m_name.c_str(), 0.5),
                            kFile, 353);
        return false;
    }
    ++g_loadedFileCount;
    ok = upload();
    if (ok)
        m_loaded = true;
    m_failCount = 0;
    return ok;
}

// Parse the RIFF, then decimate to the quality's rate when the file is faster:
// mono keeps every `factor`-th frame; stereo keeps the left channel of every `factor`-th frame,
// so a decimated stereo file comes out mono. 16-bit is assumed throughout.
bool SoundFile::loadWav(int* loopStart, int* loopEnd)
{
    m_type = 1;
    const bool ok = parseWavFile(m_name, &m_format, loopStart, loopEnd);
    int targetRate = g_qualityRates[g_soundEngineInstance->getQualityLevel()];
    if (targetRate < 22050 && (m_flags2 & 1))
        targetRate = 22050;
    if (!ok)
        return ok;
    const unsigned int rate = (unsigned int) m_format.samplesPerSecond;
    if (rate <= (unsigned int) targetRate)
        return ok;
    const int factor = (int) (rate / (unsigned int) targetRate);
#if !defined(_WIN32)
    if (std::getenv("BF2_SOUND_TRACE"))                                // [TEMP] BF2_SOUND_TRACE
        std::fprintf(stderr, "[SND] load %s: %d Hz x%d -> %d Hz mono (factor %d, quality %d)\n", m_name.c_str(),
                     m_format.samplesPerSecond, m_format.channels, targetRate, factor,
                     g_soundEngineInstance->getQualityLevel());
#endif
    if (loopStart) *loopStart /= factor;
    if (loopEnd)   *loopEnd   /= factor;
    const int newSamples = (int) ((unsigned int) m_format.numSamples / (unsigned int) factor);
    short* out = (short*) renddx9abi::engineAlloc(2 * newSamples);
    const short* in = (const short*) m_format.data;
    if (m_format.channels == 1)
    {
        for (int i = 0; i < newSamples; ++i)
            out[i] = in[i * factor];
    }
    else if (m_format.channels == 2)
    {
        for (int i = 0; i < newSamples; ++i)
            out[i] = in[i * 2 * factor];
    }
    renddx9abi::engineFree(m_format.data);
    m_format.numSamples       = newSamples;
    m_format.dataBytes        = 2 * newSamples;
    m_format.data             = (char*) out;
    m_format.channels         = 1;
    m_format.samplesPerSecond = targetRate;
    m_format.blockAlign       = 2;
    return ok;
}

namespace {
// the memory-file callbacks (off_1003F370).
size_t oggRead(void* dst, size_t size, size_t count, void* source)
{
    SoundFile::MemoryFile* m = (SoundFile::MemoryFile*) source;
    unsigned int bytes = (unsigned int) (size * count);
    if (m->position >= m->size)
        return 0;
    if ((int) (m->position + bytes) >= m->size)
        bytes = (unsigned int) (m->size - m->position);
    std::memcpy(dst, m->data + m->position, bytes);
    m->position += (int) bytes;
    return bytes;
}
int oggSeek(void* source, ogg_int64_t offset, int whence)
{
    SoundFile::MemoryFile* m = (SoundFile::MemoryFile*) source;
    const int off = (int) offset;
    if (whence == 0) { if (m->size < off) return -1; m->position = off; return 0; }
    if (whence == 1) { if (m->size < m->position + off) return -1; m->position += off; return 0; }
    if (whence == 2) { m->position = m->size - off; return 0; }
    return 0;
}
int  oggClose(void* source) { ((SoundFile::MemoryFile*) source)->position = 0; return 0; }
long oggTell(void* source)  { return ((SoundFile::MemoryFile*) source)->position; }
}

// Read the whole file into memory, open it once to validate the vendor and
// read the stream parameters, close the decoder again; the bytes stay for the streaming samples.
// The vendor-mismatch and no-comment paths leave the temporary decoder unclosed, as shipped.
bool SoundFile::loadOgg(int* loopStart, int* loopEnd)
{
    bool  ok   = false;
    char* data = 0;
    void* handle = g_soundFileIO->open(m_name.c_str());
    if (handle)
    {
        const int size = g_soundFileIO->seek(handle, 0, 2);
        g_soundFileIO->seek(handle, 0, 0);
        if (size)
        {
            data = (char*) renddx9abi::engineAlloc(size);
            if (g_soundFileIO->read(handle, data, size) == size)
            {
                MemoryFile mem;
                std::memset(&mem, 0, sizeof(mem));
                mem.data = data; mem.size = size; mem.position = 0;
                ov_callbacks callbacks = { oggRead, oggSeek, oggClose, oggTell };
                const int err = ov_open_callbacks(&mem, &mem.vf, 0, 0, callbacks);
                if (err)
                {
                    g_soundTrace->warning(sndFormat("Failed to open vorbis file: %s", ovErrorString(err)), kFile, 462);
                }
                else
                {
                    vorbis_comment* comment = ov_comment(&mem.vf, -1);
                    if (comment)
                    {
                        if (!std::strcmp(comment->vendor, kVendor))
                        {
                            vorbis_info* info = ov_info(&mem.vf, -1);
                            m_format.formatTag        = 0;
                            m_format.samplesPerSecond = (int) info->rate;
                            m_format.bitsPerSample    = 16;
                            m_format.data             = data;
                            m_format.channels         = info->channels;
                            m_format.dataBytes        = size;
                            m_format.blockAlign       = 16 * info->channels / 8;
                            m_format.numSamples       = (int) ov_pcm_total(&mem.vf, -1);
                            m_format.reserved         = 0;
                            data = 0;
                            ov_clear(&mem.vf);
                            *loopStart = 0;
                            *loopEnd   = m_format.numSamples;
                            m_type = 2;
                            g_streamedBytes += m_format.dataBytes;
                            ok = true;
                        }
                        else
                        {
                            g_soundTrace->error(sndFormat("Invalid ogg-file vendor: %s for file %s. Must be: %s",
                                                          comment->vendor, m_name.c_str(), kVendor), kFile, 478);
                        }
                    }
                    else
                    {
                        g_soundTrace->warning(sndFormat("Failed to get vorbis file comment: %s", ovErrorString(0)), kFile, 469);
                    }
                }
            }
        }
    }
    renddx9abi::engineFree(data);
    if (handle)
        g_soundFileIO->close(handle);
    return ok;
}

// A streaming file has nothing to upload. Otherwise the loop block always gets
// a buffer; the intro and outro blocks get theirs unless the quality level is LOW and the file has
// an intro, in which case only a file whose loop runs to the end keeps them (the intro/outro of a
// three-block file are dropped at low quality). The PCM is freed once OpenAL has it.
bool SoundFile::upload()
{
    if (m_type == 2)
        return true;
    const int   rate     = m_format.samplesPerSecond;
    char* const data     = m_format.data;
    const int   bpf      = m_blockAlign;
    const int   introEnd = bpf * m_loopStart;
    const int   loopEnd  = bpf * m_loopEnd;
    bool splitAll = true;
    if (g_soundEngineInstance->getQualityLevel() == 2 && introEnd > 0)
        splitAll = loopEnd == m_format.dataBytes;
    genBuffersAutomatic(1, &m_bufferLoop);
    ++g_alBufferCount;
    if (splitAll)
    {
        if (introEnd > 0)
        {
            genBuffersAutomatic(1, &m_bufferIntro);
            ++g_alBufferCount;
        }
        if (loopEnd != m_format.dataBytes)
        {
            genBuffersAutomatic(1, &m_bufferOutro);
            ++g_alBufferCount;
        }
    }
    m_loadedBytes = 0;
    const ALenum format = (bpf == 2) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    if (m_bufferIntro)
    {
        al_BufferData(m_bufferIntro, format, data, introEnd, rate);
        if (!checkOpenALError(m_name.c_str()))
            return false;
        m_loadedBytes += introEnd;
    }
    if (m_bufferLoop)
    {
        al_BufferData(m_bufferLoop, format, data + introEnd, loopEnd - introEnd, rate);
        if (!checkOpenALError(m_name.c_str()))
            return false;
        m_loadedBytes += loopEnd - introEnd;
    }
    if (m_bufferOutro)
    {
        const int outroBytes = m_format.dataBytes - loopEnd;
        al_BufferData(m_bufferOutro, format, data + loopEnd, outroBytes, rate);
        if (!checkOpenALError(m_name.c_str()))
            return false;
        m_loadedBytes += outroBytes;
    }
    g_loadedSampleBytes += m_loadedBytes;
    renddx9abi::engineFree(m_format.data);
    m_format.data = 0;
    return true;
}

void SoundFile::releaseBuffers()
{
    if (m_type == 2)
        return;
    if (m_bufferIntro) { al_DeleteBuffers(1, &m_bufferIntro); --g_alBufferCount; }
    if (m_bufferLoop)  { al_DeleteBuffers(1, &m_bufferLoop);  --g_alBufferCount; }
    if (m_bufferOutro) { al_DeleteBuffers(1, &m_bufferOutro); --g_alBufferCount; }
    m_bufferOutro = 0;
    m_bufferIntro = 0;
    m_bufferLoop  = 0;
    if (m_loaded)
    {
        g_loadedSampleBytes -= m_loadedBytes;
        m_loadedBytes = 0;
    }
}

// everything back to the constructor's state except the name and the counts.
void SoundFile::unload()
{
    if (m_format.data && m_type == 2)
        g_streamedBytes -= m_format.dataBytes;
    renddx9abi::engineFree(m_format.data);
    m_format.data = 0;
    releaseBuffers();
    m_loaded      = false;
    m_type        = 0;
    m_numSamples  = 0;
    m_loopStart   = 0;
    m_blockAlign  = 0;
    m_bufferOutro = 0;
    m_bufferLoop  = 0;
    m_bufferIntro = 0;
    m_loadedBytes = 0;
    m_oggLoopInfo = 0;
    m_loopEnd     = -1;
    m_format.formatTag = 0; m_format.data = 0; m_format.dataBytes = 0; m_format.samplesPerSecond = 0;
    m_format.bitsPerSample = 0; m_format.channels = 0; m_format.numSamples = 0; m_format.blockAlign = 0;
    m_format.reserved = 0;
    m_sampleInfo.introSeconds = 0.0f; m_sampleInfo.loopSeconds = 0.0f; m_sampleInfo.outroSeconds = 0.0f;
}

// Unload when the caller says so for this file's kind, or when nobody uses it.
void SoundFile::unloadIfUnused(bool unloadStatic, bool unloadStreams)
{
    if (!m_loaded)
        return;
    const bool unused = g_soundProperties->get(SoundProperties::SoundfileUseUsageRefcount) <= 0.0f
                      ? m_refCount <= 1 : m_usageCount <= 0;
    const bool go = (m_type == 2 ? unloadStreams : unloadStatic) || unused;
    if (go)
    {
        --g_loadedFileCount;
        unload();
    }
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_BF2AUDIO
