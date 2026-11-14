// OpenAL sample playback.

// [PLATFORM] Uses upstream libvorbis, which parses headers on each stream start.
// The retail decoder caches shared codebooks through a patched API; that cache is not reproduced here.


// BF2Audio.dll only. Without BF2_IMAGE_BF2AUDIO this file compiles to nothing: the recursive
// sweeps that build BF2.exe and bf2_w32ded.exe (CMake's bf2_client_objects glob, build.cmd's
// all-TU census) must not carry the DLL's engine: its DllMain, its two exports and its own null
// Sound/SoundTemplate pair would collide with the host's, which loads this image at run time.
// Same convention as
// Code/BF2/RendDX9/BF2Render.cpp's BF2_IMAGE_RENDDX9 guard. Build with Code\Build\bf2audio.cmd.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "OpenALSample.h"
#include "Sound.h"
#include "SoundFile.h"
#include "BfSoundEngine.h"
#include "SoundTrace.h"
#include "SoundEngineMath.h"

#include <cmath>
#include <climits>
#include <cstring>
#if !defined(_WIN32)
#include <cstdio>
#include <cstdlib>
#endif

#include <vorbis/vorbisfile.h>

namespace dice {
namespace hfe {

namespace {
#if !defined(_WIN32)
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: macos-weapon-sound-validated]
// BF2_SOUND_TRACE=1 (or a substring of the sound file name, e.g. "fire"): one stderr line per
// voice start / stop and a few gain/pitch samples per voice. Read-only: no OpenAL state changes.
const char* soundTraceFilter()
{
    static const char* filter = std::getenv("BF2_SOUND_TRACE");
    return filter;
}
bool soundTraceWants(const SoundFile* file)
{
    const char* f = soundTraceFilter();
    if (!f || !*f)
        return false;
    if (!std::strcmp(f, "1"))
        return true;
    return file && file->m_name.find(f) != std::string::npos;
}
#endif
} // namespace


namespace {
const char* const kFile = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\OpenALSample.cpp";

// Process-wide streaming-buffer, source, and loop-info pools.
std::list<ALuint> s_streamingBufferPool;
std::list<ALuint> s_sourcePool;
std::list<void*>  s_loopInfoCache;

// ds:the one 8,192-byte decode buffer every StreamingSample streams through.
char s_streamBuffer[0x2000];
}

int g_streamingBufferBytes = 0;     // ds:10048DC8
int g_streamingBufferCount = 0;     // ds:10048DCC
int g_sourceCount          = 0;     // ds:10048DD0
int g_maxSources           = 0;     // ds:10048DD4

int linearToMillibel(float gain)
{
    if (gain <= 0.0001f)
        return -10000;
    if (gain < 0.9999f)
        return (int) (std::log(gain) * 868.58331f);      // 2000 / ln 10
    return 0;
}

// Pools

// With "use-source-pool" off the pool is bypassed and a source is generated on
// demand up to the voice limit. With it on, the first call pre-generates "min-free-sources"
// sources (capped by the limit) into the pool, and a pooled source is handed out while more than
// that many are idle; otherwise a fresh one is generated, again up to the limit.
ALuint acquireSource()
{
    unsigned int limit;
    if (g_soundProperties->get(SoundProperties::UseSourcePool) < 0.5f)
    {
        limit = (unsigned int) g_maxSources;
    }
    else
    {
        const unsigned int minFree = (unsigned int) (int) g_soundProperties->get(SoundProperties::MinFreeSources);
        if (g_sourceCount == 0)
        {
            for (unsigned int i = 0; ; ++i)
            {
                unsigned int cap = (unsigned int) g_maxSources;
                if (cap >= minFree)
                    cap = minFree;
                if (i >= cap)
                    break;
                ALuint s = 0;
                al_GenSources(1, &s);
                if (s)
                {
                    ++g_sourceCount;
                    releaseSourceToPool(s);
                }
            }
        }
        limit = (unsigned int) g_maxSources;
        if ((unsigned int) s_sourcePool.size() > minFree)
        {
            ALuint s = s_sourcePool.front();
            s_sourcePool.pop_front();
            return s;
        }
    }
    ALuint s = 0;
    if ((unsigned int) g_sourceCount < limit)
    {
        al_GenSources(1, &s);
        if (s)
            ++g_sourceCount;
    }
    return s;
}

// Rewind, detach the buffer, park it; and with the pool switched off, delete
// everything parked right away.
void releaseSourceToPool(ALuint source)
{
    if (source)
    {
        al_SourceRewind(source);
        al_Sourcei(source, AL_BUFFER, 0);
        s_sourcePool.push_back(source);
        if (g_soundProperties->get(SoundProperties::UseSourcePool) < 0.5f)
            purgeSourcePool();
    }
}

void purgeSourcePool()
{
    for (std::list<ALuint>::iterator it = s_sourcePool.begin(); it != s_sourcePool.end(); ++it)
    {
        ALuint s = *it;
        al_DeleteSources(1, &s);
        checkOpenALError("OpenALSample::purgeSourcePool");
        --g_sourceCount;
    }
    s_sourcePool.clear();
}

ALuint allocateStreamingBuffer()
{
    if (!s_streamingBufferPool.empty())
    {
        ALuint b = s_streamingBufferPool.front();
        s_streamingBufferPool.pop_front();
        return b;
    }
    ALuint b = 0;
    genBuffersAccessible(1, &b);
    checkOpenALError("StreamingSample::allocateStreamingBuffer");
    ++g_streamingBufferCount;
    g_streamingBufferBytes += 0x2000;
    return b;
}

void purgeStreamingBufferPool()
{
    for (std::list<ALuint>::iterator it = s_streamingBufferPool.begin(); it != s_streamingBufferPool.end(); ++it)
    {
        ALuint b = *it;
        al_DeleteBuffers(1, &b);
        checkOpenALError("StreamingSample::purgeStreamingBufferPool");
        --g_streamingBufferCount;
        g_streamingBufferBytes -= 0x2000;
    }
    s_streamingBufferPool.clear();
}

// See the header block: nothing is ever added in this tree.
void purgeLoopInfoCache()
{
    s_loopInfoCache.clear();
}

// The streaming helpers

// "StreamingHelper::bufferData". Decode one chunk (8 KB stereo, 4 KB mono,
// halved below 25 kHz and quartered below 12 kHz), looping back to the start when the stream ends
// and a loop is left; upload it to `alBuffer`. False when nothing was decoded or the file failed.
bool bufferData(OggVorbis_File* vf, SoundFile* file, char* buffer, int* loopsLeft, ALuint alBuffer, bool report)
{
    int chunk = file->m_format.channels != 2 ? 4096 : 0x2000;
    const unsigned int rate = (unsigned int) file->m_format.samplesPerSecond;
    if (rate < 12000)
        chunk /= 4;
    else if (rate < 25000)
        chunk /= 2;
    if (chunk <= 0)
        return false;
    int got = 0;
    int bitstream = 0;
    for (;;)
    {
        const long r = ov_read(vf, buffer + got, chunk - got, 0, 2, 1, &bitstream);
        if (r > 0)
        {
            got += (int) r;
        }
        else if (r == 0)
        {
            if (--*loopsLeft <= 0)
                break;
            ov_pcm_seek(vf, 0);
        }
        else
        {
            if (!report)
                return false;
            g_soundTrace->warning(sndFormat("Failed to read from ogg file %s. Error: %s",
                                          file->m_name.c_str(), ovErrorString((int) r)), kFile, 924);
            return false;
        }
        if (got >= chunk)
            break;
    }
    if (!got)
        return false;
    al_BufferData(alBuffer, file->m_format.channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,
                  buffer, got, (ALsizei) rate);
    if (report)
        return checkOpenALError("StreamingHelper::bufferData");
    return true;
}

// "SoundFileOgg::updateSourceBuffers". Either queue one new buffer (the first
// four starts) or recycle up to two processed ones; then, if the source ran dry and stopped while
// buffers are queued, restart it. Answers whether a NEW buffer was queued.
bool updateSourceBuffers(OggVorbis_File* vf, SoundFile* file, char* buffer, int* loopsLeft,
                         ALuint source, ALuint newBuffer, bool report)
{
    if (*loopsLeft <= 0)
        return false;
    bool queuedNew = false;
    ALuint b = 0;
    if (newBuffer)
    {
        b = newBuffer;
        queuedNew = true;
        if (bufferData(vf, file, buffer, loopsLeft, newBuffer, report))
        {
            al_SourceQueueBuffers(source, 1, &b);
            if (report)
                checkOpenALError("SoundFileOgg::updateSourceBuffers::queue new");
        }
    }
    else
    {
        ALint processed = 0;
        al_GetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
        int n = processed > 2 ? 2 : processed;
        while (n > 0)
        {
            --n;
            al_SourceUnqueueBuffers(source, 1, &b);
            if (report)
                checkOpenALError("SoundFileOgg::updateSourceBuffers::unqueue");
            if (!bufferData(vf, file, buffer, loopsLeft, b, report))
                break;
            al_SourceQueueBuffers(source, 1, &b);
            if (report)
                checkOpenALError("SoundFileOgg::updateSourceBuffers::queue");
        }
    }
    ALint queued = 0;
    al_GetSourcei(source, AL_BUFFERS_QUEUED, &queued);
    ALint state = 0;
    al_GetSourcei(source, AL_SOURCE_STATE, &state);
    if (queued > 0 && state == AL_STOPPED)
    {
        al_SourcePlay(source);
        if (report)
            checkOpenALError("SoundFileOgg::updateSourceBuffers::restart");
    }
    return queuedNew;
}

namespace {
// Callbacks for SoundFile::MemoryFile storage.
size_t memRead(void* dst, size_t size, size_t count, void* source)
{
    SoundFile::MemoryFile* m = (SoundFile::MemoryFile*) source;
    unsigned int bytes = (unsigned int) (size * count);
    if (m->position >= m->size)
        return 0;
    if ((int) (m->position + bytes) >= m->size)
        bytes = m->size - m->position;
    std::memcpy(dst, m->data + m->position, bytes);
    m->position += bytes;
    return bytes;
}
int memSeek(void* source, ogg_int64_t offset, int whence)
{
    SoundFile::MemoryFile* m = (SoundFile::MemoryFile*) source;
    const int off = (int) offset;
    switch (whence)
    {
    case 0:  if (m->size < off) return -1; m->position = off; return 0;
    case 1:  if (m->size < m->position + off) return -1; m->position += off; return 0;
    case 2:  m->position = m->size - off; return 0;
    default: return 0;
    }
}
int memClose(void* source) { ((SoundFile::MemoryFile*) source)->position = 0; return 0; }
long memTell(void* source)  { return ((SoundFile::MemoryFile*) source)->position; }
}

// Open the decoder on the first call (the SoundFile's in-memory copy is the
// data source), seek on later ones, decode the first chunk into `alBuffer` and queue it.
bool streamStart(OggVorbis_File** vfHolder, SoundFile* file, char* buffer, int* loopsLeft,
                 int seekSample, ALuint source, ALuint alBuffer, bool report)
{
    bool opened = false;
    if (!*vfHolder)
    {
        opened = true;
        SoundFile::MemoryFile* m = file->createMemoryFile();
        *vfHolder = &m->vf;
        ov_callbacks callbacks = { memRead, memSeek, memClose, memTell };
        const int err = ov_open_callbacks(m, &m->vf, 0, 0, callbacks);
        if (err)
        {
            if (report)
                g_soundTrace->error(sndFormat("ov_open_callbacks failed (%d)", err), kFile, 761);
            return false;
        }
        // (retail: `if (report && !file->m_oggLoopInfo) vf->loopInfo = findOrCreateLoopInfo(file, vf)` -
        // the codebook/seek-table cache handed to its patched libvorbis; see the header block.)
    }
    al_Sourcei(source, AL_LOOPING, 0);
    if (!opened && seekSample != -1)
        ov_pcm_seek(*vfHolder, seekSample);
    if (!bufferData(*vfHolder, file, buffer, loopsLeft, alBuffer, report))
        return false;
    al_SourceQueueBuffers(source, 1, &alBuffer);
    if (report)
        checkOpenALError("StreamingSample::startBuffers");
    return true;
}

// OpenALSample

// The EAX block's defaults are Creative's (occlusion LF ratio 0.25, room ratio
// 1.5, direct ratio 1, exclusion LF ratio 1, air absorption 1, flags directhf|room|roomhf auto).
OpenALSample::OpenALSample(const Vec3* position)
    : m_streaming(false), m_source(0), m_stopped(false), m_obstruction(0.0f)
{
    if (position)
        m_position = *position;
    else
    {
        m_position.x = 0.0f; m_position.y = 0.0f; m_position.z = 0.0f;
    }
    m_eax.lDirect = 0;               m_eax.lDirectHF = 0;              m_eax.lRoom = 0;
    m_eax.lRoomHF = 0;               m_eax.lObstruction = 0;           m_eax.flObstructionLFRatio = 0.0f;
    m_eax.lOcclusion = 0;            m_eax.flOcclusionLFRatio = 0.25f; m_eax.flOcclusionRoomRatio = 1.5f;
    m_eax.flOcclusionDirectRatio = 1.0f; m_eax.lExclusion = 0;        m_eax.flExclusionLFRatio = 1.0f;
    m_eax.lOutsideVolumeHF = 0;      m_eax.flDopplerFactor = 0.0f;     m_eax.flRolloffFactor = 0.0f;
    m_eax.flRoomRolloffFactor = 0.0f; m_eax.flAirAbsorptionFactor = 1.0f; m_eax.ulFlags = 7;
}

// nothing of its own - the subclasses give the source back first.
OpenALSample::~OpenALSample()
{
}

void OpenALSample::releaseSource()
{
    if (m_source)
    {
        al_SourceStop(m_source);
        releaseBuffers();
        releaseSourceToPool(m_source);
        m_source = 0;
    }
}

// initial = 2 (finished: never started, nothing queued), playing/paused = 4,
// stopped = 2 only if we stopped it - a source that ran dry on its own still reads as 4 so the
// buffer machinery gets to refill and restart it. Anything else 0.
int OpenALSample::getState()
{
    ALint state = 0;
    al_GetSourcei(m_source, AL_SOURCE_STATE, &state);
    switch (state)
    {
    case AL_INITIAL: return 2;
    case AL_PLAYING:
    case AL_PAUSED:  return 4;
    case AL_STOPPED: return isLoopingDone() ? 2 : 4;   // slot +0x38, not isStopped (+0x20)
    default:         return 0;
    }
}

const Vec3* OpenALSample::getPosition(Vec3* out)
{
    m_sound->computePosition(out, &m_position);
    return out;
}

// `stopType` comes from the template: 0 stop now, 1 finish the loop then play
// the outro, 2 cut to the outro now. `force` overrides it to 0. Answers true when the voice is
// silent on return.
bool OpenALSample::stop(bool force)
{
    if (m_stopped && !force)
        return false;
    SoundFile* file = getSoundFile();
    if (!(file && file->m_loaded && m_source))
        return true;
    bool stoppedNow = false;
    al_GetError();
    int stopType = m_sound->getTemplateImpl()->getStopType();
    if (force)
        stopType = 0;
    if (stopType == 0)
    {
        al_SourceStop(m_source);
        releaseBuffers();
        stoppedNow = true;
    }
    else if (stopType == 1)
    {
        endLoop();
    }
    else if (stopType == 2)
    {
        SoundFile* f = getSoundFile();
        if (f && f->m_loopEnd != f->m_numSamples)
        {
            al_SourceStop(m_source);
            releaseBuffers();
            if (endLoop())
            {
                al_SourcePlay(m_source);
                goto done;
            }
        }
        else
        {
            al_SourceStop(m_source);
            releaseBuffers();
        }
        stoppedNow = true;
    }
done:
    m_stopped = true;
    checkOpenALError("OpenALSample::stop");
#if !defined(_WIN32)
    if (soundTraceWants(file))                                         // [TEMP] BF2_SOUND_TRACE
    {
        ALint state = 0, queued = 0, processed = 0;
        al_GetSourcei(m_source, AL_SOURCE_STATE, &state);
        al_GetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
        al_GetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
        std::fprintf(stderr, "[SND] stop %p src=%u file=%s stopType=%d force=%d now=%d state=0x%x queued=%d processed=%d frame=%d\n",
                     (void*) this, m_source, file->m_name.c_str(), stopType, force ? 1 : 0, stoppedNow ? 1 : 0,
                     state, queued, processed, g_soundEngineInstance->m_frame);
        std::fflush(stderr);
    }
#endif
    return stoppedNow;
}

// EAX obstruction from the evaluator's answer, scaled by how much the listener's
// own 3D obstruction property leaves.
void OpenALSample::setObstruction(float obstruction)
{
    m_obstruction = obstruction;
    if (g_eaxAvailable && is3D())
    {
        const float listenerFactor = 1.0f - clamp01(g_soundProperties->get(SoundProperties::Listener3dObstruction));
        m_eax.lObstruction = linearToMillibel((1.0f - clamp01(obstruction)) * listenerFactor);
    }
}

void OpenALSample::setOcclusion(float occlusion)
{
    if (g_eaxAvailable)
        m_eax.lOcclusion = linearToMillibel(1.0f - clamp01(occlusion));
}

// unqueue every processed buffer; answers how many there were.
int OpenALSample::releaseBuffers()
{
    ALint processed = 0;
    al_GetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
    for (ALint n = processed; n > 0; --n)
    {
        ALuint b;
        al_SourceUnqueueBuffers(m_source, 1, &b);
    }
    return processed;
}

void OpenALSample::applyDistanceAndCone()
{
    SoundTemplate* t = m_sound->getTemplateImpl();
    al_Sourcef(m_source, AL_REFERENCE_DISTANCE, t->getMinDistance());
    al_Sourcef(m_source, AL_ROLLOFF_FACTOR, t->m_rolloff);
    const float reverb = t->getReverbLevel();
    if (g_eaxAvailable)
        m_eax.lRoom = linearToMillibel(reverb);
    if (t->hasCone())
    {
        al_Sourcef(m_source, AL_CONE_INNER_ANGLE, t->getConeInnerAngle());
        al_Sourcef(m_source, AL_CONE_OUTER_ANGLE, t->getConeOuterAngle());
        al_Sourcef(m_source, AL_CONE_OUTER_GAIN,  t->getConeOuterVolume());
    }
    else
    {
        al_Sourcef(m_source, AL_CONE_INNER_ANGLE, 360.0f);
        al_Sourcef(m_source, AL_CONE_OUTER_ANGLE, 360.0f);
        al_Sourcef(m_source, AL_CONE_OUTER_GAIN,  0.0f);
    }
}

// Ask the Sound's obstruction evaluator (if any) at this sample's position,
// slew-limit the answer by 0.1 per update unless the sample is finished, scale by the template's
// global effect level, push it to EAX, and answer the gain multiplier the mixer applies: with EAX
// or for a 2D sound just (1 - obstruction); for a 3D sound on a software path the listener's 3D
// obstruction property and its factor shape it.
float OpenALSample::computeObstructionGain()
{
    float obstruction = 0.0f;
    if (m_sound->getObstructionEvaluator())
    {
        ISoundObstructionEvaluator* evaluator = m_sound->getObstructionEvaluator();
        Vec3 pos;
        getPosition(&pos);
        float raw = evaluator->evaluate(m_sound, &pos);
        if (getState() == 2)
        {
            obstruction = raw;
        }
        else
        {
            obstruction = m_obstruction;
            float delta = raw - obstruction;
            if (delta >= 0.1f) delta = 0.1f;
            if (delta < -0.1f) delta = -0.1f;
            obstruction += delta;
        }
    }
    obstruction *= m_sound->getTemplateImpl()->getGlobalEffectLevel();
    setObstruction(obstruction);
    if (g_eaxAvailable || !is3D())
        return clamp01(1.0f - obstruction);
    const float k = g_soundProperties->get(SoundProperties::Listener3dObstruction);
    const float f = g_soundProperties->get(SoundProperties::Listener3dObstructionFactor);
    return clamp01(1.0f - f * (k + obstruction));
}

// 0 = playing, 1 = no voice free (the scheduler retries), 2 = nothing to queue.
int OpenALSample::start(int startSample, int loops)
{
    al_GetError();
    getSoundFile();
    m_source = acquireSource();
    if (!m_source)
        return 1;
    al_Sourcef(m_source, AL_PITCH, 1.0f);
    al_Sourcef(m_source, AL_GAIN, 0.0f);
    al_Sourcei(m_source, AL_LOOPING, 0);
    if (startBuffers(startSample, loops))
    {
        ALfloat zero[3] = { 0.0f, 0.0f, 0.0f };
        ALfloat dir[3]  = { 0.0f, 0.0f, -1.0f };
        al_Sourcefv(m_source, AL_POSITION, zero);
        al_Sourcefv(m_source, AL_VELOCITY, zero);
        al_Sourcefv(m_source, AL_DIRECTION, dir);
        applyDistanceAndCone();
        update(0);
        al_SourcePlay(m_source);
        checkOpenALError("OpenALSample::start");
#if !defined(_WIN32)
        if (soundTraceWants(getSoundFile()))                           // [TEMP] BF2_SOUND_TRACE
        {
            SoundFile* f = getSoundFile();
            SoundTemplate* t = m_sound->getTemplateImpl();
            ALint looping = 0;
            al_GetSourcei(m_source, AL_LOOPING, &looping);
            std::fprintf(stderr, "[SND] start %p src=%u file=%s ch=%d rate=%d bits=%d samples=%d loop=[%d,%d) "
                         "buffers intro=%u loop=%u outro=%u startSample=%d loops=%d AL_LOOPING=%d 3d=%d "
                         "tmplLoopCount=%d stopType=%d frame=%d\n",
                         (void*) this, m_source, f->m_name.c_str(), f->m_format.channels, f->m_format.samplesPerSecond,
                         f->m_format.bitsPerSample, f->m_numSamples, f->m_loopStart, f->m_loopEnd,
                         f->m_bufferIntro, f->m_bufferLoop, f->m_bufferOutro, startSample, loops, looping,
                         is3D() ? 1 : 0, (int) t->getLoopCount(), t->getStopType(), g_soundEngineInstance->m_frame);
            std::fflush(stderr);
        }
#endif
        return 0;
    }
    releaseSourceToPool(m_source);
    m_source = 0;
    return 2;
}

// The per-frame voice update: buffers, position/velocity/direction in
// listener space (with the DLL's sanity clamps: |pos| >= 20000 or NaN -> (20000,20000,20000),
// |vel| >= 300 or NaN -> 0, |dir| >= 1.1 or NaN -> (1,0,0)), air absorption, tinnitus occlusion,
// obstruction, the template/evaluator volume and pitch, and the EAX block.
void OpenALSample::update(int steps)
{
    (void) steps;
    al_GetError();
    SoundFile* file = getSoundFile();
    if (!file || !file->m_loaded)
        return;
    updateBuffers();
    SoundTemplate* t = m_sound->getTemplateImpl();
    float airFactor = 1.0f;
    if (is3D())
    {
        Vec3 pos;
        getPosition(&pos);
        Vec3 vel = *m_sound->getParentVelocity();
        if (std::fabs(pos.x) >= 20000.0f || std::fabs(pos.y) >= 20000.0f || std::fabs(pos.z) >= 20000.0f || sndmath::hasNaN(pos))
        {
            pos.x = 20000.0f; pos.y = 20000.0f; pos.z = 20000.0f;
        }
        if (std::fabs(vel.x) >= 300.0f || std::fabs(vel.y) >= 300.0f || std::fabs(vel.z) >= 300.0f || sndmath::hasNaN(vel))
        {
            vel.x = 0.0f; vel.y = 0.0f; vel.z = 0.0f;
        }
        g_soundEngineInstance->toListenerSpace(m_sound, &pos, &vel);
        pos.z = -pos.z;
        vel.z = -vel.z;
        al_Sourcefv(m_source, AL_POSITION, &pos.x);
        al_Sourcefv(m_source, AL_VELOCITY, &vel.x);
        if (t->hasCone())
        {
            Vec3 dir;
            dir.x = 0.0f; dir.y = 0.0f; dir.z = 0.0f;
            sndmath::transformVector(*m_sound->getParentMatrix(), t->getDirection(), dir);
            if (std::fabs(dir.x) >= 1.1f || std::fabs(dir.y) >= 1.1f || std::fabs(dir.z) >= 1.1f || sndmath::hasNaN(dir))
            {
                dir.x = 1.0f; dir.y = 0.0f; dir.z = 0.0f;
            }
            sndmath::transformVectorInPlace(g_soundEngineInstance->m_listenerMatrix, dir);
            dir.z = -dir.z;
            al_Sourcefv(m_source, AL_DIRECTION, &dir.x);
        }
        if (file->m_numSamples <= 0)
            return;                     // a loaded 3D file with no samples skips the gain update
        const float distance = std::sqrt(pos.z * pos.z + pos.y * pos.y + pos.x * pos.x);
        const float air = g_soundEngineInstance->airAbsorption(distance);
        if (g_eaxAvailable)
            m_eax.flAirAbsorptionFactor = air * 5.0f;
    }
    float tinnitus = t->getGlobalEffectLevel() * g_soundEngineInstance->m_tinnitusLevel;
    if (g_eaxAvailable && (is3D() || 0.0f != g_soundProperties->get(SoundProperties::Use2dOcclusion)))
        setOcclusion(tinnitus);
    else
        airFactor = 1.0f - tinnitus * 0.9f;
    float gain = computeObstructionGain();
    const int   blockStart = getCurrentBlockStart();
    const float progress   = loopProgress(blockStart, g_soundEngineInstance->m_frame - m_startFrame);
    const float groupGain  = is3D() ? g_soundEngineInstance->m_gain3d : g_soundEngineInstance->m_gain2d;
    const float volume     = m_sound->computeVolume(progress, m_volumeSeed);
    gain = clamp01(volume * groupGain) * gain * airFactor;
    al_Sourcef(m_source, AL_GAIN, gain);
    const float pitch = m_sound->computePitch(progress, m_pitchSeed);
    al_Sourcef(m_source, AL_PITCH, pitch);
#if !defined(_WIN32)
    if (soundTraceWants(file))                                         // [TEMP] BF2_SOUND_TRACE
    {
        const int age = g_soundEngineInstance->m_frame - m_startFrame;
        if (age <= 3 || age % 15 == 0)
        {
            ALint state = 0, queued = 0, processed = 0;
            al_GetSourcei(m_source, AL_SOURCE_STATE, &state);
            al_GetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
            al_GetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
            std::fprintf(stderr, "[SND] update %p src=%u age=%d progress=%.3f volume=%.3f gain=%.3f pitch=%.3f state=0x%x queued=%d processed=%d\n",
                         (void*) this, m_source, age, progress, volume, gain, pitch, state, queued, processed);
            std::fflush(stderr);
        }
    }
#endif
    if (g_soundEngineInstance->getInteractiveMode())
        applyDistanceAndCone();
    if (g_eaxAvailable)
        eax_Set(&g_eaxBufferGuid, EAX_ALLPARAMETERS, m_source, &m_eax, sizeof(EAXBUFFERPROPERTIES));
    checkOpenALError("OpenALSample::update");
}

// StaticSample

StaticSample::StaticSample(const Vec3* position)
    : OpenALSample(position), m_loopPending(false)
{
}

StaticSample::~StaticSample()
{
    if (m_source)
    {
        al_SourceStop(m_source);
        releaseBuffers();
        releaseSourceToPool(m_source);
        m_source = 0;
    }
}

int StaticSample::getCurrentBlockStart()
{
    if (m_loopPending)
        return 0;
    if (isStopped())
        return getSoundFile()->m_loopEnd;
    return getSoundFile()->m_loopStart;
}

// Queue the intro (only from sample 0, only if the Sound's flag 0x200 does not
// suppress it), the loop block `loops` times - or once with AL_LOOPING when loops is 0, deferred
// behind the intro when there is one - and the outro when the start position allows.
bool StaticSample::startBuffers(int startSample, int loops)
{
    m_loopPending = false;
    SoundFile* file = getSoundFile();
    int queued = 0;
    if (!startSample && file->m_bufferIntro && !(m_sound->getSoundFlags() & 0x200))
    {
        al_SourceQueueBuffers(m_source, 1, &file->m_bufferIntro);
        queued = 1;
    }
    if (startSample <= file->m_loopStart)
    {
        if (file->m_bufferLoop)
        {
            if (loops)
            {
                queued += loops;
                for (int n = loops; n; --n)
                    al_SourceQueueBuffers(m_source, 1, &file->m_bufferLoop);
            }
            else
            {
                if (queued)
                    m_loopPending = true;
                else
                    al_Sourcei(m_source, AL_LOOPING, 1);
                al_SourceQueueBuffers(m_source, 1, &file->m_bufferLoop);
                ++queued;
            }
        }
    }
    if (file->m_bufferOutro)
    {
        if (startSample <= file->m_loopEnd && (loops || startSample == file->m_loopEnd))
        {
            al_SourceQueueBuffers(m_source, 1, &file->m_bufferOutro);
            ++queued;
        }
    }
    return queued > 0;
}

// Once the intro has been consumed, drop it and switch the loop block to
// AL_LOOPING - restarting the source, which stopped when its queue ran out.
int StaticSample::updateBuffers()
{
    const bool stopped = isStopped();
    if (!stopped && m_loopPending)
    {
        ALint processed = 0;
        al_GetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
        if (processed > 0)
        {
            ALuint b;
            al_SourceUnqueueBuffers(m_source, 1, &b);
            al_Sourcei(m_source, AL_LOOPING, 1);
            al_SourcePlay(m_source);
            m_loopPending = false;
        }
    }
    return stopped;
}

bool StaticSample::endLoop()
{
    bool queued = false;
    if (getSoundFile()->m_bufferOutro)
    {
        al_SourceQueueBuffers(m_source, 1, &getSoundFile()->m_bufferOutro);
        queued = true;
    }
    al_Sourcei(m_source, AL_LOOPING, 0);
    return queued;
}

// StreamingSample

StreamingSample::StreamingSample(const Vec3* position)
    : OpenALSample(position), m_vorbisFile(0), m_loopsLeft(0), m_bufferIndex(0)
{
    m_buffers[0] = m_buffers[1] = m_buffers[2] = m_buffers[3] = 0;
    m_streaming = true;
}

// Give the voice back, park the four buffers, close the decoder.
StreamingSample::~StreamingSample()
{
    if (m_source)
    {
        al_SourceStop(m_source);
        releaseBuffers();
        releaseSourceToPool(m_source);
        m_source = 0;
    }
    for (int i = 0; i < 4; ++i)
        if (m_buffers[i])
            s_streamingBufferPool.push_front(m_buffers[i]);    // _Insert(begin()) - the front
    if (m_vorbisFile)
    {
        ov_clear(m_vorbisFile);
        SoundFile::destroyMemoryFile(m_vorbisFile);
    }
}

bool StreamingSample::startBuffers(int startSample, int loops)
{
    m_loopsLeft = loops ? loops : INT_MAX;
    for (int i = 0; i < 4; ++i)
        m_buffers[i] = allocateStreamingBuffer();
    m_bufferIndex = 0;
    if (streamStart(&m_vorbisFile, getSoundFile(), s_streamBuffer, &m_loopsLeft, startSample,
                    m_source, m_buffers[m_bufferIndex], true))
        ++m_bufferIndex;
    return true;
}

// The first four updates each queue one more pooled buffer; from then on the
// helper recycles processed ones.
int StreamingSample::updateBuffers()
{
    const ALuint next = m_bufferIndex < 4 ? m_buffers[m_bufferIndex] : 0;
    const bool queuedNew = updateSourceBuffers(m_vorbisFile, getSoundFile(), s_streamBuffer, &m_loopsLeft,
                                               m_source, next, true);
    if (queuedNew)
        ++m_bufferIndex;
    return queuedNew;
}

int StreamingSample::releaseBuffers()
{
    const int processed = OpenALSample::releaseBuffers();
    for (int i = 0; i < 4; ++i)
    {
        if (m_buffers[i])
            s_streamingBufferPool.push_front(m_buffers[i]);   // _Insert(begin()) - the front
        m_buffers[i] = 0;
    }
    return processed;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_BF2AUDIO
