// Dedicated-server sound implementation with shared null engine, template and sound instances.
// [INFERRED] source placement and ISoundEngine/ISoundTemplate names.
// Numeric getters return zero, initialization returns an OpenAL error, and setters have no effect.
// Preserve interface order: sound/template use addRef/release without an IBase quartet;
// The engine starts with initialize.

#ifndef SOUND_NULLSOUND_H     // [INFERRED] guard style
#define SOUND_NULLSOUND_H

#include "../Math/Vec3.h"     // NullSoundEngine embeds the zero vector its listener getters return

namespace dice {
namespace hfe {

struct SoundEnvelopePoint;            // 3 floats / 12 bytes; defined by SoundObjectTemplate.cpp
// Quality values: 2 low, 1 medium, 0 high/ultra-high. [UNRESOLVED] Enumerator names.
enum SoundQuality { SoundQuality_UNRECOVERED = 0 };
class ISound;                         // below
class ISoundNode;                     // parameter-only [UNRESOLVED]
class ISoundEvaluator;                // parameter-only [UNRESOLVED]
class ISoundObstructionEvaluator;     // parameter-only [UNRESOLVED]
class ISoundFileIO;
class ISoundTrace;
class IProfilerClient;                // Parameter-only; unused by the audio DLL.


// The per-sound-template interface - 53 slots, order from the NullSoundTemplate vtable (full dump).

class ISoundTemplate
{
public:
    virtual void  addRef() = 0;                                              // +0x00
    virtual void  release() = 0;                                             // +0x04
    virtual void  setFilename(const char*) = 0;                              // +0x08
    virtual const char* getFilename() const = 0;                             // +0x0C
    virtual void  setFilenames(const char**, int) = 0;                       // +0x10
    virtual const char** getFilenames(int*) const = 0;                       // +0x14
    virtual void  setLoopCount(unsigned int) = 0;                            // +0x18
    virtual unsigned int getLoopCount() const = 0;                           // +0x1C
    virtual void  setVolume(float) = 0;                                      // +0x20
    virtual float getVolume() const = 0;                                     // +0x24
    virtual void  setVolumeEnvelope(float, float, float, float, bool,
                                    unsigned int, const SoundEnvelopePoint*) = 0;  // +0x28
    virtual const SoundEnvelopePoint* getVolumeEnvelope(
                                    float*, float*, float*, float*,
                                    bool*, unsigned int*) = 0;               // +0x2C
    virtual void* getVolumeEvaluator() const = 0;                            // +0x30 [UNRESOLVED ret]
    virtual void  setPitch(float) = 0;                                       // +0x34
    virtual float getPitch() const = 0;                                      // +0x38
    virtual void  setPitchEnvelope(float, float, float, float, bool,
                                   unsigned int, const SoundEnvelopePoint*) = 0;   // +0x3C
    virtual const SoundEnvelopePoint* getPitchEnvelope(
                                   float*, float*, float*, float*,
                                   bool*, unsigned int*) = 0;                // +0x40
    virtual void* getPitchEvaluator() const = 0;                             // +0x44 [UNRESOLVED ret]
    virtual void  setVolumeGroup(int) = 0;                                   // +0x48
    virtual int   getVolumeGroup() const = 0;                                // +0x4C
    virtual void  setPan(float) = 0;                                         // +0x50
    virtual float getPan() const = 0;                                        // +0x54
    virtual void  setMinDistance(float) = 0;                                 // +0x58
    virtual float getMinDistance() const = 0;                                // +0x5C
    virtual void  setHalfVolumeDistance(float) = 0;                          // +0x60
    virtual float getHalfVolumeDistance() const = 0;                         // +0x64
    virtual void  setIs3dSound(bool) = 0;                                    // +0x68
    virtual bool  getIs3dSound() const = 0;                                  // +0x6C
    virtual void  setStopType(int) = 0;                                      // +0x70
    virtual int   getStopType() const = 0;                                   // +0x74
    virtual void  setReverbLevel(float) = 0;                                 // +0x78
    virtual float getReverbLevel() const = 0;                                // +0x7C
    virtual void  setPosition(const Vec3&) = 0;                              // +0x80
    virtual const Vec3& getPosition() const = 0;                             // +0x84
    virtual void  setDirection(const Vec3&) = 0;                             // +0x88
    virtual const Vec3& getDirection() const = 0;                            // +0x8C
    virtual void  setRadius(float) = 0;                                      // +0x90
    virtual float getRadius() const = 0;                                     // +0x94
    virtual void  setConeInnerAngle(float) = 0;                              // +0x98
    virtual float getConeInnerAngle() const = 0;                             // +0x9C
    virtual void  setConeOuterAngle(float) = 0;                              // +0xA0
    virtual float getConeOuterAngle() const = 0;                             // +0xA4
    // The tail the first dump missed (slots 43..53, )
    virtual void  setConeOuterVolume(float) = 0;                             // +0xA8
    virtual float getConeOuterVolume() const = 0;                            // +0xAC
    virtual void  setGlobalEffectLevel(float) = 0;                           // +0xB0
    virtual float getGlobalEffectLevel() const = 0;                          // +0xB4
    virtual void  setIsUsed() = 0;                                           // +0xB8 SoundObject
                                                                             // Template::setIsUsed
                                                                             // forwards here
    virtual void  setMaxSamples(SoundQuality, int) = 0;                      // +0xBC quality 2 =
    virtual int   getMaxSamples(SoundQuality) const = 0;                     // +0xC0 low, 1 = med
    virtual void  setFlags(unsigned int) = 0;                                // +0xC4 the pair SOT
    virtual unsigned int getFlags() const = 0;                               // +0xC8 :templateIni
                                                                             // tialize or-2s
    virtual bool  checkSampleAvailable(int) const = 0;                       // +0xCC
    virtual ISound* createSound() = 0;                                       // +0xD0 the shared
                                                                             // NullSound on server
};


// Core sound-engine interface shared with the audio DLL. Preserve virtual declaration order.

// One playing sample as ISoundEngine::queryPlayingSamples hands it out: 0x2C bytes, the fields
// BF2.exe's BfSound::drawSoundSources (0x7E0E80) reads. Names are [INFERRED] from the overlay's
// "V:"/"O:"/"P:" labels and from how each field is drawn. The shipped BF2Audio.dll never returns
// any (0x10004B80: count 0, null).
struct PlayingSampleInfo
{
    Vec3  position;       // +0x00
    float minDistance;    // +0x0C  first circle radius
    float maxDistance;    // +0x10  second circle radius; <= 0 draws twice minDistance
    float volume;         // +0x14  "V: %.2f"
    float obstruction;    // +0x18  "O: %.2f"
    Vec3  velocity;       // +0x1C  drawn from position when longer than sqrt(0.1)
    float pitch;          // +0x28  "P: %.2f"
};

class ISoundEngine
{
public:
    // Returns 0 on success; errors are 1 missing provider, 2 missing DLL,
    // 3 OpenAL denied, and 4 OpenAL error. The null engine returns 4.
    virtual int   initialize(SoundQuality, const char*, int) = 0;            // +0x00
    virtual bool  isInitialized() const = 0;                                 // +0x04
    virtual void  shutdown() = 0;                                            // +0x08
    virtual int   getQualityLevel() const = 0;                               // +0x0C
    virtual ISoundTemplate* createTemplate() = 0;                            // +0x10 the slot the
                                                                             // SoundObjectTemplate
                                                                             // ctor dispatches
    virtual void  loadSoundFiles() = 0;                                      // +0x14
    virtual void  stopAllSamples() = 0;                                      // +0x18
    virtual void  setMasterVolume(float) = 0;                                // +0x1C AudioSettings
                                                                             // ::applyRecord 4B1441
    virtual float getMasterVolume() const = 0;                               // +0x20
    virtual void  setVolumeForGroup(int, float) = 0;                         // +0x24
    virtual float getVolumeForGroup(int) const = 0;                          // +0x28
    virtual void  setListenerPosition(const Vec3*) = 0;                      // +0x2C
    // Listener getters expose the null engine's shared zero vector. [CHOSEN ret] Pointer form.
    virtual const Vec3* getListenerPosition() const = 0;                     // +0x30
    virtual void  setListenerOrientation(const Vec3*, const Vec3*) = 0;      // +0x34
    virtual const Vec3* getListenerOrientationFacing() const = 0;            // +0x38
    virtual const Vec3* getListenerOrientationUp() const = 0;                // +0x3C
    virtual void  setListenerVelocity(const Vec3*) = 0;                      // +0x40
    virtual const Vec3* getListenerVelocity() const = 0;                     // +0x44
    virtual void  setListenerReverb(float) = 0;                              // +0x48
    virtual float getListenerReverb() const = 0;                             // +0x4C
    virtual void  setDopplerEffect(float) = 0;                               // +0x50
    virtual float getDopplerEffect() const = 0;                              // +0x54
    virtual void  update(int) = 0;                                           // +0x58
    virtual void  setTinnitusSound(const char**, int) = 0;                   // +0x5C
    virtual void  startTinnitus(float, float, float, float, int) = 0;        // +0x60
    virtual void  stopTinnitus(bool) = 0;                                    // +0x64
    virtual void  addTrigger(ISoundTemplate*) = 0;                           // +0x68
    virtual int   numTriggers() const = 0;                                   // +0x6C
    virtual void  setTrigger(ISound*) = 0;                                   // +0x70
    virtual ISound* getTrigger() const = 0;                                  // +0x74 [CHOSEN ret]
    virtual void  removeTrigger(ISoundTemplate*) = 0;                        // +0x78
    virtual void  removeAllTriggers() = 0;                                   // +0x7C
    virtual void  setProperty(const char*, float) = 0;
    // Engine property setter used by the client facade.
    virtual float getProperty(const char*) const = 0;                        // +0x84
    virtual void  setReverb(const char*) = 0;                                // +0x88
    virtual void  setSoundFileIO(ISoundFileIO*) = 0;                         // +0x8C
    virtual void  setSoundTrace(ISoundTrace*) = 0;                           // +0x90
    virtual void* getSoundFiles(int*) = 0;                                   // +0x94 [UNRESOLVED ret]
    virtual void* get3dProviders(int*) = 0;                                  // +0x98 [UNRESOLVED ret]
    virtual void* getStats() = 0;                                            // +0x9C [UNRESOLVED ret]
    virtual void  playAmbient(ISound*, float) = 0;                           // +0xA0
    virtual void  stopAmbient(float) = 0;                                    // +0xA4
    virtual void  setInteractiveMode(bool) = 0;                              // +0xA8
    virtual bool  getInteractiveMode() const = 0;                            // +0xAC
    virtual int   getEaxVersion() const = 0;                                 // +0xB0
    virtual const PlayingSampleInfo* queryPlayingSamples(int* count) = 0;    // +0xB4 (BF2.exe walks the result)
    virtual void  setProfilerClient(IProfilerClient*) = 0;                   // +0xB8
    virtual bool  loadMusic(const char*) = 0;                                // +0xBC [CHOSEN ret]
    virtual bool  playMusic() = 0;                                           // +0xC0 [CHOSEN ret] -
                                                                             // AudioSettings::apply
                                                                             // 4B17AF after a failed
                                                                             // re-initialisation
    virtual bool  stopMusic(float) = 0;                                      // +0xC4 [CHOSEN ret]
    virtual bool  getIsUnderwater() const = 0;                               // +0xC8
    virtual void  setIsUnderwater(bool) = 0;                                 // +0xCC
    virtual float getUnderwaterOcclusion() const = 0;                        // +0xD0
    virtual void  setUnderwaterOcclusion(float) = 0;                         // +0xD4

