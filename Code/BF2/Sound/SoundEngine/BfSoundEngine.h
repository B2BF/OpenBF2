
// Audio DLL sound-engine implementation. Preserve the host interface layout and callback ownership.


#ifndef SOUND_SOUNDENGINE_BFSOUNDENGINE_H
#define SOUND_SOUNDENGINE_BFSOUNDENGINE_H

#include "../NullSound.h"
#include "Ambient.h"
#include "SoundTrigger.h"
#include "../../Math/Mat4.h"

#include <list>
#include <map>
#include <string>
#include <vector>

namespace dice {
namespace hfe {

class Sample;
class Sound;
class SoundFile;
class SoundTemplate;
class Reverb;
class ISoundProvider;
class OpenALProvider;

// the DLL's clamp to 0..1 (five callers).
float clamp01(float v);

// Named sound-property table with 33 floats. Preserve index order across provider and client access.
class SoundProperties
{
public:
    enum Index
    {
        AmbientFadeTime = 0,            // 4.0    ambient-fade-time            (set: clamped 0..100)
        AmbientForcedFadeTime,          // 1.0    ambient-forced-fade-time
        AmbientIndoorObstruction,       // 0.98   ambient-indoor-obstruction   (set: clamped 0..1)
        AmbientIndoorObstructionSpeed,  // 0.08   ambient-indoor-obstruction-speed (set: clamped 0..1)
        DistanceDelayThreshold,         // 5.0    distance-delay-threshold
        DistanceDelayFactor,            // 1.0    distance-delay-factor
        MaxDistanceCutoff,              // 0.02   max-distance-cutoff
        PrioKFrontBack,                 // 0      prio-k-frontback
        PrioKRightLeft,                 // 0      prio-k-rightleft
        PrioKAge,                       // 0.5    prio-k-age
        PrioAgeOnlyDistance,            // 5.0    prio-age-only-distance
        PrioKDistance,                  // 1.0    prio-k-distance
        Balance2d3dDefault,             // 0.5    balance2d3d-default
        Balance2d3dEax,                 // 0.5    balance2d3d-eax
        AirAbsorptionMinDistance,       // 15.0   airabsorption-mindistance
        AirAbsorptionMaxDistance,       // 30.0   airabsorption-maxdistance
        AirAbsorptionMax,               // 0.9    airabsorption-max
        AirAbsorptionEaxMinDistance,    // 30.0   airabsorption-eax-mindistance
        AirAbsorptionEaxMaxDistance,    // 60.0   airabsorption-eax-maxdistance
        AirAbsorptionEaxDbPerMeter,     // -5.0   airabsorption-eax-dbpermeter
        SoundfileUseUsageRefcount,      // 0      soundfile-use-usagerefcount
        Dbg0,                           // 0      dbg0
        Dbg1,                           // 0      dbg1
        Dbg2,                           // 0      dbg2
        Dbg3,                           // 0      dbg3
        UseSourcePool,                  // 0      use-source-pool
        RequiredBufferSize,             // 31.0   required-buffer-size
        Use2dOcclusion,                 // 0      use-2d-occlusion
        Listener3dObstruction,          // 0      listener-3d-obstruction
        Listener3dObstructionFactor,    // 0.85   listener-3d-obstruction-factor
        AllowAnySystemOpenAL,           // 0      allow-any-system-openal
        EnableCulling,                  // 1.0    enable-culling
        MinFreeSources,                 // 4.0    min-free-sources
        Count
    };

    SoundProperties();
    void  set(int index, float value);                  // the three clamps above
    float get(int index) const { return m_values[index]; }
    void  setByName(const char* name, float value);     // "Cannot set unknown property"
    float getByName(const char* name) const;            // "Cannot get unknown property", 0

    std::map<std::string, int> m_index;                 // +0x00
    float                      m_values[Count];         // +0x0C
};

// The block ISoundEngine::getStats hands the client - 0x38 bytes at engine +0x11C, refreshed on
// every call. Field order is the ABI (BF2.exe reads it directly); names are [INFERRED].
struct SoundStats
{
    const char* providerInfo;       // +0x00
    int         playing;            // +0x04
    int         waiting;            // +0x08
    int         delayed;            // +0x0C
    int         loadedSampleBytes;  // +0x10
    int         streamedBytes;      // +0x14
    int         streamingBufferBytes; // +0x18
    int         alBuffers;          // +0x1C
    int         streamingBuffers;   // +0x20
    int         loadedFiles;        // +0x24
    int         sounds;             // +0x28
    int         samples;            // +0x2C
    int         sources;            // +0x30
    int         maxVoices;          // +0x34
};

// The two orderings the scheduler keeps (bodies in the .cpp, read out of the instantiations)
// The delay heap's comparator is a member (+0x6C, the byte the ctor zeroes and every push/pop
// reads): the sample that becomes ready first sits at the front.
struct SampleReadyLater
{
    bool operator()(const Sample* a, const Sample* b) const;
};
// The waiting and playing lists' order: higher priority first, later start among equals.
struct SamplePriorityOrder
{
    bool operator()(const Sample* a, const Sample* b) const;
};

// The engine
class BfSoundEngine : public ISoundEngine
{
public:
    BfSoundEngine();
    virtual ~BfSoundEngine();

