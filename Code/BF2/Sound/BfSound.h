// Game-side sound facade and interface declarations. [INFERRED] Header placement.

#ifndef SOUND_BFSOUND_H     // [INFERRED] guard style
#define SOUND_BFSOUND_H

#include <string>
#include "NullSound.h"
#include <list>
#include "../Core/ConsoleObjectMacro.h"    // ConsoleObjectList - bfSoundConsoleObjects() below, both images
#if !DEDICATED
#include <map>
#include <vector>
#include "ISoundFileIO.h"
#include "ISoundTrace.h"
#include "ISoundObstructionEvaluator.h"
#include "../Game/IEventListener.h"
#include "../Math/Mat4.h"
#include "../Core/WeakPtr.h"
#endif

namespace dice {
namespace hfe {

struct Mat4;                           // dice::hfe::Mat4 (Math/Mat4.h) - hfe scope, unlike Vec3
class ConsoleObject;                   // Core/ConsoleObject.h - the console rows below
typedef std::list<ConsoleObject*> ConsoleObjectList;   // the same typedef Core/ConsoleObjectMacro.h has
class ISoundTemplate2;                 // placeholder tag - see createSound below
class SoundObjectTemplate;
namespace world { class IObject; }
// [UNRESOLVED] Mute-state names. The main loop passes state 2; preserve the numeric ABI.
enum MuteState { MuteState_UNRECOVERED = 0 };

class IBfSound
{
public:
    virtual void  addRef() = 0;                                              // +0x00
    virtual int   getRef() = 0;                                              // +0x04
    virtual int   release() = 0;                                             // +0x08
    virtual void* queryInterface(unsigned int) const = 0;                    // +0x0C
    virtual ~IBfSound() {}                                                   // +0x10/+0x14 (D1/D0)
    virtual void  update(int) = 0;                                           // +0x18
    virtual void  update(int, const Mat4&, const Vec3&, bool) = 0;           // +0x1C
    // the parameter is the engine-level ISoundTemplate (the mangled name says
    // PNS0_14ISoundTemplateE) - i.e. NullSound.h's interface, not a game-side type.
    virtual ISound* createSound(ISoundTemplate*, world::IObject*) = 0;       // +0x20
    virtual void  loadSoundFiles() = 0;                                      // +0x24
    virtual ISoundEngine* engine() const = 0;                                // +0x28 what the
                                                                             // SoundObjectTemplate
                                                                             // ctor dispatches
    virtual void  playSound(const std::string&) = 0;                         // +0x2C
    virtual void  stopSound(const std::string&) = 0;                         // +0x30
    virtual void  playSound(int) = 0;                                        // +0x34
    virtual void  stopSound(int) = 0;                                        // +0x38
    virtual int   getSoundId(const std::string&) const = 0;
    // Returns an entry from the facade's sound storage.
    virtual ISound* getSound(const std::string&) const = 0;                  // +0x40
    virtual ISound* getSound(int) const = 0;                                 // +0x44
    virtual void  addSound(const std::string&) = 0;                          // +0x48
    virtual void  addMenuSound(const std::string&, const std::string&,
                               float, bool) = 0;                             // +0x4C
    // Initializes the selected provider and quality, returning the sound engine's status code.
    virtual int   initialize(int, const std::string&, int) = 0;              // +0x50
    virtual int   qualityLevel() const = 0;                                  // +0x54
    virtual void  addTrigger(const std::string&) = 0;                        // +0x58
    virtual void  removeTrigger(SoundObjectTemplate*) = 0;                   // +0x5C
    virtual void* getTriggerList() = 0;                                      // +0x60 [UNRESOLVED ret]
    virtual void  setProperty(const std::string&, float) = 0;                // Forwards to the engine property setter.
    virtual std::string getProperty(const std::string&) const = 0;           // +0x68
    virtual void  setReverb(const std::string&) = 0;                         // +0x6C
    virtual void  tinnitusSetup(const std::string&, float, float,
                                float, float) = 0;
                                // Returns the tinnitus setup block passed as ECGameLogic event 13 data.
    virtual void* getTinnitusSetup() const = 0;                              // +0x74
    virtual void  startTinnitus() = 0;                                       // +0x78
    virtual void  stopTinnitus() = 0;                                        // +0x7C
    virtual void  startManDown() = 0;                                        // +0x80
    virtual void  stopManDown() = 0;                                         // +0x84
    virtual void  windSetup(const std::string&, float) = 0;                  // +0x88
    virtual void  startWind() = 0;                                           // +0x8C
    virtual void  stopWind() = 0;                                            // +0x90
    virtual bool  isOutdoors(const Vec3&) = 0;                               // +0x94
    // Slots 38..51, the end of the table (for AudioSettings)
    virtual void  setShowStats(bool) = 0;                                    // +0x98
    virtual bool  getShowStats() const = 0;                                  // +0x9C
    virtual void  setShowSoundSources(bool) = 0;                             // +0xA0
    virtual bool  getShowSoundSources() const = 0;                           // +0xA4
    virtual void  mute() = 0;                                                // +0xA8
    virtual void  unmute() = 0;                                              // +0xAC
    // Clamps group volume to [0, 1]. Groups 0/3 are effects, 1 music, and 2 help voice.
    virtual void  setVolumeForGroup(int, float) = 0;                         // +0xB0
    virtual float getVolumeForGroup(int) const = 0;                          // +0xB4
    virtual void  setMuteState(MuteState, bool) = 0;                         // +0xB8
    virtual std::string getAllSoundTemplates() const = 0;                    // +0xBC [CHOSEN ret] -
                                                                             // the same 47-byte
                                                                             // sret-string body as
                                                                             // getProperty
    virtual void  tweakTemplate(const std::string&, float, float, float,
                                float, float, float) = 0;                    // +0xC0
    virtual bool  loadMusic(const std::string&) = 0;                         // +0xC4 [CHOSEN ret]
    virtual void  setForceUpdate() = 0;                                      // +0xC8
    virtual bool  getForceUpdate() const = 0;                                // +0xCC [CHOSEN ret]
};

// The 8-byte null implementation: {vptr, m_refCount}. Every method read (5-57 B).
class BfSoundNull : public IBfSound
{
public:
    BfSoundNull() : m_refCount(1) {}           // BfSoundModule::init news it with refcount 1