    // This shared interface ends at setUnderwaterOcclusion.
    // The audio DLL places its deleting destructor in the next slot; adding setSWReverb here would shift that ABI.
    // The executable's extra retail null-engine slot has no caller in the current interface.

};


// The null pair. Both 16 bytes in the ELF: {vptr, three zeroed dwords} - the ctors were read.

class NullSoundTemplate : public ISoundTemplate
{
public:
    NullSoundTemplate();                       // static-init TU, 16-byte object

    virtual void  addRef();
    virtual void  release();                   // refcount only, no delete (the singleton
                                               // is shared by every SoundObjectTemplate)
    virtual void  setFilename(const char*) {}                       // all setters: 5 B empty
    virtual const char* getFilename() const;
    virtual void  setFilenames(const char**, int) {}
    virtual const char** getFilenames(int*) const;
    virtual void  setLoopCount(unsigned int) {}
    virtual unsigned int getLoopCount() const { return 0; }
    virtual void  setVolume(float) {}
    virtual float getVolume() const { return 0.0f; }                // fldz
    virtual void  setVolumeEnvelope(float, float, float, float, bool,
                                    unsigned int, const SoundEnvelopePoint*) {}
    virtual const SoundEnvelopePoint* getVolumeEnvelope(
                                    float*, float*, float*, float*, bool*, unsigned int*)
                                    { return 0; }
    virtual void* getVolumeEvaluator() const { return 0; }
    virtual void  setPitch(float) {}
    virtual float getPitch() const { return 0.0f; }                 // fldz
    virtual void  setPitchEnvelope(float, float, float, float, bool,
                                   unsigned int, const SoundEnvelopePoint*) {}
    virtual const SoundEnvelopePoint* getPitchEnvelope(
                                   float*, float*, float*, float*, bool*, unsigned int*)
                                   { return 0; }
    virtual void* getPitchEvaluator() const { return 0; }
    virtual void  setVolumeGroup(int) {}
    virtual int   getVolumeGroup() const { return 0; }
    virtual void  setPan(float) {}
    virtual float getPan() const { return 0.0f; }
    virtual void  setMinDistance(float) {}
    virtual float getMinDistance() const { return 0.0f; }           // fldz
    virtual void  setHalfVolumeDistance(float) {}
    virtual float getHalfVolumeDistance() const { return 0.0f; }    // fldz
    virtual void  setIs3dSound(bool) {}
    virtual bool  getIs3dSound() const { return false; }
    virtual void  setStopType(int) {}
    virtual int   getStopType() const { return 0; }
    virtual void  setReverbLevel(float) {}
    virtual float getReverbLevel() const { return 0.0f; }           // fldz
    virtual void  setPosition(const Vec3&) {}
    virtual const Vec3& getPosition() const;                        // a static zero vector
    virtual void  setDirection(const Vec3&) {}
    virtual const Vec3& getDirection() const;
    virtual void  setRadius(float) {}
    virtual float getRadius() const { return 0.0f; }                // fldz
    virtual void  setConeInnerAngle(float) {}
    virtual float getConeInnerAngle() const { return 0.0f; }
    virtual void  setConeOuterAngle(float) {}
    virtual float getConeOuterAngle() const { return 0.0f; }
    virtual void  setConeOuterVolume(float) {}
    virtual float getConeOuterVolume() const { return 0.0f; }
    virtual void  setGlobalEffectLevel(float) {}
    virtual float getGlobalEffectLevel() const { return 0.0f; }
    virtual void  setIsUsed() {}
    virtual void  setMaxSamples(SoundQuality, int) {}
    virtual int   getMaxSamples(SoundQuality) const { return 0; }
    virtual void  setFlags(unsigned int) {}
    virtual unsigned int getFlags() const { return 0; }
    virtual bool  checkSampleAvailable(int) const { return false; }
    virtual ISound* createSound();
    // Returns the shared null sound.

