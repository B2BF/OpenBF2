
// BF2Audio.dll only. Without BF2_IMAGE_BF2AUDIO this file compiles to nothing: the recursive
// sweeps that build BF2.exe and bf2_w32ded.exe (CMake's bf2_client_objects glob, build.cmd's
// all-TU census) must not carry the DLL's engine: its DllMain, its two exports and its own null
// Sound/SoundTemplate pair would collide with the host's, which loads this image at run time.
// Same convention as
// Code/BF2/RendDX9/BF2Render.cpp's BF2_IMAGE_RENDDX9 guard. Build with Code\Build\bf2audio.cmd.
#if defined(BF2_IMAGE_BF2AUDIO)
#include "BfSoundEngine.h"
#include "Sound.h"
#include "Sample.h"
#include "SoundFile.h"
#include "SoundEvaluator.h"
#include "SoundDefaults.h"
#include "OpenALProvider.h"
#include "OpenALSample.h"
#include "Reverb.h"
#include "Music.h"
#include "SoundTrace.h"
#include "Win32System.h"
#include "SoundEngineMath.h"
#include "../../Platform/MsvcRand.h"   // [CHOSEN, not recovered] MSVCR71 rand range (RAND_MAX 0x7FFF)

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dice {
namespace hfe {

namespace {
const char* const kFile = "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Sound\\SoundEngine\\BfSoundEngine.cpp";

Vec3 makeVec3(float x, float y, float z)
{
    Vec3 v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

Mat4 makeIdentity()
{
    Mat4 m;
    for (int i = 0; i < 16; ++i)
        m.m[i] = 0.0f;
    m.m[0] = 1.0f; m.m[5] = 1.0f; m.m[10] = 1.0f; m.m[15] = 1.0f;
    return m;
}

// the std::sort comparator loadAllFiles hands the file vector - by name.
bool soundFileNameLess(const SoundFile* a, const SoundFile* b)
{
    return a->m_name < b->m_name;
}

char s_formatBuffer[2048];      // ds:100464B0
} // namespace

// The TU's globals
ISoundTrace*     g_soundTrace          = 0;               // ds:10046454
ISoundFileIO*    g_soundFileIO         = 0;               // ds:10046458
BfSoundEngine*   g_soundEngineInstance = 0;               // ds:1004645C
SoundProperties* g_soundProperties     = 0;               // ds:10046460
ISound*          g_nullSound           = 0;               // ds:getSoundEngine news it
ISoundTemplate*  g_nullSoundTemplate   = 0;               // ds:10046450
const int        g_framesPerSecond     = 30;
const Vec3       g_zeroVector          = makeVec3(0.0f, 0.0f, 0.0f);   // ds:10046464
const Vec3       g_defaultFacing       = makeVec3(0.0f, 0.0f, 1.0f);
const Vec3       g_defaultUp           = makeVec3(0.0f, 1.0f, 0.0f);
Mat4             g_identityMatrix      = makeIdentity();
float            g_maxDistanceCutoff   = 0.02f;
float            g_airMinDistance      = 0.0f;            // ds:10046438
float            g_airMaxDistance      = 0.0f;            // ds:1004643C
float            g_airMax              = 0.0f;            // ds:10046440
float            g_airEaxMinDistance   = 0.0f;            // ds:10046444
float            g_airEaxMaxDistance   = 0.0f;            // ds:10046448

float clamp01(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v <= 1.0f)
        return v;
    return 1.0f;
}

char* sndFormat(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    _vsnprintf(s_formatBuffer, 0x800, fmt, va);
    va_end(va);
    return s_formatBuffer;
}


// SoundProperties


// thirty-three `m_index[name] = i; m_values[i] = default` pairs, in this order.
SoundProperties::SoundProperties()
{
    static const struct { const char* name; float value; } kDefaults[Count] =
    {
        { "ambient-fade-time",               4.0f  },
        { "ambient-forced-fade-time",        1.0f  },
        { "ambient-indoor-obstruction",      0.98f },
        { "ambient-indoor-obstruction-speed", 0.08f },
        { "distance-delay-threshold",        5.0f  },
        { "distance-delay-factor",           1.0f  },
        { "max-distance-cutoff",             0.02f },
        { "prio-k-frontback",                0.0f  },
        { "prio-k-rightleft",                0.0f  },
        { "prio-k-age",                      0.5f  },
        { "prio-age-only-distance",          5.0f  },
        { "prio-k-distance",                 1.0f  },
        { "balance2d3d-default",             0.5f  },
        { "balance2d3d-eax",                 0.5f  },
        { "airabsorption-mindistance",       15.0f },
        { "airabsorption-maxdistance",       30.0f },
        { "airabsorption-max",               0.9f  },
        { "airabsorption-eax-mindistance",   30.0f },
        { "airabsorption-eax-maxdistance",   60.0f },
        { "airabsorption-eax-dbpermeter",    -5.0f },
        { "soundfile-use-usagerefcount",     0.0f  },
        { "dbg0",                            0.0f  },
        { "dbg1",                            0.0f  },
        { "dbg2",                            0.0f  },
        { "dbg3",                            0.0f  },
        { "use-source-pool",                 0.0f  },
        { "required-buffer-size",            31.0f },
        { "use-2d-occlusion",                0.0f  },
        { "listener-3d-obstruction",         0.0f  },
        { "listener-3d-obstruction-factor",  0.85f },
        { "allow-any-system-openal",         0.0f  },
        { "enable-culling",                  1.0f  },
        { "min-free-sources",                4.0f  },
    };
    for (int i = 0; i < Count; ++i)
    {
        m_index[std::string(kDefaults[i].name)] = i;
        m_values[i] = kDefaults[i].value;
    }
}

// index 0 is held to 0..100, 2 and 3 to 0..1, the rest stored raw.
void SoundProperties::set(int index, float value)
{
    if (index == 0)
    {
        float v = value;
        if (v >= 100.0f)
            v = 100.0f;
        if (v < 0.0f)
            v = 0.0f;
        m_values[0] = v;
    }
    else if (index <= 1 || index > 3)
    {
        m_values[index] = value;
    }
    else
    {
        m_values[index] = clamp01(value);
    }
}

void SoundProperties::setByName(const char* name, float value)
{
    std::map<std::string, int>::iterator it = m_index.find(std::string(name));
    if (it == m_index.end())
        g_soundTrace->warning(sndFormat("Cannot set unknown property %s", name), kFile, 434);
    else
        set(it->second, value);
}

float SoundProperties::getByName(const char* name) const
{
    std::map<std::string, int>::const_iterator it = m_index.find(std::string(name));
    if (it != m_index.end())
        return m_values[it->second];
    g_soundTrace->warning(sndFormat("Cannot get unknown property %s", name), kFile, 450);
    return 0.0f;
}


// Min-heap comparator: the earliest ready sample is first.
bool SampleReadyLater::operator()(const Sample* a, const Sample* b) const
{
    return a->m_readyFrame > b->m_readyFrame;
}

// Higher priority first; equal priorities use later start times first.
bool SamplePriorityOrder::operator()(const Sample* a, const Sample* b) const
{
    if (a->m_priority == b->m_priority)
        return a->m_startFrame > b->m_startFrame;
    return a->m_priority > b->m_priority;
}


// BfSoundEngine - construction


BfSoundEngine::BfSoundEngine()
    : m_initialized(false), m_quality(0), m_activeProvider(0), m_provider(0),
      m_sampleRate(44100), m_bits(16), m_masterVolume(0.0f), m_gain2d(1.0f), m_gain3d(1.0f),
      m_listenerPosition(&g_zeroVector), m_listenerFacing(&g_defaultFacing),
      m_listenerUp(&g_defaultUp), m_listenerVelocity(&g_zeroVector),
      m_ownsTrace(false), m_ownsFileIO(false), m_frame(0), m_updateCount(0),
      m_maxVoices(16), m_listenerReverb(0.5f),
      m_tinnitusSound(0), m_tinnitusStart(0), m_tinnitusEnd(-1), m_tinnitusVolume(0.0f),
      m_tinnitusT(0.0f), m_tinnitusLevel(0.0f), m_tinnitusFadeOutAt(0.0f),
      m_interactiveMode(false), m_reverb(0), m_voices(0), m_lastQuality(0),
      m_underwater(false), m_underwaterOcclusion(0.0f)
{
    m_provider = OpenALProvider::create();          // +0x30 before the trace sink exists
    for (int i = 0; i < 8; ++i)
        m_groupVolume[i] = 1.0f;
    m_listenerMatrix = makeIdentity();
    m_listenerVelocityLocal = makeVec3(0.0f, 0.0f, 0.0f);
    std::memset(&m_stats, 0, sizeof(m_stats));

    // The DLL's own defaults, until BfSound::init hands over the client's implementations.
    g_soundFileIO = new DefaultSoundFileIO;
    m_ownsFileIO  = true;
    g_soundTrace  = new DefaultSoundTrace;
    m_ownsTrace   = true;
}

BfSoundEngine::~BfSoundEngine()
{
    if (m_tinnitusSound)
        m_tinnitusSound->release();
    shutdown();
    if (m_ownsFileIO)
    {
        delete static_cast<DefaultSoundFileIO*>(g_soundFileIO);   // raw free: no virtual destructor
        g_soundFileIO = 0;
    }
    if (m_ownsTrace)
    {
        delete static_cast<DefaultSoundTrace*>(g_soundTrace);
        g_soundTrace = 0;
    }
    if (m_reverb)
        delete m_reverb;
    // The containers empty themselves once more (shutdown() already did) and then destruct.
    clearDelayed();
    clearList(m_waiting);
    clearList(m_playing);
    // not deleted, as shipped: m_provider.
}


// ISoundEngine - lifecycle


int BfSoundEngine::initialize(SoundQuality quality, const char* provider, int voices)
{
    if (!win32::hasSoundDevice())
    {
        g_soundTrace->warning(sndFormat("No sound card detected"), kFile, 583);
        return 1;
    }
    std::string name(provider ? provider : "0");
    if (!m_initialized || (int) quality != m_lastQuality || !(provider == m_providerName) || voices != m_voices)
    {
        m_voices       = voices;
        m_providerName = name;
        m_lastQuality  = quality;
        const bool musicWasPlaying = musicStop(0.0f);
        m_initialized = false;
        m_quality     = quality;
        refreshProviders();
        flushPending();
        unloadFiles(true, false);
        const int err = openProvider(name);
        if (err)
            return err;
        updateVoiceLimit(voices);
        setMasterVolume(m_masterVolume);
        m_initialized = true;
        loadAllFiles();
        if (m_reverb)
            m_reverb->markDirty();
        if (musicWasPlaying)
            musicPlay();
    }
    return 0;
}

void BfSoundEngine::shutdown()
{
    musicStop(0.0f);
    setListenerPosition(0);
    setListenerOrientation(0, 0);
    setListenerVelocity(0);
    flushPending();
    unloadFiles(true, true);
    purgeLoopInfoCache();
    for (unsigned int i = 0; i < m_soundFiles.size(); ++i)
    {
        m_soundFiles[i]->m_oggLoopInfo = 0;
        m_soundFiles[i]->release();
    }
    stopNewSamples();
    clearList(m_playing);
    clearList(m_waiting);
    clearDelayed();
    std::vector<SoundFile*>().swap(m_soundFiles);
    if (m_activeProvider)
        m_activeProvider->close();
    m_activeProvider = 0;
    setSoundFileIO(0);
    setSoundTrace(0);
    m_initialized = false;
}

// No null guard on the allocation: the shipped body calls addRef through it.
ISoundTemplate* BfSoundEngine::createTemplate()
{
    SoundTemplate* t = new SoundTemplate;
    t->addRef();
    return t;
}

void BfSoundEngine::loadSoundFiles()
{
    unloadFiles(false, false);
    loadAllFiles();
    m_triggers.sort();
}

void BfSoundEngine::stopAllSamples()
{
    stopNewSamples();
    stopAllInList(m_playing, true);
    stopAllInList(m_waiting, true);
    processStopRequests();
    clearDelayed();
}


// ISoundEngine - volumes and the listener


// The provider receives the unclamped value, and only when the clamped one changed.
void BfSoundEngine::setMasterVolume(float v)
{
    float clamped;
    if (v >= 0.0f)
        clamped = v <= 1.0f ? v : 1.0f;
    else
        clamped = 0.0f;
    if (clamped != m_masterVolume)
    {
        m_masterVolume = clamped;
        if (m_activeProvider)
            m_activeProvider->setMasterVolume(v);
    }
}

// Clamps volume for group indices 0 through 7; rejects negative indices.
void BfSoundEngine::setVolumeForGroup(int group, float v)
{
    if ((unsigned int) group < 8)
        m_groupVolume[group] = clamp01(v);
}

float BfSoundEngine::getVolumeForGroup(int group) const
{
    if ((unsigned int) group >= 8)
        return 1.0f;
    return m_groupVolume[group];
}

// a null pointer selects the built-in default.
void BfSoundEngine::setListenerPosition(const Vec3* p)
{
    m_listenerPosition = p ? p : &g_zeroVector;
}

void BfSoundEngine::setListenerOrientation(const Vec3* facing, const Vec3* up)
{
    m_listenerFacing = facing ? facing : &g_defaultFacing;
    m_listenerUp     = up ? up : &g_defaultUp;
}

void BfSoundEngine::setListenerVelocity(const Vec3* v)
{
    m_listenerVelocity = v ? v : &g_zeroVector;
}

void BfSoundEngine::setListenerReverb(float r)
{
    if (r >= 0.0f)
        m_listenerReverb = r <= 1.0f ? r : 1.0f;
    else
        m_listenerReverb = 0.0f;
}

// Clamps negative Doppler factors to zero and forwards to the active provider.
void BfSoundEngine::setDopplerEffect(float f)
{
    if (f < 0.0f)
        f = 0.0f;
    if (m_activeProvider)
        m_activeProvider->setDopplerFactor(f);
}

float BfSoundEngine::getDopplerEffect() const
{
    if (!m_activeProvider)
        return 0.0f;
    return m_activeProvider->getDopplerFactor();
}


// ISoundEngine - the frame


void BfSoundEngine::update(int steps)
{
    if (!(steps > 0 && isInitialized()))
        return;
    g_maxDistanceCutoff = g_soundProperties->get(SoundProperties::MaxDistanceCutoff);
    g_airMinDistance    = g_soundProperties->get(SoundProperties::AirAbsorptionMinDistance);
    g_airMaxDistance    = g_soundProperties->get(SoundProperties::AirAbsorptionMaxDistance);
    g_airMax            = g_soundProperties->get(SoundProperties::AirAbsorptionMax);
    g_airEaxMinDistance = g_soundProperties->get(SoundProperties::AirAbsorptionEaxMinDistance);
    g_airEaxMaxDistance = g_soundProperties->get(SoundProperties::AirAbsorptionEaxMaxDistance);
    if (m_activeProvider)
        m_activeProvider->setAirAbsorption(g_soundProperties->get(SoundProperties::AirAbsorptionEaxDbPerMeter));
    updateBalance();
    updateTinnitus();
    updateListener();
    m_ambient.update();
    m_triggers.update(m_listenerPosition);
    for (unsigned int i = 0; i < m_newSamples.size(); ++i)
        schedule(m_newSamples[i]);
    std::vector<Sample*>().swap(m_newSamples);      // the storage is released, not just emptied
    updateSamples(steps);
    m_frame += steps;
    ++m_updateCount;
    if (!m_newSamples.empty())
    {
        g_soundTrace->warning(sndFormat("Scheduled sounds list is not empty when it should be. The following samples were found:"),
                              kFile, 1267);
        for (unsigned int i = 0; i < m_newSamples.size(); ++i)
        {
            const char* fileName = "NULL";
            Sample* s = m_newSamples[i];
            if (s)
            {
                if (s->m_sound->getTemplateImpl()->getSampleCount() > 0)
                    fileName = s->m_sound->getTemplateImpl()->getSoundFile(0)->m_name.c_str();
            }
            g_soundTrace->warning(sndFormat("%2d: %s", i, fileName), kFile, 1273);
        }
    }
    processStopRequests();
    musicUpdateVolume();
}


// ISoundEngine - tinnitus


void BfSoundEngine::setTinnitusSound(const char** names, int count)
{
    stopTinnitus(true);
    if (m_tinnitusSound)
    {
        m_tinnitusSound->release();
        m_tinnitusSound = 0;
    }
    ISoundTemplate* t = createTemplate();
    t->setIs3dSound(false);
    t->setFilenames(names, count);
    t->setLoopCount(0);
    t->setReverbLevel(0.0f);
    t->setGlobalEffectLevel(0.0f);
    t->setStopType(0);
    t->setIsUsed();
    m_tinnitusSound = t->createSound();
    t->release();
    if (!m_tinnitusSound)
        g_soundTrace->warning(sndFormat("Failed to create tinnitus sound"), kFile, 914);
}

// A negative hold means "hold until stopTinnitus": the envelope parameter is
// then frozen at the middle of the hold plateau (m_tinnitusFadeOutAt) until the stop releases it.
void BfSoundEngine::startTinnitus(float attack, float hold, float release, float volume, int index)
{
    if (!m_tinnitusSound)
        return;
    if (m_tinnitusSound->isPlaying())
        stopTinnitus(true);
    if (attack < 0.1f)
        attack = 0.1f;
    const bool holdUntilStopped = hold < 0.0f;
    if (hold < 0.1f)
        hold = 0.1f;
    if (release < 0.1f)
        release = 0.1f;
    if (volume >= 0.0f)
    {
        if (volume > 1.0f)
            volume = 1.0f;
    }
    else
    {
        volume = 0.0f;
    }
    const float attackEnd = attack + hold;
    const float total     = release + attackEnd;
    const float inv       = 1.0f / total;
    EnvelopePoint points[4];        // the DLL's spelling of the host's SoundEnvelopePoint (same 12 bytes)
    points[0].x = 0.0f;            points[0].yMin = 0.0f; points[0].yMax = 0.0f;
    points[1].x = inv * attack;    points[1].yMin = 1.0f; points[1].yMax = 0.0f;
    points[2].x = inv * attackEnd; points[2].yMin = 1.0f; points[2].yMax = 0.0f;
    points[3].x = 1.0f;            points[3].yMin = 0.0f; points[3].yMax = 0.0f;
    static_cast<Sound*>(m_tinnitusSound)->getTemplateImpl()->setVolumeEnvelope(
        0.0f, 1.0f, 0.0f, 1.0f, false, 4, reinterpret_cast<const SoundEnvelopePoint*>(points));
    m_tinnitusSound->setVolumeEvaluator(0, &m_tinnitusT);
    int frames = (int) (total * 30.0f);
    if (frames < 20)
        frames = 20;
    if (m_tinnitusSound->playSample(index))
    {
        m_tinnitusStart     = m_frame;
        m_tinnitusEnd       = m_tinnitusStart + frames;
        m_tinnitusVolume    = volume;
        m_tinnitusT         = 0.0f;
        m_tinnitusLevel     = 0.0f;
        m_tinnitusFadeOutAt = holdUntilStopped ? (hold * 0.5f + attack) * inv : -1.0f;
    }
}

void BfSoundEngine::stopTinnitus(bool force)
{
    if (m_tinnitusSound && m_tinnitusEnd >= 0)
    {
        if (force || m_tinnitusFadeOutAt < 0.0f)
        {
            m_tinnitusSound->stop();
            m_tinnitusEnd   = -1;
            m_tinnitusLevel = 0.0f;
        }
        else
        {
            m_tinnitusFadeOutAt = -1.0f;    // release the frozen parameter: the envelope runs out
        }
    }
}

void BfSoundEngine::updateTinnitus()
{
    if (m_tinnitusEnd < 0)
        return;
    const float t = (float) ((double) (m_frame - m_tinnitusStart) / (double) (m_tinnitusEnd - m_tinnitusStart));
    m_tinnitusT = t;
    if (m_tinnitusFadeOutAt >= 0.0f && t > m_tinnitusFadeOutAt)
        m_tinnitusT = m_tinnitusFadeOutAt;
    ISoundEvaluator* evaluator = m_tinnitusSound->getVolumeEvaluator();
    const float v = evaluator->getValue(m_tinnitusT, 0);
    m_tinnitusLevel = v * m_tinnitusVolume;
    if (v <= FLT_EPSILON && m_frame > m_tinnitusStart)
        stopTinnitus(true);
}


// ISoundEngine - triggers, ambience, properties, reverb, sinks


void BfSoundEngine::addTrigger(ISoundTemplate* t)      { m_triggers.addTrigger(t); }
int  BfSoundEngine::numTriggers() const                { return m_triggers.numTriggers(); }
void BfSoundEngine::setTrigger(ISound* s)              { m_triggers.setTrigger(static_cast<Sound*>(s)); }
ISound* BfSoundEngine::getTrigger() const              { return m_triggers.getTrigger(); }
void BfSoundEngine::removeTrigger(ISoundTemplate* t)   { m_triggers.removeTrigger(t); }

void BfSoundEngine::removeAllTriggers()
{
    m_ambient.stopAll(0.0f);
    m_triggers.removeAll();
}

void BfSoundEngine::setProperty(const char* name, float v)      { g_soundProperties->setByName(name, v); }
float BfSoundEngine::getProperty(const char* name) const        { return g_soundProperties->getByName(name); }

// A preset that fails to load takes the Reverb object with it.
void BfSoundEngine::setReverb(const char* name)
{
    if (!m_reverb)
        m_reverb = new Reverb;
    if (!m_reverb->load(name))
    {
        if (m_reverb)
            delete m_reverb;
        m_reverb = 0;
    }
}

// Replaces file I/O; null restores the DLL's default implementation.
void BfSoundEngine::setSoundFileIO(ISoundFileIO* io)
{
    if (g_soundFileIO && m_ownsFileIO)
        delete static_cast<DefaultSoundFileIO*>(g_soundFileIO);
    if (io)
    {
        g_soundFileIO = io;
        m_ownsFileIO  = false;
    }
    else
    {
        g_soundFileIO = new DefaultSoundFileIO;
        m_ownsFileIO  = true;
    }
}

void BfSoundEngine::setSoundTrace(ISoundTrace* trace)
{
    if (g_soundTrace && m_ownsTrace)
        delete static_cast<DefaultSoundTrace*>(g_soundTrace);
    if (trace)
    {
        g_soundTrace = trace;
        m_ownsTrace  = false;
    }
    else
    {
        g_soundTrace = new DefaultSoundTrace;
        m_ownsTrace  = true;
    }
}

// counts the files this quality wants, that are used, and are not loaded yet;
// answers nothing else (the pointer is always null).
void* BfSoundEngine::getSoundFiles(int* count)
{
    if (count)
    {
        *count = 0;
        for (unsigned int i = 0; i < m_soundFiles.size(); ++i)
        {
            SoundFile* f = m_soundFiles[i];
            if (f && f->isWantedAtQuality() && f->isUsed() && !f->m_loaded)
                ++*count;
        }
    }
    return 0;
}

void* BfSoundEngine::get3dProviders(int* count)
{
    refreshProviders();
    if (count)
        *count = m_providerNames.empty() ? 0 : (int) m_providerNames.size();
    return m_providerNames.empty() ? 0 : (void*) &m_providerNames[0];
}

void* BfSoundEngine::getStats()
{
    updateStats();
    return &m_stats;
}

void BfSoundEngine::playAmbient(ISound* s, float fadeSeconds)   { m_ambient.play(s, fadeSeconds); }
void BfSoundEngine::stopAmbient(float fadeSeconds)              { m_ambient.stopAll(fadeSeconds); }

int BfSoundEngine::getEaxVersion() const
{
    if (m_activeProvider)
        return m_activeProvider->getEaxVersion();
    return 0;
}

// BF2Audio.dll 0x10004B80: no records - the count is zeroed and the list is null.
const PlayingSampleInfo* BfSoundEngine::queryPlayingSamples(int* count)
{
    if (count)
        *count = 0;
    return 0;
}

void BfSoundEngine::setProfilerClient(IProfilerClient* p)
{
    g_profilerClient = (IProfilerClient*) &g_nullProfilerClient;
    if (p)
        g_profilerClient = p;
}

// the music calls answer false while the engine is down.
bool BfSoundEngine::loadMusic(const char* name)
{
    if (isInitialized())
        return musicLoad(name);
    return false;
}

bool BfSoundEngine::playMusic()
{
    if (isInitialized())
        return musicPlay();
    return false;
}

bool BfSoundEngine::stopMusic(float fadeSeconds)
{
    if (isInitialized())
        return musicStop(fadeSeconds);
    return false;
}


// What the Sounds and Samples call


// Transfers the provider-created sample to its Sound and queues it for the next update.
Sample* BfSoundEngine::playSample(Sound* sound, int index, const Vec3* position)
{
    if (!sound)
        return 0;
    if (!m_activeProvider)
        return 0;
    ISoundTemplate* t = sound->getTemplate();
    if (!t->checkSampleAvailable(index))
        return 0;
    if (shouldCull(sound, position))
        return 0;
    Sample*  holder = 0;
    Sample** made   = m_activeProvider->createSample(&holder, sound->getTemplateImpl(), index, position);
    Sample*  sample = *made;
    *made = 0;
    if (holder)
        delete holder;
    if (!sample)
        return 0;
    sample->m_sound = sound;
    sound->addPlaying();
    sample->m_index = (short) index;
    if (!sample->getSoundFile()->m_numSamples)
    {
        delete sample;
        return 0;
    }
    m_newSamples.push_back(sample);
    sample->m_pending = false;
    return m_newSamples.back();
}

// A sample still queued for scheduling is stopped later (a non-forced stop goes
// to m_stopRequests, processed after this frame's scheduling) or deleted now (forced); otherwise the
// playing list first, then the waiting list (always forced there), then the delay heap, where the
// sample is only detached from its Sound - startDelayed deletes it when its turn comes.
bool BfSoundEngine::stopSample(Sample* s, bool force)
{
    for (unsigned int i = 0; i < m_newSamples.size(); ++i)
    {
        if (m_newSamples[i] && m_newSamples[i] == s)
        {
            if (!force)
            {
                m_stopRequests.push_back(m_newSamples[i]);
                return true;
            }
            if (m_newSamples[i])
                delete m_newSamples[i];
            m_newSamples[i] = 0;
            return true;
        }
    }
    if (stopInList(m_playing, s, force))
        return true;
    if (stopInList(m_waiting, s, true))
        return true;
    for (std::vector<Sample*>::iterator it = m_delayed.begin(); it != m_delayed.end(); ++it)
    {
        if (*it == s)
        {
            (*it)->detachFromSound();
            return true;
        }
    }
    return false;
}

// The same three stages for every sample of one Sound.
void BfSoundEngine::stopSound(Sound* sound, bool force)
{
    for (unsigned int i = 0; i < m_newSamples.size(); ++i)
    {
        Sample* s = m_newSamples[i];
        if (s && s->m_sound == sound)
        {
            if (!force)
            {
                m_stopRequests.push_back(m_newSamples[i]);
            }
            else
            {
                if (s)
                    delete s;
                m_newSamples[i] = 0;
            }
        }
    }
    stopSoundInList(m_playing, sound, force);
    stopSoundInList(m_waiting, sound, true);
    for (std::vector<Sample*>::iterator it = m_delayed.begin(); it != m_delayed.end(); ++it)
    {
        if ((*it)->m_sound == sound)
            (*it)->detachFromSound();
    }
}

// Names are matched lower-cased; a new file is referenced twice - once for the
// engine's own list, once for the caller.
SoundFile* BfSoundEngine::getOrCreateSoundFile(std::string name)
{
    for (unsigned int i = 0; i < name.size(); ++i)
        name[i] = (char) std::tolower((unsigned char) name[i]);
    for (unsigned int i = 0; i < m_soundFiles.size(); ++i)
    {
        if (m_soundFiles[i]->m_name == name)
        {
            m_soundFiles[i]->addRef();
            return m_soundFiles[i];
        }
    }
    SoundFile* file = new SoundFile(name);
    if (file)
    {
        file->addRef();
        m_soundFiles.push_back(file);
        file->addRef();
    }
    return file;
}

// a world position and velocity into the listener frame; a first-person sound
// sits on the listener and does not move.
void BfSoundEngine::toListenerSpace(Sound* sound, Vec3* position, Vec3* velocity)
{
    if (sound->getIs1PSound())
    {
        position->x = 0.0f; position->y = 0.0f; position->z = 0.0f;
        velocity->x = 0.0f; velocity->y = 0.0f; velocity->z = 0.0f;
        return;
    }
    Vec3 local = makeVec3(0.0f, 0.0f, 0.0f);
    sndmath::transformPoint(m_listenerMatrix, *position, local);
    *position = local;
    sndmath::transformVectorInPlace(m_listenerMatrix, *velocity);
}

// 0 up to the minimum distance, the maximum at and beyond the maximum distance,
// linear between; EAX 2+ uses its own distance pair and a maximum of 1.
float BfSoundEngine::airAbsorption(float distance)
{
    float maximum, minDistance, maxDistance;
    if (getEaxVersion() < 2)
    {
        maximum     = g_airMax;
        minDistance = g_airMinDistance;
        maxDistance = g_airMaxDistance;
    }
    else
    {
        maximum     = 1.0f;
        minDistance = g_airEaxMinDistance;
        maxDistance = g_airEaxMaxDistance;
    }
    if (distance <= minDistance)
        return 0.0f;
    if (distance < maxDistance)
        return maximum * (distance - minDistance) / (maxDistance - minDistance);
    return maximum;
}


// Private helpers - providers, files, voices


// Asks m_provider (the object, whether or not a device is open).
bool BfSoundEngine::refreshProviders()
{
    m_providerNames.clear();
    m_provider->getProviderNames(&m_providerNames);
    if (!m_providerNames.empty())
        return true;
    g_soundTrace->warning(sndFormat("No 3D sound providers are available."), kFile, 1590);
    return false;
}

// A one-character name '0'..'9' selects a row of the provider list by index.
int BfSoundEngine::openProvider(const std::string& requested)
{
    std::string name(requested);
    if (name.size() == 1)
    {
        const int code = name[0] - '0';
        if (code >= 0 && code < (int) m_providerNames.size())
            name = m_providerNames[code];
    }
    const int err = m_provider->open(name);
    if (err)
    {
        g_soundTrace->warning(sndFormat("Failed to open 3d provider \"%s\"", name.c_str()), kFile, 1616);
        m_activeProvider = 0;
    }
    else
    {
        m_activeProvider = m_provider;
    }
    return err;
}

// the requested voice count (default 16, floor 2) plus "min-free-sources",
// capped one below what the provider counted.
void BfSoundEngine::updateVoiceLimit(int voices)
{
    int wanted = voices;
    if (voices)
    {
        if (voices < 2)
            wanted = 2;
    }
    else
    {
        wanted = 16;
    }
    int limit = (int) g_soundProperties->get(SoundProperties::MinFreeSources) + wanted;
    if (m_activeProvider)
    {
        const int available = m_activeProvider->getMaxVoices() - 1;
        if (available < limit)
            limit = available;
    }
    m_maxVoices  = limit;
    g_maxSources = limit;
}

// Interactive mode loads nothing here - files load on use.
void BfSoundEngine::loadAllFiles()
{
    if (getInteractiveMode())
        return;
    std::sort(m_soundFiles.begin(), m_soundFiles.end(), soundFileNameLess);
    for (unsigned int i = 0; i < m_soundFiles.size(); ++i)
        m_soundFiles[i]->load(m_sampleRate, m_bits);
}

void BfSoundEngine::unloadFiles(bool unloadStatic, bool unloadStreams)
{
    for (unsigned int i = 0; i < m_soundFiles.size(); ++i)
        m_soundFiles[i]->unloadIfUnused(unloadStatic, unloadStreams);
}


// Private helpers - the scheduler's containers


void BfSoundEngine::pushDelayed(Sample* s)
{
    m_delayed.push_back(s);
    std::push_heap(m_delayed.begin(), m_delayed.end(), m_delayedCompare);
}

// every playing sample gives its voice back and goes to the delay heap for this
// frame; every waiting one goes there for the next.
void BfSoundEngine::flushPending()
{
    stopNewSamples();
    while (!m_playing.empty())
    {
        Sample* s = m_playing.front();
        s->releaseSource();
        s->m_readyFrame = m_frame;
        m_playing.pop_front();
        pushDelayed(s);
    }
    while (!m_waiting.empty())
    {
        Sample* s = m_waiting.front();
        s->m_readyFrame = m_frame + 1;
        m_waiting.pop_front();
        pushDelayed(s);
    }
}

void BfSoundEngine::stopNewSamples()
{
    for (unsigned int i = 0; i < m_newSamples.size(); ++i)
    {
        if (m_newSamples[i])
            delete m_newSamples[i];
    }
    std::vector<Sample*>().swap(m_newSamples);
}

void BfSoundEngine::clearList(std::list<Sample*>& l)
{
    for (std::list<Sample*>::iterator it = l.begin(); it != l.end(); ++it)
    {
        if (*it)
            delete *it;
    }
    l.clear();
}

void BfSoundEngine::clearDelayed()
{
    for (std::vector<Sample*>::iterator it = m_delayed.begin(); it != m_delayed.end(); ++it)
    {
        if (*it)
            delete *it;
    }
    std::vector<Sample*>().swap(m_delayed);
}

// the sample, if it is in this list, is asked to stop; true means "it was here".
bool BfSoundEngine::stopInList(std::list<Sample*>& l, Sample* s, bool force)
{
    std::list<Sample*>::iterator it = l.begin();
    if (it == l.end())
        return false;
    while (*it != s)
    {
        ++it;
        if (it == l.end())
            return false;
    }
    if ((*it)->stop(force))
    {
        if (*it)
            delete *it;
        l.erase(it);
    }
    return true;
}

void BfSoundEngine::stopSoundInList(std::list<Sample*>& l, Sound* sound, bool force)
{
    for (std::list<Sample*>::iterator it = l.begin(); it != l.end(); )
    {
        Sample* s = *it;
        if (s->m_sound == sound && s->stop(force))
        {
            if (s)
                delete s;
            it = l.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void BfSoundEngine::stopAllInList(std::list<Sample*>& l, bool force)
{
    for (std::list<Sample*>::iterator it = l.begin(); it != l.end(); )
    {
        if ((*it)->stop(force))
        {
            if (*it)
                delete *it;
            it = l.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// the deferred non-forced stops, then the list's storage goes.
void BfSoundEngine::processStopRequests()
{
    for (unsigned int i = 0; i < m_stopRequests.size(); ++i)
        stopSample(m_stopRequests[i], false);
    std::vector<Sample*>().swap(m_stopRequests);
}

// with "enable-culling" on, a looping 3D sound at least 5 m away and beyond its
// minimum distance is dropped when it is past its maximum distance or would play below 0.3 %.
bool BfSoundEngine::shouldCull(Sound* sound, const Vec3* position)
{
    if (g_soundProperties->get(SoundProperties::EnableCulling) < 0.5f)
        return false;
    if (!sound->getTemplateImpl()->getIs3dSound())
        return false;
    if (!sound->getTemplateImpl()->getLoopCount())
        return false;
    Vec3 where;
    const Vec3* p = sound->computePosition(&where, position);
    const Vec3* listener = m_listenerPosition;
    const float dy = listener->y - p->y;
    const float dz = listener->z - p->z;
    const float dx = listener->x - p->x;
    const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
    if (distance < 5.0f)
        return false;
    const float minDistance = sound->getTemplateImpl()->getMinDistance();
    if (distance <= minDistance)
        return false;
    if (sound->getTemplateImpl()->m_maxDistance <= distance)
        return true;
    const float rolloff = sound->getTemplateImpl()->m_rolloff;
    return sound->computeVolume(0.0f, 0) / ((distance - minDistance) * rolloff / minDistance + 1.0f) < 0.003f;
}

// a looping 3D sample beyond "distance-delay-threshold" metres starts late, by
// (distance - threshold) * "distance-delay-factor" / 343 m/s, in frames. The two envelope-row seeds
// are drawn here.
void BfSoundEngine::schedule(Sample* s)
{
    if (!s)
        return;
    int delay = 0;
    if (s->is3D())
    {
        if (s->m_sound->getLoopOverride() > 0)
        {
            const Vec3* listener = getListenerPosition();
            Vec3 where;
            const Vec3* p = s->getPosition(&where);
            const float dx = p->x - listener->x;
            const float dy = p->y - listener->y;
            const float dz = p->z - listener->z;
            const float distance  = std::sqrt(dz * dz + dy * dy + dx * dx);
            const float threshold = g_soundProperties->get(SoundProperties::DistanceDelayThreshold);
            if (distance > threshold)
                delay = (int) std::floor((distance - threshold) * g_soundProperties->get(SoundProperties::DistanceDelayFactor)
                                         * 0.002915452 * 30.0);
        }
    }
    s->m_startFrame = m_frame + delay;
    s->m_readyFrame = m_frame + delay;
    const int r = bf2crt::rand();
    s->m_pitchSeed  = (unsigned char) r;
    s->m_volumeSeed = (unsigned char) (r >> 8);
    pushDelayed(s);
}

// the 2D/3D balance property (the EAX one when EAX is up) into the two gains;
// the 3D gain is further scaled by the master volume under EAX.
void BfSoundEngine::updateBalance()
{
    const float balance = getEaxVersion() <= 0
                        ? g_soundProperties->get(SoundProperties::Balance2d3dDefault)
                        : g_soundProperties->get(SoundProperties::Balance2d3dEax);
    float upper = balance;
    if (upper >= 1.0f)
        upper = 1.0f;
    if (upper < 0.5f)
        upper = 0.5f;
    m_gain2d = 1.0f - upper + 1.0f - upper;
    float lower = balance;
    if (lower >= 0.5f)
        lower = 0.5f;
    if (lower < 0.0f)
        lower = 0.0f;
    m_gain3d = lower + lower;
    if (getEaxVersion() > 0)
        m_gain3d = m_masterVolume * m_gain3d;
}

// the listener frame (side = up x facing) as a rigid matrix, inverted in place
// into the world-to-listener transform every sample uses; the listener's velocity goes through it
// and on to the provider, and the reverb block is refreshed.
void BfSoundEngine::updateListener()
{
    const Vec3& facing = *m_listenerFacing;
    const Vec3& up     = *m_listenerUp;
    Vec3 side;
    side.x = facing.z * up.y - facing.y * up.z;
    side.y = up.z * facing.x - facing.z * up.x;
    side.z = facing.y * up.x - up.y * facing.x;
    sndmath::setRows(m_listenerMatrix, side, up, facing, *m_listenerPosition);
    Mat4 world = m_listenerMatrix;
    sndmath::setInverseRigid(m_listenerMatrix, world);
    sndmath::transformVector(m_listenerMatrix, *m_listenerVelocity, m_listenerVelocityLocal);
    if (m_activeProvider)
    {
        m_activeProvider->setListenerVelocity(&m_listenerVelocityLocal);
        if (m_reverb)
        {
            m_reverb->setRoomLevel(m_listenerReverb);
            m_reverb->apply(m_activeProvider);
        }
    }
}

void BfSoundEngine::updateStats()
{
    m_stats.providerInfo         = m_activeProvider ? m_activeProvider->getInfo() : "";
    m_stats.playing              = (int) m_playing.size();
    m_stats.waiting              = (int) m_waiting.size();
    m_stats.delayed              = (int) m_delayed.size();
    m_stats.loadedSampleBytes    = g_loadedSampleBytes;
    m_stats.streamedBytes        = g_streamedBytes;
    m_stats.streamingBufferBytes = g_streamingBufferBytes;
    m_stats.alBuffers            = g_alBufferCount;
    m_stats.streamingBuffers     = g_streamingBufferCount;
    m_stats.loadedFiles          = g_loadedFileCount;
    m_stats.sounds               = g_soundCount;
    m_stats.samples              = g_sampleCount;
    m_stats.sources              = g_sourceCount;
    m_stats.maxVoices            = m_maxVoices;
}

// finished samples leave; ready delayed samples enter the waiting list; the
// waiting list is aged, prioritised and sorted, the playing list re-prioritised and sorted, and
// waiting samples take voices - stealing from the lowest-priority playing sample while it is beaten
// (a tie goes to the later start); then every playing sample advances.
void BfSoundEngine::updateSamples(int steps)
{
    for (std::list<Sample*>::iterator it = m_playing.begin(); it != m_playing.end(); )
    {
        if ((*it)->getState() == 2)
        {
            if (*it)
                delete *it;
            it = m_playing.erase(it);
        }
        else
        {
            ++it;
        }
    }
    while (!m_delayed.empty() && !(m_delayed.front()->m_readyFrame > m_frame))
        startDelayed();
    if (!m_waiting.empty())
    {
        dropStale(m_waiting);
        computePriorities(m_waiting);
        m_waiting.sort(SamplePriorityOrder());
        computePriorities(m_playing);
        m_playing.sort(SamplePriorityOrder());
        while (!m_waiting.empty())
        {
            if (startWaiting())
                continue;
            if (!m_waiting.empty())
            {
                while (!m_playing.empty())
                {
                    Sample* want = m_waiting.front();
                    Sample* have = m_playing.back();
                    if (want->m_priority == have->m_priority)
                    {
                        if (want->m_startFrame <= have->m_startFrame)
                            goto advance;
                    }
                    else if (want->m_priority <= have->m_priority)
                    {
                        goto advance;
                    }
                    stealVoice();
                    startWaiting();
                    if (m_waiting.empty())
                        goto advance;
                }
            }
            break;
        }
    }
advance:
    for (std::list<Sample*>::iterator it = m_playing.begin(); it != m_playing.end(); ++it)
        (*it)->update(steps);
}

// the earliest delayed sample leaves the heap - deleted if its Sound let go, its
// file index went out of range or its time already ran out; otherwise it takes (or already holds)
// one of its Sound's two retrigger slots and joins the waiting list.
void BfSoundEngine::startDelayed()
{
    Sample* s = m_delayed.front();
    std::pop_heap(m_delayed.begin(), m_delayed.end(), m_delayedCompare);
    m_delayed.pop_back();
    if (s->hasNoSound())
    {
        if (s)
            delete s;
        return;
    }
    if (!(s->m_index < s->m_sound->getTemplateImpl()->getSampleCount()))
    {
        delete s;
        return;
    }
    if (m_frame - s->m_startFrame < s->getLengthFrames())
    {
        if (s->m_pending)
        {
            m_waiting.push_front(s);
            return;
        }
        if (s->m_sound->attachSample(s))
        {
            s->m_pending = true;
            m_waiting.push_front(s);
            return;
        }
    }
    delete s;
}

// the head of the waiting list tries to start. Where in its file it starts
// depends on how late it is: fresh (up to 10 frames) from the top; later, from the loop block
// (once, or with one loop left when several were asked); stopped, or past a one-loop play with no
// outro buffer, from the loop end. Past its whole length it is dropped. `false` only when the
// provider had no voice (the sample returns to the back of the list).
bool BfSoundEngine::startWaiting()
{
    Sample* s = m_waiting.front();
    m_waiting.pop_front();
    SoundFile* file = s->getSoundFile();
    unsigned int loops = (unsigned int) s->m_sound->getLoopOverride();
    if (!s->m_started)
    {
        if (m_frame - s->m_startFrame > 60 && loops)
        {
            delete s;
            return true;
        }
        s->m_startFrame = m_frame;
    }
    const unsigned int elapsed = (unsigned int) (m_frame - s->m_startFrame);
    unsigned int length = (unsigned int) s->getLengthFrames();
    if (s->isStopped() && !loops)
    {
        const double remaining = (double) s->m_sound->computePitch(0.0f, 0)
                               * (double) (file->m_numSamples - file->m_loopStart);
        length = (unsigned int) (int) (remaining * (30.0 / (double) (unsigned int) file->m_format.samplesPerSecond));
    }
    int start;
    if (elapsed < length)
    {
        if (!s->isStopped() && elapsed <= 10)
        {
            start = 0;
        }
        else if (!s->isStopped() && !loops)
        {
            start = file->m_loopStart;
        }
        else if (!s->isStopped() && loops > 1)
        {
            start = file->m_loopStart;
            loops = 1;
        }
        else
        {
            start = file->m_loopEnd;
        }
    }
    else
    {
        start = file->m_numSamples;
    }
    if (start == file->m_loopEnd && loops == 1 && !file->m_bufferOutro)
        start = file->m_loopStart;
    if (start == file->m_numSamples)
    {
        delete s;
        return true;
    }
    const int result = s->start(start, (int) loops);
    if (result == 0)
    {
        m_playing.push_front(s);
        s->m_started = true;
        return true;
    }
    if (result == 1)
    {
        m_waiting.push_front(s);
        return false;
    }
    delete s;
    return true;
}

// the lowest-priority playing sample gives its voice up; with 15 frames or more
// left it is re-queued five frames out, otherwise it is dropped.
void BfSoundEngine::stealVoice()
{
    Sample* victim = m_playing.back();
    m_playing.pop_back();
    victim->releaseSource();
    const int age = m_frame - victim->m_startFrame;
    if (age + 15 >= victim->getLengthFrames())
    {
        delete victim;
        return;
    }
    victim->m_readyFrame = m_frame + 5;
    pushDelayed(victim);
}

// samples whose whole length has already passed are dropped from a list.
void BfSoundEngine::dropStale(std::list<Sample*>& l)
{
    for (std::list<Sample*>::iterator it = l.begin(); it != l.end(); )
    {
        Sample* s = *it;
        if (m_frame - s->m_startFrame < s->getLengthFrames())
        {
            ++it;
        }
        else
        {
            delete s;
            it = l.erase(it);
        }
    }
}

// a 3D sample's priority is its volume attenuated by distance (minus one past
// the maximum distance); a 2D sample's is 2.
void BfSoundEngine::computePriorities(std::list<Sample*>& l)
{
    for (std::list<Sample*>::iterator it = l.begin(); it != l.end(); ++it)
    {
        Sample* s = *it;
        float priority = 2.0f;
        if (s->is3D())
        {
            Vec3 where;
            const Vec3* p = s->getPosition(&where);
            const Vec3* listener = m_listenerPosition;
            const float dx = listener->x - p->x;
            const float dy = listener->y - p->y;
            const float dz = listener->z - p->z;
            const float distance    = std::sqrt(dz * dz + dy * dy + dx * dx);
            const float minDistance = s->m_sound->getTemplateImpl()->getMinDistance();
            const float maxDistance = s->m_sound->getTemplateImpl()->m_maxDistance;
            const float rolloff     = s->m_sound->getTemplateImpl()->m_rolloff;
            priority = s->m_sound->computeVolume(0.0f, 0);
            if (distance > minDistance)
            {
                priority = priority / ((distance - minDistance) * rolloff / minDistance + 1.0f);
                if (distance >= maxDistance)
                    priority = priority - 1.0f;
            }
        }
        s->m_priority = priority;
    }
}


// The exports


// The four singletons are made on the first call, in this order.
extern "C" ISoundEngine* getSoundEngine()
{
    if (!g_soundEngineInstance)
    {
        g_nullSound          = new NullSound;
        g_nullSoundTemplate  = new NullSoundTemplate;
        g_soundProperties    = new SoundProperties;
        g_soundEngineInstance = new BfSoundEngine;
    }
    return g_soundEngineInstance;
}

// The engine goes through its virtual destructor (slot 54); the null pair is
// freed without one (they have none) and left dangling, as shipped.
extern "C" void shutdownSoundEngine()
{
    if (g_soundEngineInstance)
    {
        delete g_soundEngineInstance;
        if (g_soundProperties)
            delete g_soundProperties;
        delete static_cast<NullSound*>(g_nullSound);
        delete static_cast<NullSoundTemplate*>(g_nullSoundTemplate);
    }
    g_soundEngineInstance = 0;
    g_soundProperties     = 0;
}

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_BF2AUDIO