    virtual void  addRef()  { ++m_refCount; }
    virtual int   getRef()  { return m_refCount; }
    virtual int   release();                                                 // delete on 0
    virtual void* queryInterface(unsigned int) const { return 0; }           // [UNRESOLVED
                                                                             // the iid test - no
                                                                             // recovered caller]
    virtual void  update(int) {}
    virtual void  update(int, const Mat4&, const Vec3&, bool) {}
    // the 10-B body is `mov eax, ds:8AC0DACh` - it returns the shared
    // NullSound, not 0. (The first reading assumed xor eax; the byte at +1 is A1, a load.)
    virtual ISound* createSound(ISoundTemplate*, world::IObject*) { return g_nullSound; }
    virtual void  loadSoundFiles() {}
    virtual ISoundEngine* engine() const { return g_nullSoundEngine; }
    virtual void  playSound(const std::string&) {}
    virtual void  stopSound(const std::string&) {}
    virtual void  playSound(int) {}
    virtual void  stopSound(int) {}
    virtual int   getSoundId(const std::string&) const { return 0; }
    virtual ISound* getSound(const std::string&) const { return 0; }
    virtual ISound* getSound(int) const { return 0; }
    virtual void  addSound(const std::string&) {}
    virtual void  addMenuSound(const std::string&, const std::string&, float, bool) {}
    virtual int   initialize(int, const std::string&, int) { return 0; }     // xor eax, both images
    virtual int   qualityLevel() const { return 0; }
    virtual void  addTrigger(const std::string&) {}
    virtual void  removeTrigger(SoundObjectTemplate*) {}
    virtual void* getTriggerList() { return 0; }
    virtual void  setProperty(const std::string&, float) {}
    virtual std::string getProperty(const std::string&) const { return std::string(); }
    virtual void  setReverb(const std::string&) {}
    virtual void  tinnitusSetup(const std::string&, float, float, float, float) {}
    virtual void* getTinnitusSetup() const { return 0; }
    virtual void  startTinnitus() {}
    virtual void  stopTinnitus() {}
    virtual void  startManDown() {}
    virtual void  stopManDown() {}
    virtual void  windSetup(const std::string&, float) {}
    virtual void  startWind() {}
    virtual void  stopWind() {}
    virtual bool  isOutdoors(const Vec3&) { return true; }
    virtual void  setShowStats(bool) {}
    virtual bool  getShowStats() const { return false; }                     // xor eax
    virtual void  setShowSoundSources(bool) {}
    virtual bool  getShowSoundSources() const { return false; }              // xor eax
    virtual void  mute() {}
    virtual void  unmute() {}
    virtual void  setVolumeForGroup(int, float) {}                           // / PE retn 8
    virtual float getVolumeForGroup(int) const { return 0.0f; }
    virtual void  setMuteState(MuteState, bool) {}
    virtual std::string getAllSoundTemplates() const { return std::string(); }
    virtual void  tweakTemplate(const std::string&, float, float, float,
                                float, float, float) {}
    virtual bool  loadMusic(const std::string&) { return true; }             // mov eax, 1
    virtual void  setForceUpdate() {}
    virtual bool  getForceUpdate() const { return false; }                   // xor eax