    virtual int   initialize(SoundQuality quality, const char* provider, int voices);
    virtual bool  isInitialized() const { return m_initialized; }
    virtual void  shutdown();
    virtual int   getQualityLevel() const { return m_quality; }
    virtual ISoundTemplate* createTemplate();
    virtual void  loadSoundFiles();
    virtual void  stopAllSamples();
    virtual void  setMasterVolume(float v);
    virtual float getMasterVolume() const { return m_masterVolume; }
    virtual void  setVolumeForGroup(int group, float v);
    virtual float getVolumeForGroup(int group) const;
    virtual void  setListenerPosition(const Vec3* p);
    virtual const Vec3* getListenerPosition() const { return m_listenerPosition; }
    virtual void  setListenerOrientation(const Vec3* facing, const Vec3* up);
    virtual const Vec3* getListenerOrientationFacing() const { return m_listenerFacing; }
    virtual const Vec3* getListenerOrientationUp() const { return m_listenerUp; }
    virtual void  setListenerVelocity(const Vec3* v);
    virtual const Vec3* getListenerVelocity() const { return m_listenerVelocity; }
    virtual void  setListenerReverb(float r);
    virtual float getListenerReverb() const { return m_listenerReverb; }
    virtual void  setDopplerEffect(float f);
    virtual float getDopplerEffect() const;
    virtual void  update(int steps);
    virtual void  setTinnitusSound(const char** names, int count);
    virtual void  startTinnitus(float attack, float hold, float release, float volume, int index);
    virtual void  stopTinnitus(bool force);
    virtual void  addTrigger(ISoundTemplate* t);
    virtual int   numTriggers() const;
    virtual void  setTrigger(ISound* s);
    virtual ISound* getTrigger() const;
    virtual void  removeTrigger(ISoundTemplate* t);
    virtual void  removeAllTriggers();
    virtual void  setProperty(const char* name, float v);
    virtual float getProperty(const char* name) const;
    virtual void  setReverb(const char* name);
    virtual void  setSoundFileIO(ISoundFileIO* io);
    virtual void  setSoundTrace(ISoundTrace* trace);
    virtual void* getSoundFiles(int* count);
    virtual void* get3dProviders(int* count);
    virtual void* getStats();
    virtual void  playAmbient(ISound* s, float fadeSeconds);
    virtual void  stopAmbient(float fadeSeconds);
    virtual void  setInteractiveMode(bool b) { m_interactiveMode = b; }
    virtual bool  getInteractiveMode() const { return m_interactiveMode; }
    virtual int   getEaxVersion() const;
    virtual const PlayingSampleInfo* queryPlayingSamples(int* count);   // 0x10004B80
    virtual void  setProfilerClient(IProfilerClient* p);
    virtual bool  loadMusic(const char* name);
    virtual bool  playMusic();
    virtual bool  stopMusic(float fadeSeconds);
    virtual bool  getIsUnderwater() const { return m_underwater; }
    virtual void  setIsUnderwater(bool b) { m_underwater = b; }
    virtual float getUnderwaterOcclusion() const { return m_underwaterOcclusion; }
    virtual void  setUnderwaterOcclusion(float o) { m_underwaterOcclusion = o; }

