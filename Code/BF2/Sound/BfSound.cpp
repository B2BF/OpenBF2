
// [FRONTIER] Sound debug overlays and remaining sound paths stop where their implementation is missing.
// [RETIRE-WHEN: bfsound-debug-draw] Complete and validate the corresponding BfSound operation.


#include "BfSound.h"
#include "SoundObjectTemplate.h"
#include "../Core/ConsoleObjectMacro.h"
#include "../Game/Utility/Vars.h"
#include "../Scene/Object/ObjectTemplateUtil.h"      // world::getActiveTemplate - the rows' gate
#if !DEDICATED
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "SoundEngine/BfSoundEngine.h"               // SoundStats - the block getStats() returns
#include "../Core/ClassManager.h"
#include "../Core/Debug.h"
#include "../Core/EventManager.h"
#include "../Core/Ids.h"                             // CID_SoldierTemplate
#include "../Core/IProfilerClient.h"
#include "../Core/LoadStat.h"                        // NextLoadPart - CEUpdateLoadStats' payload
#include "../Core/SmartItf.h"                        // classManager
#include "../Core/StringUtil.h"                      // formatString
#include "../Core/System.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/ObjectLayoutAccess.h"    // LayoutAccess::physicsNode - the +0x44 cache
#include "../Game/Objects/IPlayer.h"
#include "../IO/Console/Console.h"
#include "../IO/File/FileManager.h"
#include "../IO/Stream/IStream.h"
#include "../Physics/IPhysicsNode.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#endif