    int m_refCount;                            // +0x04 the ctor zeroes +0x04/+0x08/+0x0C
    int m_field08;                             // [UNRESOLVED] - zeroed, no recovered reader
    int m_field0C;                             // [UNRESOLVED]
};

class NullSoundEngine : public ISoundEngine
{
public:
    NullSoundEngine();                         // static-init TU, 16-byte object

    virtual int   initialize(SoundQuality, const char*, int) { return 4; }
    // Reports OpenAL error.
    virtual bool  isInitialized() const { return false; }                        // xor eax
    virtual void  shutdown() {}
    virtual int   getQualityLevel() const { return 0; }
    virtual ISoundTemplate* createTemplate();
    // Returns the shared null template.
    virtual void  loadSoundFiles() {}
    virtual void  stopAllSamples() {}
    virtual void  setMasterVolume(float) {}
    virtual float getMasterVolume() const { return 0.0f; }
    virtual void  setVolumeForGroup(int, float) {}
    virtual float getVolumeForGroup(int) const { return 0.0f; }
    virtual void  setListenerPosition(const Vec3*) {}
    // Slots 12..54: every setter is a 5-byte empty body; the getters are noted
    virtual const Vec3* getListenerPosition() const { return &m_zeroVector; }          // this+4
    virtual void  setListenerOrientation(const Vec3*, const Vec3*) {}
    virtual const Vec3* getListenerOrientationFacing() const { return &m_zeroVector; } // this+4
    virtual const Vec3* getListenerOrientationUp() const { return &m_zeroVector; }     // this+4
    virtual void  setListenerVelocity(const Vec3*) {}
    virtual const Vec3* getListenerVelocity() const { return &m_zeroVector; }          // this+4
    virtual void  setListenerReverb(float) {}
    virtual float getListenerReverb() const { return 0.0f; }                // fldz
    virtual void  setDopplerEffect(float) {}
    virtual float getDopplerEffect() const { return 0.0f; }                 // fldz
    virtual void  update(int) {}
    virtual void  setTinnitusSound(const char**, int) {}
    virtual void  startTinnitus(float, float, float, float, int) {}
    virtual void  stopTinnitus(bool) {}
    virtual void  addTrigger(ISoundTemplate*) {}
    virtual int   numTriggers() const { return 0; }                         // xor eax
    virtual void  setTrigger(ISound*) {}
    virtual ISound* getTrigger() const { return 0; }                        // xor eax
    virtual void  removeTrigger(ISoundTemplate*) {}
    virtual void  removeAllTriggers() {}
    virtual void  setProperty(const char*, float) {}
    virtual float getProperty(const char*) const { return 0.0f; }           // fldz
    virtual void  setReverb(const char*) {}
    virtual void  setSoundFileIO(ISoundFileIO*) {}
    virtual void  setSoundTrace(ISoundTrace*) {}
    virtual void* getSoundFiles(int*) { return 0; }                         // xor eax
    virtual void* get3dProviders(int*) { return 0; }                        // xor eax
    virtual void* getStats() { return 0; }                                  // xor eax
    virtual void  playAmbient(ISound*, float) {}
    virtual void  stopAmbient(float) {}
    virtual void  setInteractiveMode(bool) {}
    virtual bool  getInteractiveMode() const { return false; }              // xor eax
    virtual int   getEaxVersion() const { return 0; }                       // xor eax
    virtual const PlayingSampleInfo* queryPlayingSamples(int*) { return 0; } // xor eax
    virtual void  setProfilerClient(IProfilerClient*) {}
    virtual bool  loadMusic(const char*) { return true; }                   // mov eax, 1 / mov al, 1
    virtual bool  playMusic() { return true; }                              // mov eax, 1 / mov al, 1
    virtual bool  stopMusic(float) { return false; }                        // xor eax / xor al
    virtual bool  getIsUnderwater() const { return false; }                 // xor eax
    virtual void  setIsUnderwater(bool) {}
    virtual float getUnderwaterOcclusion() const { return 0.0f; }           // fldz
    virtual void  setUnderwaterOcclusion(float) {}
    // Shared zero vector returned by all four listener getters. [INFERRED] Name.
    Vec3 m_zeroVector;
};


// Interface for one sound instance. Preserve virtual declaration order across the audio DLL boundary.

class ISound
{
public:
    virtual int   addRef() = 0;                                              // +0x00 returns 1 on
    virtual int   release() = 0;                                             // +0x04 the null: the
                                                                             // shared instance is
                                                                             // never destroyed
    virtual void  setSoundFlags(unsigned int) = 0;                           // +0x08
    virtual unsigned int getSoundFlags() const = 0;                          // +0x0C
    virtual void  play() = 0;                                                // +0x10
    virtual void  play(int) = 0;                                             // +0x14
    virtual void  playAt(const Vec3&) = 0;                                   // +0x18
    virtual void  playAt(const Vec3&, int) = 0;                              // +0x1C
    virtual int   playSample(int) = 0;                                       // +0x20
    virtual int   playSample(int, int) = 0;                                  // +0x24
    virtual int   playSampleAt(const Vec3&, int) = 0;                        // +0x28
    virtual int   playSampleAt(const Vec3&, int, int) = 0;                   // +0x2C
    virtual void  stop() = 0;                                                // +0x30
    virtual void  forceStop() = 0;                                           // +0x34
    virtual bool  isPlaying() = 0;                                           // +0x38
    virtual void  setParent(ISoundNode*) = 0;                                // +0x3C
    virtual ISoundNode* getParent() const = 0;                               // +0x40
    virtual void  setVolumeEvaluator(ISoundEvaluator*, const float*) = 0;    // +0x44
    virtual ISoundEvaluator* getVolumeEvaluator() const = 0;                 // +0x48
    virtual void  setPitchEvaluator(ISoundEvaluator*, const float*) = 0;     // +0x4C
    virtual ISoundEvaluator* getPitchEvaluator() const = 0;                  // +0x50
    virtual void  setObstructionEvaluator(ISoundObstructionEvaluator*) = 0;  // +0x54
    virtual ISoundObstructionEvaluator* getObstructionEvaluator() const = 0; // +0x58
    virtual void  setIs1PSound(bool) = 0;                                    // +0x5C
    virtual bool  getIs1PSound() const = 0;                                  // +0x60
    virtual void  setUserData(int, void*) = 0;                               // +0x64
    virtual void* getUserData(int) const = 0;                                // +0x68
    virtual void* getSampleInfo(int) const = 0;                              // +0x6C [UNRESOLVED ret]
    virtual ISoundTemplate* getTemplate() const = 0;                         // +0x70
};

// The 4-byte null sound: a vptr and nothing else. addRef/release return a constant 1 (there is no
// refcount field to touch - the object is process-lifetime shared). getTemplate returns the shared
// NullSoundTemplate. Everything else: empty / zero.
class NullSound : public ISound
{
public:
    NullSound() {}                             // ELF ctor: vptr store only (operator new(4))