    int m_refCount;                            // +0x04
};

// Sound facade and engine services, published during module initialization and cleared on shutdown.
extern IBfSound*     g_bfSound;
extern ISoundEngine* g_soundEngine;

// Console rows registered and removed by BfSoundModule; Sound.* rows are client-only.
ConsoleObjectList& bfSoundConsoleObjects();

#if !DEDICATED

// Client sound facade implementing engine callbacks for file access, tracing and obstruction.

class BfSound : public IBfSound,
                public ISoundFileIO,
                public ISoundTrace,
                public IEventListener,
                public ISoundObstructionEvaluator
{
public:
    BfSound();
    virtual ~BfSound();
    void init();
    // IBfSound primary interface; preserve declaration order.
    virtual void  addRef();                                         // (a shared 8-B body)
    virtual int   getRef();
    virtual int   release();                                        // D0 on zero
    virtual void* queryInterface(unsigned int iid) const;
    virtual void  update(int steps);
    virtual void  update(int steps, const Mat4& listener,
                         const Vec3& velocity, bool windActive);
    virtual ISound* createSound(ISoundTemplate*, world::IObject*);
    virtual void  loadSoundFiles();
    virtual ISoundEngine* engine() const;                           // g_soundEngine
    virtual void  playSound(const std::string&);
    virtual void  stopSound(const std::string&);
    virtual void  playSound(int);
    virtual void  stopSound(int);
    virtual int   getSoundId(const std::string&) const;
    virtual ISound* getSound(const std::string&) const;
    virtual ISound* getSound(int) const;
    virtual void  addSound(const std::string&);
    virtual void  addMenuSound(const std::string&, const std::string&, float, bool);
    virtual int   initialize(int, const std::string&, int);
    virtual int   qualityLevel() const;
    virtual void  addTrigger(const std::string&);
    virtual void  removeTrigger(SoundObjectTemplate*);
    virtual void* getTriggerList();                                 // xor eax
    virtual void  setProperty(const std::string&, float);
    virtual std::string getProperty(const std::string&) const;
    virtual void  setReverb(const std::string&);
    virtual void  tinnitusSetup(const std::string&, float, float, float, float);
    virtual void* getTinnitusSetup() const;                         // lea eax,[ecx+60h]
    virtual void  startTinnitus();
    virtual void  stopTinnitus();
    virtual void  startManDown();
    virtual void  stopManDown();
    virtual void  windSetup(const std::string&, float);
    virtual void  startWind();
    virtual void  stopWind();
    virtual bool  isOutdoors(const Vec3&);
    virtual void  setShowStats(bool);                               // (shared byte store)
    virtual bool  getShowStats() const;
    virtual void  setShowSoundSources(bool);
    virtual bool  getShowSoundSources() const;
    virtual void  mute();
    virtual void  unmute();
    virtual void  setVolumeForGroup(int, float);
    virtual float getVolumeForGroup(int) const;
    virtual void  setMuteState(MuteState, bool);
    virtual std::string getAllSoundTemplates() const;
    virtual void  tweakTemplate(const std::string&, float, float, float, float, float, float);
    virtual bool  loadMusic(const std::string&);
    virtual void  setForceUpdate();
    virtual bool  getForceUpdate() const;

    // File access callbacks.
    virtual void* open(const char* name);
    virtual int   close(void* handle);
    virtual int   seek(void* handle, int offset, int origin);
    virtual int   read(void* handle, void* buffer, int bytes);

    // Trace callbacks intentionally do nothing.
    virtual void  error(const char*, const char*, int) {}
    virtual void  warning(const char*, const char*, int) {}
    virtual void  info(const char*, const char*, int) {}
    virtual void  assertFailed(const char*, const char*, int) {}
    virtual void  debug(const char*, const char*, int) {}

    // Event listener.
    virtual void  handleEvent(EventCategory category, unsigned int id,
                              EventNode* node, void* data);

    // Sound obstruction evaluation.
    virtual float evaluate(ISound* sound, const Math::Vec3* position);

    // The block getTinnitusSetup() hands out (this+0x60): Soldier::stopTinnitus posts its address
    // as ECGameLogic 13's data.
    struct TinnitusSetup
    {
        int   state;        // +0x60 0 after setup, 1 while started, 2 after a stop
        float attack;       // +0x64 ctor 0.5
        float hold;         // +0x68 ctor 2.0
        float release;      // +0x6C ctor 2.0
        float volume;       // +0x70 ctor 0.8
    };
    struct VolumeGroup
    {
        float volume;       // what setVolumeForGroup stores (clamped 0..1)
        float current;      // the fader's position, chasing `target` by 1/15 per update
        float target;       // derived from the mute-state table every update
    };

private:
    void  updateWind(bool active);
    void  updateGroups();
    void  loadGameSounds();                                         // the CEPostLoad arm
    void  drawStats();                                              // BF2.exe 0x7E08D0 - DebugTextWriterFixed, see .cpp
    void  drawSoundSources();                                       // BF2.exe 0x7E0E80, see .cpp

    int                         m_refCount;             // +0x14 ctor 1
    std::map<std::string, int>  m_soundIds;             // +0x18 name -> index; menu ids carry 0x1000000
    std::vector<ISound*>        m_sounds;               // +0x24 addSound's sounds
    std::vector<ISound*>        m_menuSounds;           // +0x34 addMenuSound's sounds
    std::string                 m_providerName;         // +0x44 initialize's second argument
    TinnitusSetup               m_tinnitus;             // +0x60
    bool                        m_showStats;            // +0x74
    bool                        m_showSoundSources;     // +0x75
    Mat4                        m_listenerMatrix;       // +0x78 ctor identity
    Vec3                        m_listenerVelocity;     // +0xB8
    int                         m_muteCount;            // +0xC4
    float                       m_savedMasterVolume;    // +0xC8 what unmute restores
    ISound*                     m_windSound;            // +0xCC
    float                       m_windFactor;           // +0xD0 windSetup's factor, >= 1
    float                       m_windParam;            // +0xD4 |listener velocity| / factor, 0..1;
                                                        // the wind sound's volume and pitch evaluator input
    bool                        m_windPlaying;          // +0xD8
    bool                        m_windOn;               // +0xD9
    WeakPtr<world::IObject>     m_ignoreObject;         // +0xDC the local player's vehicle root;
                                                        // the obstruction rays ignore it
    VolumeGroup                 m_groups[8];            // +0xE0 ctor all 1.0
    bool                        m_muteStates[4];        // +0x140 setMuteState's table (see MuteState)
    bool                        m_forceUpdate;          // +0x144
};
#endif // !DEDICATED

} // namespace hfe
} // namespace dice

#endif