    // What the Sounds, Samples and helpers call
    Sample*    playSample(Sound* sound, int index, const Vec3* position);
    bool       stopSample(Sample* s, bool force);
    void       stopSound(Sound* sound, bool force);
    SoundFile* getOrCreateSoundFile(std::string name);                      // by value: lower-cased in place
    void       toListenerSpace(Sound* sound, Vec3* position, Vec3* velocity);
    float      airAbsorption(float distance);

private:
    bool  refreshProviders();
    int   openProvider(const std::string& name);
    void  updateVoiceLimit(int voices);
    void  loadAllFiles();
    void  unloadFiles(bool unloadStatic, bool unloadStreams);
    void  flushPending();
    void  stopNewSamples();
    void  clearList(std::list<Sample*>& l);
    void  clearDelayed();
    bool  stopInList(std::list<Sample*>& l, Sample* s, bool force);
    void  stopSoundInList(std::list<Sample*>& l, Sound* sound, bool force);
    void  stopAllInList(std::list<Sample*>& l, bool force);
    void  processStopRequests();
    bool  shouldCull(Sound* sound, const Vec3* position);
    void  schedule(Sample* s);
    void  updateBalance();
    void  updateTinnitus();
    void  updateListener();
    void  updateStats();
    void  updateSamples(int steps);
    void  startDelayed();
    bool  startWaiting();
    void  stealVoice();
    void  dropStale(std::list<Sample*>& l);
    void  computePriorities(std::list<Sample*>& l);
    void  pushDelayed(Sample* s);               // the push_back + push_heap pair every scheduler site inlines

public:
    bool                        m_initialized;          // +0x04
    int                         m_quality;              // +0x08
    std::vector<SoundFile*>     m_soundFiles;           // +0x0C (pad) +0x10 first
    std::vector<const char*>    m_providerNames;        // +0x1C (pad) +0x20 first
    ISoundProvider*             m_activeProvider;       // +0x2C m_provider while open, else 0
    ISoundProvider*             m_provider;             // +0x30 the OpenALProvider, for life
    std::vector<Sample*>        m_newSamples;           // +0x34 (pad) +0x38 first - played this frame
    std::list<Sample*>          m_playing;              // +0x44 (pad) +0x48 head
    std::list<Sample*>          m_waiting;              // +0x50 (pad) +0x54 head
    std::vector<Sample*>        m_delayed;              // +0x5C (pad) +0x60 first - a min-heap on m_readyFrame
    SampleReadyLater            m_delayedCompare;       // +0x6C the heap's comparator object
    std::vector<Sample*>        m_stopRequests;
    // Non-forced stops of newly queued samples, processed after scheduling.
    int                         m_sampleRate;           // +0x80 = 44100
    int                         m_bits;                 // +0x84 = 16
    float                       m_masterVolume;         // +0x88 = 0
    float                       m_groupVolume[8];       // +0x8C = 1.0 each
    float                       m_gain2d;               // +0xAC
    float                       m_gain3d;               // +0xB0
    const Vec3*                 m_listenerPosition;     // +0xB4 defaults to the zero vector
    const Vec3*                 m_listenerFacing;       // +0xB8 (0,0,1)
    const Vec3*                 m_listenerUp;           // +0xBC (0,1,0)
    const Vec3*                 m_listenerVelocity;     // +0xC0 zero
    Mat4                        m_listenerMatrix;       // +0xC4 world to listener (the inverse)
    Vec3                        m_listenerVelocityLocal;// +0x104
    bool                        m_ownsTrace;            // +0x110
    bool                        m_ownsFileIO;           // +0x111
    int                         m_frame;                // +0x114 sum of update steps
    int                         m_updateCount;          // +0x118 update calls
    SoundStats                  m_stats;                // +0x11C
    int                         m_maxVoices;            // +0x154 = 16
    float                       m_listenerReverb;       // +0x158 = 0.5
    ISound*                     m_tinnitusSound;        // +0x15C
    int                         m_tinnitusStart;        // +0x160
    int                         m_tinnitusEnd;          // +0x164 = -1
    float                       m_tinnitusVolume;       // +0x168
    float                       m_tinnitusT;            // +0x16C
    float                       m_tinnitusLevel;        // +0x170 what the samples read
    float                       m_tinnitusFadeOutAt;    // +0x174 = 0; -1 = no forced fade-out point
    AmbientPlayer               m_ambient;              // +0x178
    SoundTriggerManager         m_triggers;             // +0x1B0
    bool                        m_interactiveMode;      // +0x1C8
    std::vector<int>            m_unresolvedVector;     // +0x1CC (pad) +0x1D0 first - [UNRESOLVED] no reader
    Reverb*                     m_reverb;               // +0x1DC
    int                         m_voices;               // +0x1E0 initialize's third argument
    std::string                 m_providerName;         // +0x1E4 initialize's second argument
    int                         m_lastQuality;          // +0x200 initialize's first argument
    bool                        m_underwater;           // +0x204
    float                       m_underwaterOcclusion;  // +0x208
};

// The singletons and constants the TU owns
extern BfSoundEngine*   g_soundEngineInstance;  // ds:getSoundEngine's object
extern SoundProperties* g_soundProperties;      // ds:10046460
extern const int        g_framesPerSecond;
extern const Vec3       g_zeroVector;           // ds:10046464
extern const Vec3       g_defaultFacing;
extern const Vec3       g_defaultUp;
extern Mat4             g_identityMatrix;
extern float            g_maxDistanceCutoff;
// Refreshed each update.
extern float            g_airMinDistance;       // ds:the five air-absorption caches
extern float            g_airMaxDistance;       // ds:1004643C
extern float            g_airMax;               // ds:10046440
extern float            g_airEaxMinDistance;    // ds:10046444
extern float            g_airEaxMaxDistance;    // ds:10046448

// The DLL's own null pair (NullSound.h declares them; BF2Audio.dll news them in getSoundEngine).
// ds:1004644C / ds:10046450.

// The two exports (the entire ABI of the DLL; BF2Audio.def pins their ordinals)
extern "C" ISoundEngine* getSoundEngine();
extern "C" void          shutdownSoundEngine();

} // namespace hfe
} // namespace dice

#endif // SOUND_SOUNDENGINE_BFSOUNDENGINE_H