    virtual int   addRef()  { return 1; }                                    // mov eax, 1
    virtual int   release() { return 1; }                                    // mov eax, 1
    virtual void  setSoundFlags(unsigned int) {}
    virtual unsigned int getSoundFlags() const { return 0; }
    virtual void  play() {}
    virtual void  play(int) {}
    virtual void  playAt(const Vec3&) {}
    virtual void  playAt(const Vec3&, int) {}
    virtual int   playSample(int) { return 0; }
    virtual int   playSample(int, int) { return 0; }
    virtual int   playSampleAt(const Vec3&, int) { return 0; }
    virtual int   playSampleAt(const Vec3&, int, int) { return 0; }
    virtual void  stop() {}
    virtual void  forceStop() {}
    virtual bool  isPlaying() { return false; }
    virtual void  setParent(ISoundNode*) {}
    virtual ISoundNode* getParent() const { return 0; }
    virtual void  setVolumeEvaluator(ISoundEvaluator*, const float*) {}
    virtual ISoundEvaluator* getVolumeEvaluator() const { return 0; }
    virtual void  setPitchEvaluator(ISoundEvaluator*, const float*) {}
    virtual ISoundEvaluator* getPitchEvaluator() const { return 0; }
    virtual void  setObstructionEvaluator(ISoundObstructionEvaluator*) {}
    virtual ISoundObstructionEvaluator* getObstructionEvaluator() const { return 0; }
    virtual void  setIs1PSound(bool) {}
    virtual bool  getIs1PSound() const { return false; }
    virtual void  setUserData(int, void*) {}
    virtual void* getUserData(int) const { return 0; }
    virtual void* getSampleInfo(int) const { return 0; }
    virtual ISoundTemplate* getTemplate() const;   // the shared NullSoundTemplate
};

// The three static-init singletons (ELF: constructed before main by the sound TU's initialiser,
// in this order). [CHOSEN] names - the globals carry no symbols.
extern ISound*         g_nullSound;
extern ISoundTemplate* g_nullSoundTemplate;
extern ISoundEngine*   g_nullSoundEngine;

} // namespace hfe
} // namespace dice

#endif