namespace dice {
namespace hfe {

// Sound interfaces remain null until BfSoundModule::init.
IBfSound*     g_bfSound     = 0;
ISoundEngine* g_soundEngine = 0;

// the only non-trivial BfSoundNull method - refcount decrement with
// delete-on-zero (the D0 path through the vtable).
int BfSoundNull::release()
{
    const int r = --m_refCount;
    if (r == 0)
        delete this;
    return r;
}


// Register in this order for both images; only the client facade reads these settings.
// [CHOSEN] Local variable names.

namespace {
float* s_listenerRadius            = Vars::getFloat("sound-listener-radius",          0.2f);
float* s_obstructionMax            = Vars::getFloat("sound-obstruction-max",          0.8f);
float* s_obstructionRadius         = Vars::getFloat("sound-obstruction-radius",       2.0f);
int*   s_obstructionOpt0           = Vars::getInt  ("sound-obstruction-opt0",         1);
int*   s_obstructionOpt1           = Vars::getInt  ("sound-obstruction-opt1",         1);
int*   s_obstructionDisable        = Vars::getInt  ("sound-obstruction-disable",      0);
int*   s_obstructionReuseFrames    = Vars::getInt  ("sound-obstruction-reuse-frames", 0);
int*   s_obstructionMaxReuseFrames = Vars::getInt  ("sound-obstruction-max-reuse-frames", 15);
int*   s_optLateLoading            = Vars::getInt  ("opt-late-loading",               0);
} // namespace

#if !DEDICATED

// File-local vector operations for listener and obstruction calculations.

namespace {

inline Vec3  vec3(float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }
inline float lengthSq(const Vec3& v)         { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline float length(const Vec3& v)           { return sqrtf(lengthSq(v)); }
inline Vec3  operator+(const Vec3& a, const Vec3& b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3  operator-(const Vec3& a, const Vec3& b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3  operator*(const Vec3& a, float s)       { return vec3(a.x * s, a.y * s, a.z * s); }
inline Vec3  cross(const Vec3& a, const Vec3& b)     // out = a x b
{
    return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
// normalise only when the length is not already 1 (to FLT_EPSILON); a degenerate
// vector is zeroed and the call answers -20001. The callers here discard the answer.
inline int normalise(Vec3& v)
{
    const float lenSq = lengthSq(v);
    if (fabsf(lenSq - 1.0f) >= 1.1920929e-7f)
    {
        if (fabsf(lenSq) < 1.1920929e-7f)
        {
            v = vec3(0.0f, 0.0f, 0.0f);
            return -20001;
        }
        const float s = 1.0f / sqrtf(lenSq);
        v.x *= s; v.y *= s; v.z *= s;
    }
    return 0;
}
// `v = value; if (value >= high) v = high; if (v < low) return low; return v;`
inline float clampf(float value, float low, float high)
{
    float v = value;
    if (value >= high)
        v = high;
    if (v < low)
        return low;
    return v;
}

// the quality names the stats overlay prints.
const char* qualityName(int quality)
{
    if (quality == 0) return "High";
    if (quality == 1) return "Medium";
    if (quality == 2) return "Low";
    return "Unknown";
}

// Filters obstruction rays: reject soldiers, objects without collision, and the listener's vehicle root.
class SoundObstructionPredicator : public world::ObjectPredicator     // [CHOSEN] name
{
public:
    explicit SoundObstructionPredicator(world::IObject* ignoreRoot) : m_ignoreRoot(ignoreRoot) {}
    virtual bool predicate(world::IObject* object)
    {
        return object->getTemplate()->getClassID() != world::CID_SoldierTemplate
            && (object->getFlags() & world::IObject::OFHasCollision) != 0
            && m_ignoreRoot != world::getRootParent(object);
    }
    virtual unsigned int getFlags() const { return 256; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }
    world::IObject* m_ignoreRoot;                                       // +0x04
};

// Estimates listener reverb from surrounding geometry using three obstruction rays.
float computeListenerReverb(const Vec3& position)                       // [CHOSEN] name
{
    world::ObjectFlagPredicatorEx predicator(0x1000100u, 8u);
    world::IObject* hitObject = 0;
    Vec3 hitPoint  = vec3(0.0f, 0.0f, 0.0f);
    Vec3 hitNormal = vec3(0.0f, 0.0f, 0.0f);
    int  hitPart   = 0;
    const float* rayLength = Vars::getFloat("sound-raylength", 15.0f);

    Vec3 from = vec3(position.x + 0.4f, position.y, position.z + 0.3f);
    Vec3 dir  = vec3(0.0f, *rayLength, 0.0f);
    if (!world::objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, from, dir,
                                             &predicator, false, true, false, true, 0))
        return 0.0f;
    const float ceiling1 = hitPoint.y - position.y;

    from = vec3(position.x - 0.4f, position.y, position.z - 0.3f);
    if (!world::objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, from, dir,
                                             &predicator, false, true, false, true, 0))
        return 0.0f;
    const float ceiling = (hitPoint.y - position.y + ceiling1) * 0.5f;

    dir = vec3(0.0f, -*rayLength, 0.0f);
    if (!world::objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, position, dir,
                                             &predicator, false, true, false, true, 0))
        return 0.0f;
    const float height = position.y - hitPoint.y + ceiling;

    const float maxReverb = clampf(*Vars::getFloat("sound-maxreverb", 0.5f), 0.0f, 1.0f);
    const float steepness = clampf(*Vars::getFloat("sound-reverb-steepness", 0.98f), 0.0f, 1.0f);
    const float h         = clampf(height, 0.0f, *rayLength);
    return maxReverb * h / ((1.0f - steepness) * *rayLength + steepness * h);
}

} // namespace


// BfSound - construction, init and teardown


// Initializes the listener to identity and clears the sound state.
BfSound::BfSound()
    : m_refCount(1),
      m_showStats(false),
      m_showSoundSources(false),
      m_muteCount(0),
      m_savedMasterVolume(0.0f),
      m_windSound(0),
      m_windFactor(0.0f),
      m_windParam(0.0f),
      m_windPlaying(false),
      m_windOn(false),
      m_forceUpdate(false)
{
    m_tinnitus.state   = 0;
    m_tinnitus.attack  = 0.5f;
    m_tinnitus.hold    = 2.0f;
    m_tinnitus.release = 2.0f;
    m_tinnitus.volume  = 0.8f;
    m_listenerMatrix   = Mat4::getIdentity();
    m_listenerVelocity = vec3(0.0f, 0.0f, 0.0f);
    for (int g = 0; g < 8; ++g)
    {
        m_groups[g].volume  = 1.0f;
        m_groups[g].current = 1.0f;
        m_groups[g].target  = 1.0f;
    }
    for (int s = 0; s < 4; ++s)
        m_muteStates[s] = false;
}

// Detaches the engine's file/trace sinks, releases the game sounds (not the menu
// sounds - their vector is only emptied, as shipped), clears the id map, unregisters both event
// handlers, and lets the members go.
BfSound::~BfSound()
{
    g_system->storeAndTurnOffFPUExceptions();
    if (g_soundEngine)
    {
        g_soundEngine->setSoundFileIO(0);
        g_soundEngine->setSoundTrace(0);
    }
    for (unsigned int i = 0; i < m_sounds.size(); ++i)
        if (m_sounds[i])
            m_sounds[i]->release();
    m_menuSounds.clear();                    // std::vector::_Tidy - no release on the entries
    m_soundIds.clear();
    eventManager->unregisterEventHandler(ECCore, this);
    eventManager->unregisterEventHandler(ECSound, this);
    g_system->restoreFPUExceptions();
}

// Registers the debug-draw event and the two handlers, then hands the engine
// this object's file and trace sinks, the profiler client, a zero listener reverb, the
// usage-refcount property and - under opt-late-loading - interactive mode.
void BfSound::init()
{
    g_system->storeAndTurnOffFPUExceptions();
    eventManager->registerEvent(ECSound, 6000, std::string("drawSoundDebugPrimitives"));
    if (!eventManager->registerEventHandler(ECCore, this, 0))
    {
        Debug(DebugTypeError, "Sound/BfSound.cpp", 311, "")
            << "Failed to register BfSound as event handler for ECCore.";
        g_system->restoreFPUExceptions();
        return;
    }
    if (!eventManager->registerEventHandler(ECSound, this, 0))
    {
        Debug(DebugTypeError, "Sound/BfSound.cpp", 317, "")
            << "Failed to register BfSound as event handler for ECSound.";
        g_system->restoreFPUExceptions();
        return;
    }
    engine()->setSoundFileIO(this);
    engine()->setSoundTrace(this);
    engine()->setProfilerClient(profilerClient);
    engine()->setListenerReverb(0.0f);
    engine()->setProperty("soundfile-use-usagerefcount", 1.0f);
    if (*s_optLateLoading)
        engine()->setInteractiveMode(true);
    g_system->restoreFPUExceptions();
}

// The IBase quartet
void  BfSound::addRef()  { ++m_refCount; }
int   BfSound::getRef()  { return m_refCount; }
int   BfSound::release()
{
    const int r = --m_refCount;
    if (r == 0)
        delete this;                                                    // the +0x10 D0 dispatch
    return r;
}
// answers itself for IID_IBase (1) and for 9985003 [UNRESOLVED name - the facade's own
// interface id, not in Core/Ids.h; no recovered caller].
void* BfSound::queryInterface(unsigned int iid) const
{
    if (iid == 1)
        return const_cast<BfSound*>(this);
    return iid == 9985003u ? const_cast<BfSound*>(this) : 0;
}


// The per-frame update


// Builds the listener from the local human player: the camera's transformation
// (getTransformation(0.0f)) and, in first-person view inside a vehicle, the vehicle root's physics
// speed as the listener velocity - which also marks the wind sound active and remembers that root
// (as a WeakPtr) for the obstruction rays to ignore. Everything is then handed to the four-argument
// update. The image reuses the `steps` stack slot for the wind flag (`mov byte ptr [ebp+8], 0/1`)
// and passes the whole dword; with steps below 256 - always, the catch-up clamp is far lower - the
// upper bytes are zero and the flag is the plain bool written here.
void BfSound::update(int steps)
{
    g_system->storeAndTurnOffFPUExceptions();
    if (steps <= 0 && !m_forceUpdate)
    {
        g_system->restoreFPUExceptions();
        return;
    }
    Vec3 velocity = vec3(0.0f, 0.0f, 0.0f);
    Mat4 listener = Mat4::getIdentity();
    bool windActive = false;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player)
    {
        world::ICameraObject* camera = player->getCamera();
        if (camera)
            listener = camera->getTransformation(0.0f);
        if (player->hasFirstPersonView())
        {
            world::IObject* vehicle = player->getVehicle();
            if (vehicle)
            {
                world::IObject* root = world::getRootParent(vehicle);
                world::IPhysicsNode* node = static_cast<world::IPhysicsNode*>(   // the +0x44 cache
                    world::object_detail::LayoutAccess::physicsNode(root));
                if (node)
                {
                    velocity   = node->getPositionalSpeed();
                    windActive = true;
                }
                m_ignoreObject = world::getRootParent(vehicle)->getWeakPtr();
            }
        }
    }
    update(steps, listener, velocity, windActive);
    g_system->restoreFPUExceptions();
}

// Wind, group fades, the listener state (reverb from the environment probe on the
// matrix's translation, position = translation column, orientation = the Z row as facing and the
// Y row as up, velocity), then the engine's update - with one step while a forced update is
// pending, which is cleared once effect group 0's fader has reached its target.
void BfSound::update(int steps, const Mat4& listener, const Vec3& velocity, bool windActive)
{
    g_system->storeAndTurnOffFPUExceptions();
    if (steps > 0 || m_forceUpdate)
    {
        updateWind(windActive);
        updateGroups();
        m_listenerVelocity = velocity;
        m_listenerMatrix   = listener;
        const Vec3* translation = reinterpret_cast<const Vec3*>(&m_listenerMatrix.m[12]);
        ISoundEngine* e = engine();
        e->setListenerReverb(computeListenerReverb(*translation));
        engine()->setListenerPosition(translation);
        engine()->setListenerOrientation(reinterpret_cast<const Vec3*>(&m_listenerMatrix.m[8]),
                                         reinterpret_cast<const Vec3*>(&m_listenerMatrix.m[4]));
        engine()->setListenerVelocity(&m_listenerVelocity);
        engine()->update(m_forceUpdate ? 1 : steps);
        if (m_forceUpdate && fabsf(m_groups[0].target - m_groups[0].current) <= 1.1920929e-7f)
            m_forceUpdate = false;
    }
    g_system->restoreFPUExceptions();
}

// The wind parameter is the listener speed over the setup factor, clamped to
// 0..1 (the sound's volume and pitch evaluators read it). While the listener is in a vehicle the
// wind follows the on/off request; out of one it is stopped.
void BfSound::updateWind(bool active)
{
    if (!m_windSound)
        return;
    m_windParam = clampf(length(m_listenerVelocity) / m_windFactor, 0.0f, 1.0f);
    if (active)
    {
        if (m_windOn != m_windPlaying)
        {
            if (m_windOn)
                m_windSound->play();
            else
                m_windSound->stop();
            m_windPlaying = m_windOn;
        }
    }
    else
    {
        if (m_windPlaying)
            m_windSound->stop();
        m_windPlaying = false;
    }
}

// The mute-state table sets four group targets (0 and 2 - the effect groups -
// go silent under states 1 or 2, to 1/8 under state 3, to 1/4 under state 0; 1 and 3 go silent
// under state 1, to 1/4 under state 0), every fader chases its target by 1/15 per update, and
// the engine gets volume x fader for all eight groups. States 0 and 3 are one-shot: cleared here.
void BfSound::updateGroups()
{
    const bool s0 = m_muteStates[0], s1 = m_muteStates[1], s2 = m_muteStates[2], s3 = m_muteStates[3];
    m_groups[0].target = (s2 || s1) ? 0.0f : s3 ? 0.125f : s0 ? 0.25f : 1.0f;
    m_groups[1].target = s1 ? 0.0f : s0 ? 0.25f : 1.0f;
    m_groups[2].target = (s2 || s1) ? 0.0f : s3 ? 0.125f : s0 ? 0.25f : 1.0f;
    m_groups[3].target = s1 ? 0.0f : s0 ? 0.25f : 1.0f;
    for (int g = 0; g < 8; ++g)
    {
        VolumeGroup& group = m_groups[g];
        const float current = group.current;
        const float target  = group.target;
        if (current <= target)
        {
            if (current < target)
            {
                float next = current + 0.06666667f;
                if (target < next)
                    next = target;
                group.current = next;
            }
        }
        else
        {
            float next = current - 0.06666667f;
            if (target >= next)
                next = target;
            group.current = next;
        }
        engine()->setVolumeForGroup(g, group.volume * group.current);
    }
    m_muteStates[3] = false;
    m_muteStates[0] = false;
}


// Sounds


// User data 0 is the owning object (the obstruction filter reads it), 1 and 2
// are the obstruction evaluator's own caches; 3D sounds get this facade as their evaluator.
ISound* BfSound::createSound(ISoundTemplate* t, world::IObject* object)
{
    g_system->storeAndTurnOffFPUExceptions();
    ISound* s = t->createSound();
    s->setUserData(0, object);
    s->setUserData(1, 0);
    s->setUserData(2, 0);
    if (t->getIs3dSound())
        s->setObstructionEvaluator(this);
    g_system->restoreFPUExceptions();
    return s;
}

void BfSound::loadSoundFiles()
{
    g_system->storeAndTurnOffFPUExceptions();
    engine()->loadSoundFiles();
    g_system->restoreFPUExceptions();
}

ISoundEngine* BfSound::engine() const { return g_soundEngine; }

void BfSound::playSound(int id)
{
    ISound* s = getSound(id);
    if (s)
        s->play();
}
void BfSound::stopSound(int id)
{
    ISound* s = getSound(id);
    if (s)
        s->stop();
}
void BfSound::playSound(const std::string& name) { playSound(getSoundId(name)); }
void BfSound::stopSound(const std::string& name) { stopSound(getSoundId(name)); }

int BfSound::getSoundId(const std::string& name) const
{
    std::map<std::string, int>::const_iterator it = m_soundIds.find(name);
    if (it == m_soundIds.end())
        return -1;
    return it->second;
}

// -1 is "no sound"; ids from 0x1000000 index the menu vector. No bounds check, as shipped.
ISound* BfSound::getSound(int id) const
{
    if (id == -1)
        return 0;
    if (id < 0x1000000)
        return m_sounds[id];
    return m_menuSounds[id - 0x1000000];
}
ISound* BfSound::getSound(const std::string& name) const { return getSound(getSoundId(name)); }

// `Sound.addSound <name>` (GameSounds.con). creates the template "S_Game_<name>"
// of class Sound (createTemplate makes it the active template, so the ObjectTemplate.* lines that
// follow in the script configure it), asks its engine template for a Sound, and files it under the
// bare name - replacing (and releasing) an earlier sound of that name.
void BfSound::addSound(const std::string& name)
{
    world::IObjectTemplate* t =
        world::objectTemplateManager->createTemplate(9986001u, std::string("S_Game_") + name);
    ISoundObjectTemplate* soundTemplate = static_cast<SoundObjectTemplate*>(t);
    ISound* sound = 0;
    if (soundTemplate->impl())
        sound = soundTemplate->impl()->createSound();
    std::map<std::string, int>::iterator it = m_soundIds.find(name);
    if (it == m_soundIds.end())
    {
        m_soundIds[name] = static_cast<int>(m_sounds.size());
        m_sounds.push_back(sound);
    }
    else
    {
        const int id = it->second;
        if (m_sounds[id])
            m_sounds[id]->release();
        m_sounds[id] = sound;
    }
}

// Creates a 2D menu sound in group 3, assigning ID 0x1000000 + index.
// Loop count is zero for looping sounds, otherwise one.
void BfSound::addMenuSound(const std::string& name, const std::string& file, float volume, bool loop)
{
    ISoundTemplate* t = engine()->createTemplate();
    if (!t)
        return;
    setTemplateFilenames(t, file);
    t->setVolume(volume);
    t->setIs3dSound(false);
    t->setLoopCount(loop ? 0u : 1u);
    t->setStopType(0);
    t->setFlags(t->getFlags() | 1u);
    t->setIsUsed();
    t->setVolumeGroup(3);
    ISound* sound = t->createSound();
    t->release();
    std::map<std::string, int>::iterator it = m_soundIds.find(name);
    if (it == m_soundIds.end())
    {
        m_soundIds[name] = static_cast<int>(m_menuSounds.size()) + 0x1000000;
        m_menuSounds.push_back(sound);
    }
    else
    {
        const int index = it->second - 0x1000000;
        if (m_menuSounds[index])
            m_menuSounds[index]->release();
        m_menuSounds[index] = sound;
    }
}

// the quality is clamped to 0..2, the provider name kept, the engine's code returned.
int BfSound::initialize(int quality, const std::string& provider, int voices)
{
    g_system->storeAndTurnOffFPUExceptions();
    int q = quality;
    if (quality < 0)
        q = 0;
    else if (quality > 2)
        q = 2;
    m_providerName = provider;
    const int result = engine()->initialize(static_cast<SoundQuality>(q), m_providerName.c_str(), voices);
    g_system->restoreFPUExceptions();
    return result;
}

int BfSound::qualityLevel() const { return engine()->getQualityLevel(); }

// On the LOW quality level only the first trigger is accepted (numTriggers is
// the engine's +0x6C); the template is marked used and its engine template becomes the trigger.
void BfSound::addTrigger(const std::string& name)
{
    if (engine()->getQualityLevel() != 2 || engine()->numTriggers() <= 0)
    {
        world::IObjectTemplate* t = world::objectTemplateManager->getTemplate(name, 9986001u);
        if (t)
        {
            ISoundObjectTemplate* soundTemplate = static_cast<SoundObjectTemplate*>(t);
            soundTemplate->setIsUsed();
            engine()->addTrigger(soundTemplate->impl());
        }
    }
}

void BfSound::removeTrigger(SoundObjectTemplate* t)
{
    engine()->removeTrigger(static_cast<ISoundObjectTemplate*>(t)->impl());
}

void* BfSound::getTriggerList() { return 0; }

void BfSound::setProperty(const std::string& name, float value)
{
    engine()->setProperty(name.c_str(), value);
}

std::string BfSound::getProperty(const std::string& name) const
{
    const float value = engine()->getProperty(name.c_str());
    std::stringstream out;
    out << value;
    return out.str();
}

void BfSound::setReverb(const std::string& name)
{
    engine()->setReverb(name.c_str());
}

// The comma list is split the way the template filenames are (the image inlines
// its own copy of the parser over a local scratch string), the engine gets the pieces, and the
// four envelope numbers are stored with the state reset to 0.
void BfSound::tinnitusSetup(const std::string& names, float attack, float hold, float release,
                            float volume)
{
    g_system->storeAndTurnOffFPUExceptions();
    if (engine())
    {
        std::vector<const char*> pieces;
        std::string scratch;
        parseTemplateFilenames(&pieces, &scratch, names);
        engine()->setTinnitusSound(pieces.empty() ? 0 : &pieces[0], static_cast<int>(pieces.size()));
        m_tinnitus.attack  = attack;
        m_tinnitus.hold    = hold;
        m_tinnitus.state   = 0;
        m_tinnitus.release = release;
        m_tinnitus.volume  = volume;
    }
    g_system->restoreFPUExceptions();
}

void* BfSound::getTinnitusSetup() const { return const_cast<TinnitusSetup*>(&m_tinnitus); }

void BfSound::startTinnitus()
{
    g_system->storeAndTurnOffFPUExceptions();
    if (engine())
        engine()->startTinnitus(m_tinnitus.attack, m_tinnitus.hold, m_tinnitus.release,
                                m_tinnitus.volume, 0);
    m_tinnitus.state = 1;
    g_system->restoreFPUExceptions();
}
void BfSound::stopTinnitus()
{
    g_system->storeAndTurnOffFPUExceptions();
    if (engine())
        engine()->stopTinnitus(true);
    m_tinnitus.state = 2;
    g_system->restoreFPUExceptions();
}
// The man-down variant holds forever (hold -1) and passes 1 as the engine's mode.
void BfSound::startManDown()
{
    g_system->storeAndTurnOffFPUExceptions();
    if (engine())
        engine()->startTinnitus(m_tinnitus.attack, -1.0f, m_tinnitus.release, m_tinnitus.volume, 1);
    m_tinnitus.state = 1;
    g_system->restoreFPUExceptions();
}
void BfSound::stopManDown()
{
    g_system->storeAndTurnOffFPUExceptions();
    if (engine())
        engine()->stopTinnitus(false);
    m_tinnitus.state = 2;
    g_system->restoreFPUExceptions();
}

// The wind sound's volume and pitch both follow m_windParam through the engine's
// host-evaluator hook (evaluator 0, the float's address).
void BfSound::windSetup(const std::string& name, float factor)
{
    if (factor < 1.0f)
        factor = 1.0f;
    world::IObjectTemplate* t = world::objectTemplateManager->getTemplate(name, 9986001u);
    if (!t)
        return;
    ISoundObjectTemplate* soundTemplate = static_cast<SoundObjectTemplate*>(t);
    soundTemplate->setIsUsed();
    if (m_windSound)
    {
        m_windSound->release();
        m_windSound = 0;
    }
    m_windSound = soundTemplate->impl()->createSound();
    m_windParam = 0.0f;
    m_windSound->setVolumeEvaluator(0, &m_windParam);
    m_windSound->setPitchEvaluator(0, &m_windParam);
    m_windFactor  = factor;
    m_windOn      = false;
    m_windPlaying = false;
}

void BfSound::startWind()                                               // not on Low
{
    if (engine()->getQualityLevel() != 2)
        m_windOn = true;
}
void BfSound::stopWind() { m_windOn = false; }

bool BfSound::isOutdoors(const Vec3& position)
{
    return computeListenerReverb(position) == 0.0f;
}

void BfSound::setShowStats(bool b)               { m_showStats = b; }
bool BfSound::getShowStats() const               { return m_showStats; }
void BfSound::setShowSoundSources(bool b)        { m_showSoundSources = b; }
bool BfSound::getShowSoundSources() const        { return m_showSoundSources; }

// a counted mute: the first mute saves the master volume and zeroes it, the
// matching unmute restores it. The count is decremented even below zero, as shipped.
void BfSound::mute()
{
    if (m_muteCount == 0)
    {
        m_savedMasterVolume = engine()->getMasterVolume();
        engine()->setMasterVolume(0.0f);
    }
    ++m_muteCount;
}
void BfSound::unmute()
{
    if (m_muteCount-- == 1)
        engine()->setMasterVolume(m_savedMasterVolume);
}

void BfSound::setVolumeForGroup(int group, float volume)
{
    if (group >= 0 && group < 8)
        m_groups[group].volume = clampf(volume, 0.0f, 1.0f);
}
float BfSound::getVolumeForGroup(int group) const                       // unsigned test
{
    if (static_cast<unsigned int>(group) >= 8u)
        return 1.0f;
    return m_groups[group].volume;
}

void BfSound::setMuteState(MuteState state, bool b)                     // no range check
{
    m_muteStates[state] = b;
}

// every template of class Sound in the manager's creation-order list, one per line.
std::string BfSound::getAllSoundTemplates() const
{
    std::ostringstream out;
    std::list<world::IObjectTemplate*>* templates = world::objectTemplateManager->getGlobalTemplateList();
    for (std::list<world::IObjectTemplate*>::iterator it = templates->begin(); it != templates->end(); ++it)
    {
        if ((*it)->getClassID() == 9986001u)
            out << (*it)->getName() << '\n';
    }
    return out.str();
}

// `sound.tweakTemplate` (audiotweak.con): six multipliers onto a Sound template.
void BfSound::tweakTemplate(const std::string& name, float volume, float pitch, float minDistance,
                            float halfVolumeDistance, float reverbLevel, float soundRadius)
{
    world::IObjectTemplate* t = world::objectTemplateManager->getTemplate(name, 9986001u);
    if (t)
        static_cast<SoundObjectTemplate*>(t)->tweak(volume, pitch, minDistance, halfVolumeDistance,
                                                    reverbLevel, soundRadius);
}

// an empty name unloads (the engine gets a null).
bool BfSound::loadMusic(const std::string& name)
{
    if (name == "")
        return engine()->loadMusic(0);
    return engine()->loadMusic(name.c_str());
}

void BfSound::setForceUpdate()           { m_forceUpdate = true; }
bool BfSound::getForceUpdate() const     { return m_forceUpdate; }


// ISoundFileIO - the engine reads its files through io::fileManager, so sounds inside the mounted
// archives resolve like every other asset. Handles are io::IStream pointers.

void* BfSound::open(const char* name)
{
    return io::fileManager->open(std::string(name), io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0);
}
int BfSound::close(void* handle)
{
    return static_cast<io::IStream*>(handle)->release();
}
// origins 0/1/2 seek; anything else only reports the position. A null handle answers 0.
// The +0x30/+0x34 slots the body dispatches are the repositionable extension this tree keeps on
// io::INonlinearStream (every file-like stream implements it); the handle is one of those.
int BfSound::seek(void* handle, int offset, int origin)
{
    io::INonlinearStream* stream = static_cast<io::INonlinearStream*>(static_cast<io::IStream*>(handle));
    if (!stream)
        return 0;
    if (origin == 0)
        stream->seek(io::SEEKORIGIN_BEGIN, offset);
    else if (origin == 1)
        stream->seek(io::SEEKORIGIN_CURRENT, offset);
    else if (origin == 2)
        stream->seek(io::SEEKORIGIN_END, offset);
    return stream->getPosition();
}
int BfSound::read(void* handle, void* buffer, int bytes)
{
    return static_cast<int>(static_cast<io::IStream*>(handle)->read(buffer, bytes));
}


// IEventListener - ECSound 6000 (the debug overlays), ECCore 3 CEPostLoad (the sound scripts and
// the engine's file load) and ECCore 9 CECollectSystemInfo (the "- Sound -" rows).

void BfSound::handleEvent(EventCategory category, unsigned int id, EventNode* /*node*/, void* data)
{
    if (category == ECSound)
    {
        if (id == 6000)
        {
            drawStats();
            drawSoundSources();
        }
        else
        {
            Debug(DebugTypeError, "Sound/BfSound.cpp", 993, "") << "Unknown ECSound event: " << id;
        }
        return;
    }
    if (category != ECCore)
        return;
    if (id == 3)                                    // CEPostLoad
    {
        loadGameSounds();
        std::string out;
        io::mainConsole->run(std::string("Common/Sound/AudioTweak.con"), std::string(), std::string(),
                             std::string(), std::string(), std::string(), std::string(),
                             std::string(), std::string(), out);
        int fileCount = 0;
        g_soundEngine->getSoundFiles(&fileCount);
        NextLoadPart part;
        part.steps  = fileCount;
        part.weight = 1.0f;
        part.name   = "Sounds";
        eventManager->postEvent(ECCore, 4, &part, 0.0f);   // CEUpdateLoadStats
        g_soundEngine->loadSoundFiles();
    }
    else if (id == 9)                               // CECollectSystemInfo
    {
        typedef std::vector<std::pair<std::string, std::string> > InfoRows;
        InfoRows& rows = *static_cast<InfoRows*>(data);
        rows.push_back(std::make_pair(std::string("- Sound -"), std::string("")));
        rows.push_back(std::make_pair(std::string("Initialized"),
                                      std::string(engine()->isInitialized() ? "Yes" : "No")));
        if (engine()->isInitialized())
        {
            const SoundStats* stats = static_cast<const SoundStats*>(engine()->getStats());
            rows.push_back(std::make_pair(std::string("Provider"), std::string(stats->providerInfo)));
            rows.push_back(std::make_pair(std::string("MaxVoices"), formatString("%d", stats->maxVoices)));
            rows.push_back(std::make_pair(std::string("Quality"),
                                          formatString("%d", engine()->getQualityLevel())));
        }
    }
}

// the CEPostLoad arm's first half: the loading-screen text, room for 64 game
// sounds, GameSounds.con then ModGameSounds.con (each `Sound.addSound` + its ObjectTemplate lines),
// and every sound's template marked used so the engine loads its files.
void BfSound::loadGameSounds()
{
    eventManager->postEvent(ECHud, 7, const_cast<char*>("Loading Sounds.."), 0.0f);
    m_sounds.reserve(64);
    std::string out;
    io::mainConsole->run(std::string("Common/Sound/GameSounds.con"), std::string(), std::string(),
                         std::string(), std::string(), std::string(), std::string(), std::string(),
                         std::string(), out);
    io::mainConsole->run(std::string("Common/Sound/ModGameSounds.con"), std::string(), std::string(),
                         std::string(), std::string(), std::string(), std::string(), std::string(),
                         std::string(), out);
    for (unsigned int i = 0; i < m_sounds.size(); ++i)
        if (m_sounds[i])
            m_sounds[i]->getTemplate()->setIsUsed();
}

// Draws sound statistics when enabled and the console is closed.
// [FRONTIER] [RETIRE-WHEN: bfsound-debug-draw] Implement DebugTextWriterFixed drawing.
void BfSound::drawStats()
{
    if (!m_showStats || !io::mainConsole || io::mainConsole->getConsoleActive())
        return;
    const SoundStats* stats = static_cast<const SoundStats*>(g_bfSound->engine()->getStats());
    std::vector<std::string> lines;
    lines.reserve(32);
    if (stats)
    {
        lines.push_back(formatString("Provider            : %s", stats->providerInfo));
        lines.push_back(formatString("Quality Level       : %s", qualityName(engine()->getQualityLevel())));
        lines.push_back(formatString("Playing samples     : %u", stats->playing));
        lines.push_back(formatString("Active samples      : %u", stats->waiting));
        lines.push_back(formatString("Inactive samples    : %u", stats->delayed));
        lines.push_back(formatString("Allocated voices    : %u", stats->sources));
        lines.push_back(formatString("Max voices          : %u", stats->maxVoices));
        lines.push_back(formatString("#Static buffers     : %u", stats->alBuffers));
        lines.push_back(formatString("Static buf mem MB   : %f", stats->loadedSampleBytes * 0.00000095367432));
        lines.push_back(formatString("#Streaming buffers  : %u", stats->streamingBuffers));
        lines.push_back(formatString("Streaming sys mem MB: %f", stats->streamedBytes * 0.00000095367432));
        lines.push_back(formatString("Streaming buf mem MB: %f", stats->streamingBufferBytes * 0.00000095367432));
        lines.push_back(formatString("Total buffer mem MB : %f",
                                     (stats->streamingBufferBytes + stats->loadedSampleBytes) * 0.00000095367432));
        lines.push_back(formatString("Loaded samples      : %u", stats->loadedFiles));
        lines.push_back(formatString("#Sounds             : %u", stats->sounds));
        lines.push_back(formatString("#Samples            : %u", stats->samples));
        lines.push_back(formatString("Master volume       : %f", engine()->getMasterVolume()));
        lines.push_back(formatString("Effects volume      : %f", engine()->getVolumeForGroup(0)));
        lines.push_back(formatString("Music volume        : %f", engine()->getVolumeForGroup(1)));
        lines.push_back(formatString("Doppler effect      : %f", engine()->getDopplerEffect()));
        lines.push_back(formatString("Interactive Mode    : %s", engine()->getInteractiveMode() ? "Yes" : "No"));
        const Vec3* listener = engine()->getListenerPosition();
        lines.push_back(formatString("Listener position   : %f/%f/%f", listener->x, listener->y, listener->z));
        lines.push_back(formatString("Listener reverb     : %f", engine()->getListenerReverb()));
        // ("Update              : %f", 0.0) follows behind the [UNRESOLVED] global ds:0x9EB384.
    }
    else
    {
        lines.push_back(std::string("No stats available."));
    }
    // classManager->getSingleton(std::string("DebugTextWriterFixed")) -> slot +0x2C twice.
    std::fprintf(stderr, "[FRONTIER] dice::hfe::BfSound::drawStats reached - the DebugTextWriterFixed "
                         "draw (slot +0x2C) has no interface in this tree; %u lines were built\n",
                 static_cast<unsigned int>(lines.size()));
    std::fflush(stderr);
    std::abort();
}

// [FRONTIER] Draws playing sound sources once the debug-rendering path is implemented.
// [RETIRE-WHEN: bfsound-debug-draw] Complete and validate the corresponding BfSound operation.
void BfSound::drawSoundSources()
{
    if (!m_showSoundSources)
        return;
    int count = 0;
    engine()->queryPlayingSamples(&count);
    IBase* circleDrawer = classManager->getSingleton(std::string("DebugCircleDrawer"));
    IBase* lineDrawer   = classManager->getSingleton(std::string("DebugLineDrawer"));
    IBase* textWriter   = classManager->getSingleton(std::string("DebugTextWriter"));
    IBase* renderer     = classManager->getSingleton(std::string("Renderer"));
    (void) circleDrawer; (void) lineDrawer; (void) textWriter; (void) renderer;
    if (count <= 0)
        return;
    std::fprintf(stderr, "[FRONTIER] dice::hfe::BfSound::drawSoundSources reached with %d samples - "
                         "the debug drawer slots have no interface in this tree\n", count);
    std::fflush(stderr);
    std::abort();
}


// Returns obstruction in [0, 1] for a sound at the supplied position; one means fully obstructed.

float BfSound::evaluate(ISound* sound, const Math::Vec3* position)
{
    if (*s_obstructionDisable)
        return 0.0f;
    Vec3 from[3];
    Vec3 dir[3];
    for (int r = 0; r < 3; ++r)
    {
        from[r] = vec3(0.0f, 0.0f, 0.0f);
        dir[r]  = vec3(0.0f, 0.0f, 0.0f);
    }
    const Vec3* listener = engine()->getListenerPosition();
    dir[0] = *position - *listener;
    float radius = sound->getTemplate()->getRadius();
    if (radius <= 0.0f)
        radius = 0.05f;
    const float distance = sqrtf(lengthSq(dir[0])) - radius - *s_listenerRadius;
    if (distance <= 0.0f)
        return 0.0f;

    if (*s_obstructionOpt1)
    {
        const unsigned int cached = static_cast<unsigned int>(reinterpret_cast<size_t>(sound->getUserData(2)));
        if (cached && (cached >> 16))
        {
            const unsigned int next = (((cached >> 16) - 1) << 16) | (cached & 0xFFFFu);
            sound->setUserData(2, reinterpret_cast<void*>(static_cast<size_t>(next)));
            return static_cast<float>(next & 0xFFFFu) * 1.5259022e-05f;    // 1 / 65535
        }
    }
    sound->setUserData(2, 0);

    Vec3 n = dir[0];
    normalise(n);
    listener = engine()->getListenerPosition();
    from[0] = *listener + n * *s_listenerRadius;
    const float reach = radius + *s_listenerRadius;
    dir[0] = dir[0] - n * reach;
    Vec3 up = (fabsf(n.y) >= 0.99f) ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
    const Vec3 right = cross(up, n);
    up = cross(n, right);
    listener = engine()->getListenerPosition();
    from[1] = *listener + right * *s_listenerRadius;
    dir[1]  = (*position + right * radius) - from[1];
    listener = engine()->getListenerPosition();
    from[2] = *listener + up * *s_listenerRadius;
    dir[2]  = (*position + up * radius) - from[2];

    world::IObject* hitObject = 0;
    Vec3 hitPoint[3];
    for (int r = 0; r < 3; ++r)
        hitPoint[r] = vec3(0.0f, 0.0f, 0.0f);
    Vec3 hitNormal = vec3(0.0f, 0.0f, 0.0f);
    int  hitPart   = 0;
    const unsigned int start =
        *s_obstructionOpt0 ? static_cast<unsigned int>(reinterpret_cast<size_t>(sound->getUserData(1))) : 0u;
    const bool near = distance <= 40.0f;
    const unsigned int rayCount = near ? 3u : 1u;
    SoundObstructionPredicator predicator(m_ignoreObject.get());
    unsigned int i = (rayCount > 1u) ? start : 0u;
    const unsigned int end = start + rayCount;
    unsigned int ray = 0;
    for (; i < end; ++i)
    {
        ray = i % rayCount;
        if (!world::objectManager->intersectLine(hitObject, hitPoint[ray], hitNormal, hitPart,
                                                 from[ray], dir[ray], &predicator,
                                                 near, true, false, true, 0))
        {
            sound->setUserData(1, reinterpret_cast<void*>(static_cast<size_t>(ray)));
            break;
        }
    }

    float obstruction = 0.0f;
    if (i == end)
    {
        world::IObject* owner = static_cast<world::IObject*>(sound->getUserData(0));
        if (!owner || world::getRootParent(owner) != world::getRootParent(hitObject))
        {
            const float blocked   = length(hitPoint[0] - from[0]);
            const float invRadius = 1.0f / *s_obstructionRadius;
            const float remaining = (distance - blocked) * invRadius;
            const float a1 = atan2f(blocked * invRadius, 1.0f);
            const float a2 = atan2f(remaining, 1.0f);
            obstruction = clampf((1.0f - (a1 + a2) * 0.31830987f) * *s_obstructionMax, 0.0f, 1.0f);
        }
    }
    if (*s_obstructionOpt1)
    {
        float d = distance;
        if (d >= 1000.0f)
            d = 1000.0f;
        const unsigned int frames = static_cast<unsigned int>(static_cast<int>(expf(d * 0.01f + 1.0f)));
        unsigned int reuse = frames + static_cast<unsigned int>(*s_obstructionReuseFrames);
        if (reuse > static_cast<unsigned int>(*s_obstructionMaxReuseFrames))
            reuse = static_cast<unsigned int>(*s_obstructionMaxReuseFrames);
        const unsigned int packed =
            (reuse << 16) | (static_cast<unsigned int>(static_cast<int>(obstruction * 65535.0f)) & 0xFFFFu);
        sound->setUserData(2, reinterpret_cast<void*>(static_cast<size_t>(packed)));
    }
    return obstruction;
}
#endif // !DEDICATED


// Console bindings for game-side sound settings and diagnostics.

namespace {

const char* const kBfSoundConsoleTag = "sound.bfsound";     // [CHOSEN] list tag

// with ECX = 9986001: objectTemplateManager->getActiveObjectTemplate(), then
// getTemplate(active, cid) - null unless the active template is a Sound.
inline SoundObjectTemplate* activeSoundTemplate()
{
    return static_cast<SoundObjectTemplate*>(world::getActiveTemplate(9986001u));
}
inline ISoundObjectTemplate* soundTemplate() { return activeSoundTemplate(); }
inline ISoundTemplate*       soundImpl()     { return soundTemplate()->impl(); }

} // namespace

BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 17)
// Sound-object template properties.
BF2_CONSOLE_PROPERTY_FN(soundFilename, std::string, soundTemplate() != 0,
                        soundTemplate()->getFilename(), soundTemplate()->setFilename, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(volumeEnvelope, std::string, soundTemplate() != 0,
                        soundTemplate()->getVolumeEnvelope(), soundTemplate()->setVolumeEnvelope, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(pitchEnvelope, std::string, soundTemplate() != 0,
                        soundTemplate()->getPitchEnvelope(), soundTemplate()->setPitchEnvelope, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(loopCount, int, soundTemplate() != 0,
                        soundTemplate()->getLoopCount(), soundTemplate()->setLoopCount, 0, 100000, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(minDistance, float, soundTemplate() != 0,
                        soundTemplate()->getMinDistance(), soundTemplate()->setMinDistance, 0.01f, 30000.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(halfVolumeDistance, float, soundTemplate() != 0,
                        soundTemplate()->getHalfVolumeDistance(), soundTemplate()->setHalfVolumeDistance, 0.0f, 30000.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(volume, float, soundTemplate() != 0,
                        soundTemplate()->getVolume(), soundTemplate()->setVolume, 0.0f, 1.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(pitch, float, soundTemplate() != 0,
                        soundTemplate()->getPitch(), soundTemplate()->setPitch, 0.2f, 5.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(reverbLevel, float, soundTemplate() != 0,
                        soundTemplate()->getReverbLevel(), soundTemplate()->setReverbLevel, 0.0f, 1.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(soundRadius, float, soundTemplate() != 0,
                        soundTemplate()->getSoundRadius(), soundTemplate()->setSoundRadius, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(lowSamples, int, soundTemplate() != 0,
                        soundTemplate()->getLowSamples(), soundTemplate()->setLowSamples, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(mediumSamples, int, soundTemplate() != 0,
                        soundTemplate()->getMediumSamples(), soundTemplate()->setMediumSamples, kBfSoundConsoleTag)
// Engine-template properties.
BF2_CONSOLE_PROPERTY_FN(is3dSound, bool, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getIs3dSound(), soundImpl()->setIs3dSound, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(stopType, int, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getStopType(), soundImpl()->setStopType, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(pan, float, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getPan(), soundImpl()->setPan, 0.0f, 1.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(position, Vec3, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getPosition(), soundImpl()->setPosition, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(coneDirection, Vec3, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getDirection(), soundImpl()->setDirection, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(coneInnerAngle, float, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getConeInnerAngle(), soundImpl()->setConeInnerAngle, 0.0f, 360.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(coneOuterAngle, float, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getConeOuterAngle(), soundImpl()->setConeOuterAngle, 0.0f, 360.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(coneOuterVolume, float, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getConeOuterVolume(), soundImpl()->setConeOuterVolume, 0.0f, 1.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN_RANGED_T(globalEffectLevel, float, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getGlobalEffectLevel(), soundImpl()->setGlobalEffectLevel, 0.0f, 1.0f, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(volumeGroup, int, soundTemplate() != 0 && soundImpl() != 0,
                        soundImpl()->getVolumeGroup(), soundImpl()->setVolumeGroup, kBfSoundConsoleTag)

#if !DEDICATED
namespace {

inline bool facadeActive() { return g_bfSound != 0; }
inline bool engineActive() { return g_bfSound != 0 && g_bfSound->engine() != 0; }

void consoleRemoveAllTriggers()                              { g_bfSound->engine()->removeAllTriggers(); }
void consoleAddSound(const std::string& name)                { g_bfSound->addSound(name); }
void consolePlaySound(const std::string& name)               { g_bfSound->playSound(name); }
void consoleStopSound(const std::string& name)               { g_bfSound->stopSound(name); }
void consoleAddTrigger(const std::string& name)              { g_bfSound->addTrigger(name); }
void consoleSetReverb(const std::string& name)               { g_bfSound->setReverb(name); }
void consoleSetProperty(const std::string& name, float v)    { g_bfSound->setProperty(name, v); }
void consoleWindSetup(const std::string& name, float factor) { g_bfSound->windSetup(name, factor); }
void consoleSetVolumeForGroup(int group, float volume)       { g_bfSound->setVolumeForGroup(group, volume); }
std::string consoleGetAllSoundTemplates()                    { return g_bfSound->getAllSoundTemplates(); }
void consoleTinnitusSetup(const std::string& names, float attack, float hold, float release, float volume)
{
    g_bfSound->tinnitusSetup(names, attack, hold, release, volume);
}

// The longhand rows: the generated constructor's stores (name, type, arity, return type, argument
// types, conversion flags, list registration) and an executor that runs the call and clears
// m_hasReturnValue. Same base and hook set as the macro-generated classes.
class SoundConsoleBinding : public ConsoleObjectBaseImpl
{
protected:
    SoundConsoleBinding(const char* function, int minArgs, int maxArgs, const char* returnType)
    {
        m_objectName     = consoleDefaultName();
        m_access         = consoleDefaultAccess();
        m_category       = consoleDefaultCategory();
        m_functionName   = function;
        m_type           = 0;
        m_minNoArgs      = minArgs;
        m_maxNoArgs      = maxArgs;
        m_returnType     = returnType;
        m_hasReturnValue = false;
        m_isDynamic      = true;
    }
    void arguments(const char* a0, const char* a1 = 0, const char* a2 = 0, const char* a3 = 0,
                   const char* a4 = 0, const char* a5 = 0, const char* a6 = 0)
    {
        const char* types[7] = { a0, a1, a2, a3, a4, a5, a6 };
        int n = 0;
        for (; n < 7 && types[n]; ++n)
        {
            m_argTypes[n] = types[n];
            m_argNeedsConversion[n] = (std::string(types[n]) == "std::string") ? 0 : -1;
        }
        consoleBindingList(kBfSoundConsoleTag).push_back(this);
    }
public:
    virtual bool isObjectActive() const { return facadeActive(); }
    BF2_CC_NO_RETURN_AS_STRING
    BF2_CC_COMMON_HOOKS
};

// Initializes sound with quality, provider, and voice count; discards the status code.
class SoundInitializeBinding : public SoundConsoleBinding
{
public:
    SoundInitializeBinding() : SoundConsoleBinding("initialize", 3, 3, "SoundInitResult")
    { arguments("int", "std::string", "int"); }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 3) return;
        m_hasReturnValue = false;
        g_bfSound->initialize(BF2_CC_ARG(int, 0), BF2_CC_ARG(std::string, 1), BF2_CC_ARG(int, 2));
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(int, 1)
        BF2_CC_CONVERT(std::string, 2)
        BF2_CC_CONVERT(int, 3)
    }
};

// Adds a menu sound; omitted volume and loop default to 1.0 and false.
class SoundAddMenuSoundBinding : public SoundConsoleBinding
{
public:
    SoundAddMenuSoundBinding() : SoundConsoleBinding("addMenuSound", 2, 4, "void")
    { arguments("std::string", "std::string", "float", "bool"); }
    virtual void executeObjectMethod()
    {
        m_hasReturnValue = false;
        if (m_argCount == 2)
            g_bfSound->addMenuSound(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1), 1.0f, false);
        else if (m_argCount == 3)
            g_bfSound->addMenuSound(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1),
                                    BF2_CC_ARG(float, 2), false);
        else if (m_argCount == 4)
            g_bfSound->addMenuSound(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(std::string, 1),
                                    BF2_CC_ARG(float, 2), BF2_CC_ARG(bool, 3));
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(std::string, 2)
        BF2_CC_CONVERT(float, 3)
        BF2_CC_CONVERT(bool, 4)
    }
};

// Queries a property and discards the returned string.
class SoundGetPropertyBinding : public SoundConsoleBinding
{
public:
    SoundGetPropertyBinding() : SoundConsoleBinding("getProperty", 1, 1, "std::string")
    { arguments("std::string"); }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1) return;
        m_hasReturnValue = false;
        g_bfSound->getProperty(BF2_CC_ARG(std::string, 0));
    }
    virtual void setArgFromString(int index, const std::string& value) { BF2_CC_CONVERT(std::string, 1) }
};

// Applies six named template multipliers.
class SoundTweakTemplateBinding : public SoundConsoleBinding
{
public:
    SoundTweakTemplateBinding() : SoundConsoleBinding("tweakTemplate", 7, 7, "void")
    { arguments("std::string", "float", "float", "float", "float", "float", "float"); }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 7) return;
        m_hasReturnValue = false;
        g_bfSound->tweakTemplate(BF2_CC_ARG(std::string, 0), BF2_CC_ARG(float, 1), BF2_CC_ARG(float, 2),
                                 BF2_CC_ARG(float, 3), BF2_CC_ARG(float, 4), BF2_CC_ARG(float, 5),
                                 BF2_CC_ARG(float, 6));
    }
    virtual void setArgFromString(int index, const std::string& value)
    {
        BF2_CC_CONVERT(std::string, 1)
        BF2_CC_CONVERT(float, 2)
        BF2_CC_CONVERT(float, 3)
        BF2_CC_CONVERT(float, 4)
        BF2_CC_CONVERT(float, 5)
        BF2_CC_CONVERT(float, 6)
        BF2_CC_CONVERT(float, 7)
    }
};

// Loads music and discards the Boolean result.
class SoundLoadMusicBinding : public SoundConsoleBinding
{
public:
    SoundLoadMusicBinding() : SoundConsoleBinding("loadMusic", 1, 1, "bool") { arguments("std::string"); }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1) return;
        m_hasReturnValue = false;
        g_bfSound->loadMusic(BF2_CC_ARG(std::string, 0));
    }
    virtual void setArgFromString(int index, const std::string& value) { BF2_CC_CONVERT(std::string, 1) }
};

// Starts music and discards the Boolean result.
class SoundPlayMusicBinding : public SoundConsoleBinding
{
public:
    SoundPlayMusicBinding() : SoundConsoleBinding("playMusic", 0, 0, "bool")
    { consoleBindingList(kBfSoundConsoleTag).push_back(this); }
    virtual bool isObjectActive() const { return engineActive(); }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 0) return;
        m_hasReturnValue = false;
        g_bfSound->engine()->playMusic();
    }
    virtual void setArgFromString(int, const std::string&) {}
};

// Stops music with a fade duration and discards the Boolean result.
class SoundStopMusicBinding : public SoundConsoleBinding
{
public:
    SoundStopMusicBinding() : SoundConsoleBinding("stopMusic", 1, 1, "bool") { arguments("float"); }
    virtual bool isObjectActive() const { return engineActive(); }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1) return;
        m_hasReturnValue = false;
        g_bfSound->engine()->stopMusic(BF2_CC_ARG(float, 0));
    }
    virtual void setArgFromString(int index, const std::string& value) { BF2_CC_CONVERT(float, 1) }
};

// Queries group volume and discards the returned value.
class SoundGetVolumeForGroupBinding : public SoundConsoleBinding
{
public:
    SoundGetVolumeForGroupBinding() : SoundConsoleBinding("getVolumeForGroup", 1, 1, "float")
    { arguments("int"); }
    virtual void executeObjectMethod()
    {
        if (m_argCount != 1) return;
        m_hasReturnValue = false;
        g_bfSound->getVolumeForGroup(BF2_CC_ARG(int, 0));
    }
    virtual void setArgFromString(int index, const std::string& value) { BF2_CC_CONVERT(int, 1) }
};

}
// Register engine properties before facade properties.
BF2_CONSOLE_OBJECT("Sound", 2, 17)
BF2_CONSOLE_PROPERTY_FN(masterVolume, float, engineActive(),
                        g_bfSound->engine()->getMasterVolume(), g_bfSound->engine()->setMasterVolume, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(listenerReverb, float, engineActive(),
                        g_bfSound->engine()->getListenerReverb(), g_bfSound->engine()->setListenerReverb, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dopplerEffect, float, engineActive(),
                        g_bfSound->engine()->getDopplerEffect(), g_bfSound->engine()->setDopplerEffect, kBfSoundConsoleTag)
BF2_CONSOLE_METHOD_0_ACTIVE(removeAllTriggers, consoleRemoveAllTriggers, engineActive(), kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(interactiveMode, bool, engineActive(),
                        g_bfSound->engine()->getInteractiveMode(), g_bfSound->engine()->setInteractiveMode, kBfSoundConsoleTag)
namespace { SoundInitializeBinding s_soundInitializeBinding; }
BF2_CONSOLE_METHOD_1_ACTIVE(addSound, consoleAddSound, facadeActive(), std::string, kBfSoundConsoleTag)
namespace { SoundAddMenuSoundBinding s_soundAddMenuSoundBinding; }
BF2_CONSOLE_METHOD_1_ACTIVE(playSound, consolePlaySound, facadeActive(), std::string, kBfSoundConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(stopSound, consoleStopSound, facadeActive(), std::string, kBfSoundConsoleTag)
BF2_CONSOLE_METHOD_5(tinnitusSetup, consoleTinnitusSetup, facadeActive(), std::string, float, float, float, float, kBfSoundConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(windSetup, consoleWindSetup, facadeActive(), std::string, float, kBfSoundConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(addTrigger, consoleAddTrigger, facadeActive(), std::string, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(showStats, bool, facadeActive(),
                        g_bfSound->getShowStats(), g_bfSound->setShowStats, kBfSoundConsoleTag)
BF2_CONSOLE_PROPERTY_FN(showSoundSources, bool, facadeActive(),
                        g_bfSound->getShowSoundSources(), g_bfSound->setShowSoundSources, kBfSoundConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(setProperty, consoleSetProperty, facadeActive(), std::string, float, kBfSoundConsoleTag)
namespace { SoundGetPropertyBinding s_soundGetPropertyBinding; }
BF2_CONSOLE_METHOD_1_ACTIVE(setReverb, consoleSetReverb, facadeActive(), std::string, kBfSoundConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(getAllSoundTemplates, consoleGetAllSoundTemplates, std::string, facadeActive(), kBfSoundConsoleTag)
namespace { SoundTweakTemplateBinding s_soundTweakTemplateBinding; }
namespace { SoundLoadMusicBinding s_soundLoadMusicBinding; }
namespace { SoundPlayMusicBinding s_soundPlayMusicBinding; }
namespace { SoundStopMusicBinding s_soundStopMusicBinding; }
BF2_CONSOLE_METHOD_2_ACTIVE(setVolumeForGroup, consoleSetVolumeForGroup, facadeActive(), int, float, kBfSoundConsoleTag)
namespace { SoundGetVolumeForGroupBinding s_soundGetVolumeForGroupBinding; }
#endif // !DEDICATED

ConsoleObjectList& bfSoundConsoleObjects() { return consoleBindingList(kBfSoundConsoleTag); }

} // namespace hfe
} // namespace dice
