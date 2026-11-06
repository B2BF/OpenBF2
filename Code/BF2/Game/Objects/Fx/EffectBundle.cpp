// Path: attested by BF2.exe 0x642DFA: Game/Objects/Fx/EffectBundle.cpp.
// Runtime recovered from complete client PE 0x641360..0x643632 and symbolized ELF

#include "EffectBundle.h"
#include "EffectBundleTemplate.h"
#include "ParticleSystemTemplates.h"
#include "Emitter.h"                                  // init's LOD cap - Emitter::m_lodDistance
#include "../TemplateLeaves.h"
#include "../ObjectLayoutAccess.h"
#include "../GeometryPartAccess.h"
#include "../IPlayer.h"
#include "../Game/EngineBase.h"
#include "../../../Core/ClassManager.h"
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../../../Geom/ParticleSystem/ParticleSystemManager.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Sound/NullSound.h"
#include "../../Main/Game.h"
#include "FxTrace.h"                                  // [TEMP] BF2_TRACE_FX - the timed-bundle path
#include <cmath>
#include <cfloat>
#include <cstring>

namespace dice { namespace hfe {
extern ClassManager* classManager;
namespace world {
bool g_effectsDisabled = false;
extern IBase* heightmapCluster;

bool EffectBundleTemplate::physicsUsed() const
{
    return m_byte338 || effectPropertiesUsed(); // short-circuit retained
}

#if !DEDICATED
namespace {
typedef object_detail::LayoutAccess L;
IPhysicsNode* physics(IObject* object)
{
    return static_cast<IPhysicsNode*>(L::physicsNode(object));
}
IHeightmapCluster* heights() { return static_cast<IHeightmapCluster*>(heightmapCluster); }
geom::IParticleSystemManager* particleManager()
{
    return static_cast<geom::IParticleSystemManager*>(classManager->getSingleton("ParticleSystemManager"));
}
float length(const Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}
void normalize(Vec3& value)
{
    const float squared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(squared - 1.0f) >= FLT_EPSILON)
    {
        if (std::fabs(squared) < FLT_EPSILON)
            value = Vec3{0, 0, 0};
        else
        {
            const float inverse = 1.0f / std::sqrt(squared);
            value.x *= inverse; value.y *= inverse; value.z *= inverse;
        }
    }
}
float unitRandom()
{
    // sign draw / Random helpers: signed shift after setting bit 7.
    return static_cast<float>((static_cast<int>(Rand.getRaw() | 0x80u) >> 7) + 1)
        * 0.000000059604645f;
}
float sample(const RandomDistribution& distribution)
{
    // > 0x5E7450. The relative-sign draw occurs before the distribution draw.
    const float sign = distribution.relative && unitRandom() <= 0.5f ? -1.0f : 1.0f;
    float result;
    switch (distribution.distribution)
    {
    case CRD_NONE: result = distribution.value; break;
    case CRD_UNIFORM:
        result = distribution.value + (distribution.variation - distribution.value) * unitRandom();
        break;
    case CRD_EXPONENTIAL: result = -distribution.value * std::log(unitRandom()); break;
    case CRD_NORMAL: result = distribution.value + Rand.getNormal() * distribution.variation; break;
    default: result = 0.0f; break;
    }
    return result * sign;
}
float geometryFov(void* geometry)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    typedef float (__thiscall* Function)(void*);
#else
    typedef float (*Function)(void*);
#endif
    // BF2.exe 0x64160E / ELF 0x81FA5D0: float-returning geometry slot +14/+18.
    return geometry_part::slot<Function>(geometry, 0x14, 0x18)(geometry);
}
}

IObject* EffectBundleTemplate::createObject() const // ; dedicated retains its null body
{
    if (isDedicated())
        return 0;
    if ((getFlags() & 0x20) == 0)
        preCacheGeoms();
    return new EffectBundle(this);
}
void EffectBundleTemplate::preCacheGeoms() const // ; ELF is empty
{
    if (!isDedicated())
        for (std::vector<TemplateInfo*>::const_iterator i = m_templateInfos.begin(); i != m_templateInfos.end(); ++i)
        {
            IObjectTemplate* child = objectTemplateManager->getTemplate((*i)->m_templateName);
            if (child && child->getClassID() == CID_MeshParticleSystemTemplate)
                child->preCache();
        }
}
void EffectBundleTemplate::addBundleInstances(EffectBundle* bundle) const
{
    geom::IParticleSystemManager* manager = particleManager();
    if (!isDedicated() || manager)
    {
        const float ageScale = sample(m_dist2F4);
        for (std::vector<TemplateInfo*>::const_iterator i = m_templateInfos.begin(); i != m_templateInfos.end(); ++i)
        {
            IObjectTemplate* child = objectTemplateManager->getTemplate((*i)->m_templateName);
            if (child)
            {
                const unsigned int cid = child->getClassID();
                if (cid == CID_SpriteParticleSystemTemplate || cid == CID_MeshParticleSystemTemplate ||
                    cid == CID_TrailSystemTemplate || cid == CID_ParticleSystemEmitterTemplate ||
                    cid == CID_NonScreenAlignedParticleSystemTemplate)
                {
                    ParticleSystemTemplateBase* system = static_cast<ParticleSystemTemplateBase*>(child);
                    geom::IParticleSystemInstance* instance = manager->createInstance(system->m_impl);
                    // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h
                    if (!instance)
                        BF2_FX_TRACE("addBundleInstances: %s child %s (cid %u) impl=%d -> createInstance NULL",
                                     getName().c_str(), (*i)->m_templateName.c_str(), cid,
                                     system->m_impl ? 1 : 0);
                    if (instance)
                    {
                        unsigned char viewMask = system->m_byte40 ? 1 : 0;
                        if (system->m_byte41) viewMask |= 2;
                        instance->reserved14(&(*i)->m_transformation);
                        bundle->addParticleSystemInstance(instance, viewMask);
                        instance->reserved30(instance->reserved31() * ageScale);
                        instance->setEffectController(bundle ? static_cast<IEffectController*>(bundle) : 0);
                    }
                }
            }
            else
                Debug(DebugTypeError, "Game/Objects/Fx/EffectBundle.cpp", 1274, "")
                    << getName() << " addBundleChilds() template not found:" << (*i)->m_templateName;
        }
    }
}

EffectBundle::EffectBundle(const EffectBundleTemplate* objectTemplate)
    : Bundle(objectTemplate), m_startOnNextUpdate(false), m_keepUpdating(false),
      m_effectsPlaying(false), m_numEmittersPlaying(0), m_timeToLive(sample(objectTemplate->m_dist2E0)),
      m_startSound(0), m_paused(false), m_speed{0, 0, 0}, m_hasSpeed(false),
      m_maxAcceleration(0), m_overrideTimeToLive(false), m_soundIntensity(0),
      m_fovOverride(false), m_firstPerson(false)
{
    if (m_timeToLive > 0.0f || objectTemplate->m_field2D8 >= 0.0f || objectTemplate->physicsUsed())
    {
        Object::updateFlags(0x800, 0);
        m_keepUpdating = true;
    }
    else
        Object::updateFlags(1, 0);
    m_startSound = static_cast<ISound*>(objectTemplate->createSoundFromTemplate("Start", this));
    if (m_startSound)
    {
        m_startSound->setParent(this);
        m_startSound->setVolumeEvaluator(0, &m_soundIntensity);
        m_startSound->setPitchEvaluator(0, &m_soundIntensity);
    }
}
EffectBundle::~EffectBundle()
{
    removeInstances();
    if (m_startSound)
    {
        m_startSound->setVolumeEvaluator(0, 0);
        m_startSound->setPitchEvaluator(0, 0);
        m_startSound->release();
    }
}
void EffectBundle::enableCachedObject()
{
    Bundle::enableCachedObject();
    m_numEmittersPlaying = 0;
    m_effectsPlaying = false;
    const EffectBundleTemplate* t = static_cast<const EffectBundleTemplate*>(L::objectTemplate(this));
    if (t->m_byte314)
    {
        updateFlags(0, 1);
        updateFlags(0x800, 0);
        m_startOnNextUpdate = true;
    }
    else m_startOnNextUpdate = false;
}
void EffectBundle::disableCachedObject()
{
    stopEffects();
    SimpleObject::disableCachedObject();
}
void EffectBundle::init()
{
    Bundle::init();
    const EffectBundleTemplate* t = static_cast<const EffectBundleTemplate*>(L::objectTemplate(this));
    if (!m_overrideTimeToLive && m_timeToLive < 0.0f && !getParent())
    {
        m_timeToLive = 0.5f;
        updateFlags(0, 1);
        updateFlags(0x800, 0);
        m_keepUpdating = true;
    }
    // BF2.exe 0x6413B0 / ELF 0x81F9C00: the cap is a raw store into the emitter child (PE +0x178 /
    // ELF +0x160) - typed since the Emitter was published.
    for (IObject* child = getChild(); child; child = child->getSibling())
        if (L::objectTemplate(child)->getClassID() == CID_EmitterTemplate &&
            static_cast<const EmitterTemplate*>(L::objectTemplate(child))->m_lodDistance > t->m_field308)
            static_cast<Emitter*>(child)->m_lodDistance = t->m_field308;
    if (t->m_byte314)
    {
        updateFlags(0, 1);
        updateFlags(0x800, 0);
        m_startOnNextUpdate = true;
    }
    else m_startOnNextUpdate = false;
}
void EffectBundle::addNumEmittersPlaying()
{
    if (!m_keepUpdating)
    {
        if (m_numEmittersPlaying == 0)
            updateFlags(0, 1);
        ++m_numEmittersPlaying;
        IObject* parent = getParent();
        if (parent && L::objectTemplate(parent)->getClassID() == CID_EffectBundleTemplate)
            static_cast<EffectBundle*>(parent)->addNumEmittersPlaying();
    }
}
void EffectBundle::decNumEmittersPlaying()
{
    if (!m_keepUpdating)
    {
        if (--m_numEmittersPlaying <= 0)
        {
            m_numEmittersPlaying = 0;
            updateFlags(1, 0);
        }
        IObject* parent = getParent();
        if (parent && L::objectTemplate(parent)->getClassID() == CID_EffectBundleTemplate)
            static_cast<EffectBundle*>(parent)->decNumEmittersPlaying();
    }
}
IObject* EffectBundle::getObject() const { return const_cast<EffectBundle*>(this); }
const Mat4& EffectBundle::getTransform() { return getAbsoluteTransformation(); }
const Math::Vec3* EffectBundle::getVelocity() { return 0; } // genuine ELF 0x81FBA10 null
void EffectBundle::overRideTimeToLive(bool enabled) { m_overrideTimeToLive = enabled; }
void EffectBundle::addParticleSystemInstance(geom::IParticleSystemInstance* instance, unsigned char viewMask)
{
    m_instances.push_back(instance);
    m_viewMasks.push_back(viewMask);
}
void EffectBundle::removeParticleSystemInstance(geom::IParticleSystemInstance* instance)
{
    std::vector<unsigned char>::iterator mask = m_viewMasks.begin();
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i, ++mask)
        if (*i == instance)
        {
            m_instances.erase(i);
            m_viewMasks.erase(mask);
            return;
        }
}
void EffectBundle::removeInstances()
{
    geom::IParticleSystemManager* manager = particleManager();
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i)
        manager->removeInstance(*i);
    m_instances.resize(0);
    m_viewMasks.resize(0);
}
bool EffectBundle::getIsFirstPerson(bool* fovOverride)
{
    *fovOverride = false;
    IPlayer* player = playerManager->getLocalHumanPlayer();
    if (player && player->hasFirstPersonView())
    {
        IPlayerControlObject* vehicle = findOwningPlayerControlObject(player->getVehicle());
        if (vehicle && !vehicle->getNeverDrawAs1p())
        {
            IPlayerControlObject* owner = findOwningPlayerControlObject(this);
            if (owner && !owner->getNeverDrawAs1p() &&
                getRootParent(player->getVehicle()) == getRootParent(this))
            {
                if (getParent() && L::geometry(getParent()))
                    *fovOverride = geometryFov(L::geometry(getParent())) != -1.0f;
                m_fovOverride = *fovOverride;
                m_firstPerson = true;
                return true;
            }
        }
    }
    m_fovOverride = false;
    m_firstPerson = false;
    return false;
}
void EffectBundle::updateFirstPerson(bool forceThirdPerson)
{
    bool fovOverride = false, firstPerson = false;
    if (!forceThirdPerson)
        firstPerson = getIsFirstPerson(&fovOverride);
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i)
    {
        (*i)->setFovOverride(fovOverride && firstPerson);
        (*i)->reserved42(firstPerson);
    }
}
void EffectBundle::startEffects()
{
    const EffectBundleTemplate* t = static_cast<const EffectBundleTemplate*>(L::objectTemplate(this));
    if (m_timeToLive > 0.0f)
    {
        Mat4 camera = Mat4::getIdentity();
        float farDistance;
        objectManager->getCameraInfo(&camera, &farDistance, 0);
        const Vec3& position = getAbsolutePosition();
        const float x = position.x - camera.m[12], y = position.y - camera.m[13], z = position.z - camera.m[14];
        // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h; timed bundles only
        BF2_FX_TRACE("startEffects: %s ttl=%.2f pos=(%.1f %.1f %.1f) camera=(%.1f %.1f %.1f) "
                     "dist=%.1f far=%.1f -> %s", t->getName().c_str(), m_timeToLive,
                     position.x, position.y, position.z, camera.m[12], camera.m[13], camera.m[14],
                     std::sqrt(x*x + y*y + z*z), farDistance,
                     x*x + y*y + z*z > farDistance * farDistance ? "REFUSED (beyond far)" : "go");
        if (x*x + y*y + z*z > farDistance * farDistance)
            return;
    }
    if (m_instances.empty())
        t->addBundleInstances(this);
    updateEffectProperties();
    for (IObject* child = getChild(); child; child = child->getSibling())
    {
        const int id = static_cast<signed char>(child->getChildId());
        if (id >= 0 && id < static_cast<int>(t->m_templateInfos.size()))
            child->handleMessage(static_cast<TemplateMessage>(0), 0, false);
    }
    if (m_startSound) m_startSound->play();
    bool fovOverride = false;
    const bool firstPerson = getIsFirstPerson(&fovOverride);
    int startedInstances = 0;   // [TEMP] counted for the trace below only
    std::vector<unsigned char>::iterator mask = m_viewMasks.begin();
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i, ++mask)
        if ((firstPerson && (*mask & 1)) || (!firstPerson && (*mask & 2)))
        {
            (*i)->setFovOverride(fovOverride && firstPerson);
            (*i)->reserved03(firstPerson);
            ++startedInstances;
        }
    // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h; timed bundles only
    if (m_timeToLive > 0.0f)
        BF2_FX_TRACE("startEffects: %s children=%d instances=%d started=%d firstPerson=%d sound=%d",
                     t->getName().c_str(), static_cast<int>(t->m_templateInfos.size()),
                     static_cast<int>(m_instances.size()), startedInstances, firstPerson ? 1 : 0,
                     m_startSound ? 1 : 0);
}
void EffectBundle::stopEffects()
{
    const EffectBundleTemplate* t = static_cast<const EffectBundleTemplate*>(L::objectTemplate(this));
    for (IObject* child = getChild(); child; child = child->getSibling())
    {
        const int id = static_cast<signed char>(child->getChildId());
        if (id >= 0 && id < static_cast<int>(t->m_templateInfos.size()))
            child->handleMessage(static_cast<TemplateMessage>(1), 0, false);
    }
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i)
        (*i)->reserved04();
    if (m_startSound) m_startSound->stop();
    m_soundIntensity = 0;
    removeInstances();
}
void EffectBundle::togglePause()
{
    m_paused = !m_paused;
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i)
        (*i)->setPaused(m_paused);
}
#endif // !DEDICATED
// Also present in dedicated PE 5F9470..5F95B0 / 5F9710 and ELF 81FA310..81FA4A0 /
// 81FAA00. WheelEffectBundle uses these shared instance forwarders.
#define EFFECT_SETTER(name, slot, type) \
void EffectBundle::name(type value) { \
    for (std::vector<geom::IParticleSystemInstance*>::iterator i=m_instances.begin(); i!=m_instances.end(); ++i) \
        (*i)->slot(value); }
EFFECT_SETTER(setAlphaIntensityForAllEffects, reserved22, float)
EFFECT_SETTER(setEmitScaleFactorForAllEffects, reserved24, float)
EFFECT_SETTER(setEmitSpeedScaleFactorForAllEffects, reserved26, float)
EFFECT_SETTER(setEmitDirectionScaleFactorForAllEffects, reserved28, const Vec3&)
EFFECT_SETTER(setAgeScaleFactorForAllEffects, reserved30, float)
EFFECT_SETTER(setStartAgeForAllEffects, reserved32, float)
#undef EFFECT_SETTER
void EffectBundle::setCollisionPlane(const Plane3& value)
{
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i)
        (*i)->reserved18(&value);
}
#if !DEDICATED
void EffectBundle::setPlayerForGasCloudType(IPlayer* player, int material, int gasCloudType)
{
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i)
        if ((*i)->getTemplate()->getGasCloudType() == gasCloudType)
        {
            (*i)->setPlayer(player);
            (*i)->setMaterial(material);
        }
}
void EffectBundle::setSpeedForAllEffects()
{
    Vec3 speed = {0, 0, 0};
    if (m_hasSpeed) speed = m_speed;
    else
    {
        IObject* root = getRootParent(this);
        if (root && root != this && physics(root))
            speed = physics(root)->getTangentSpeed(getAbsolutePosition());
    }
    const float magnitude = std::sqrt(length(speed)); // shipped fourth root, not ordinary speed
    normalize(speed);
    for (std::vector<geom::IParticleSystemInstance*>::iterator i = m_instances.begin(); i != m_instances.end(); ++i)
    {
        (*i)->reserved19(magnitude);
        (*i)->reserved16(speed);
    }
}
void EffectBundle::handleMessage(TemplateMessage message, IPlayer*, bool)
{
    // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h; timed bundles only
    if (m_timeToLive > 0.0f)
        BF2_FX_TRACE("bundle message: %s msg=%d effectsDisabled=%d startOn=%u stopOn=%u "
                     "minDepth=%.2f playing=%d",
                     L::objectTemplate(this)->getName().c_str(), static_cast<int>(message),
                     g_effectsDisabled ? 1 : 0,
                     static_cast<const EffectBundleTemplate*>(L::objectTemplate(this))->m_field330,
                     static_cast<const EffectBundleTemplate*>(L::objectTemplate(this))->m_field334,
                     static_cast<const EffectBundleTemplate*>(L::objectTemplate(this))->m_field2D8,
                     m_effectsPlaying ? 1 : 0);
    if (g_effectsDisabled) return;
    const EffectBundleTemplate* t = static_cast<const EffectBundleTemplate*>(L::objectTemplate(this));
    const unsigned int starts = t->m_field330, stops = t->m_field334;
    bool start = false, stop = false;
    unsigned int mask = 0;
    switch (static_cast<int>(message))
    {
    case 0: start = starts == 0; break;
    case 1: stop = stops == 0; break;
    case 2:
        if (starts & 8) start = true; else if (stops & 8) stop = true;
        updateFirstPerson(false); break;
    case 3:
        if (starts & 16) start = true; else if (stops & 16) stop = true;
        updateFirstPerson(true); break;
    case 4:
        if (getObjectCacheID()) updateFlags(0, 1);
        mask = 32; break;
    case 5: mask = 64; break;
    case 6:
        if (starts & 1) start = true;
        else if (stops & 1) stop = false; // deliberate shipped quirk
        break;
    case 28: updateFirstPerson(false); break;
    case 42: mask = 128; break;
    case 43: mask = 256; break;
    case 64: start = true; break;
    case 65: stop = true; break;
    case 68: case 69: break;
    default:
        if (static_cast<int>(message) >= 44 && static_cast<int>(message) <= 53)
            mask = 1u << (static_cast<int>(message) - 35);
        break;
    }
    if (mask)
    {
        if (starts & mask) start = true;
        else if (stops & mask) stop = true;
    }
    if (t->m_byte339)
    {
        const float water = heights()->getWaterLevelInWorldCoords(getAbsolutePosition());
        const float depth = water - getAbsolutePosition().y;
        float threshold;
        std::memcpy(&threshold, &t->m_field33C, sizeof(threshold));
        if ((depth > threshold && !t->m_byte33A) || (depth <= threshold && t->m_byte33A))
            start = false;
    }
    if (t->m_field2D8 < 0.0f)
    {
        if (start) startEffects();
        else if (stop) stopEffects();
    }
    else if (start)
    {
        if (!m_effectsPlaying) { m_effectsPlaying = true; startEffects(); }
    }
    else if (stop && m_effectsPlaying)
    {
        m_effectsPlaying = false;
        stopEffects();
    }
}
void EffectBundle::handleUpdate(float deltaTime, unsigned int flags)
{
    const EffectBundleTemplate* t = static_cast<const EffectBundleTemplate*>(L::objectTemplate(this));
    if (flags & 4) return;
    if (m_startOnNextUpdate)
    {
        handleMessage(static_cast<TemplateMessage>(0), 0, false);
        m_startOnNextUpdate = false;
    }
    if (!m_keepUpdating)
    {
        if (!t->m_byte314) updateFlags(1, 0);
        updateFlags(0, 0x800);
    }
    if (m_timeToLive > 0) m_timeToLive -= deltaTime;
    if (getObjectCacheID())
    {
        m_timeToLive = -1;
        if (m_numEmittersPlaying == 0)
        {
            IObject* root = getRootParent(this);
            IPhysicsNode* node = physics(root);
            IPlayerControlObject* control = findOwningPlayerControlObject(root);
            if (control && !control->isOccupied() && node->getIsSleeping())
                updateFlags(1, 0);
        }
    }
    else if (m_timeToLive != -1.0f && m_timeToLive < 0)
    {
        // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h
        BF2_FX_TRACE("bundle expired: %s instances=%d -> stopEffects + destroy", t->getName().c_str(),
                     static_cast<int>(m_instances.size()));
        if (m_startSound) m_startSound->stop();
        stopEffects();
        destroy(true);
        return;
    }
    if (t->m_field2D8 >= 0)
    {
        bool outside = getParent() && L::objectTemplate(getParent()) &&
            L::objectTemplate(getParent())->getClassID() == CID_SoldierTemplate && getParent()->getParent();
        if (!outside)
        {
            const float water = heights()->getWaterLevelInWorldCoords(getAbsolutePosition());
            const float depth = water - getAbsolutePosition().y;
            outside = depth < 0 || depth < t->m_field2D8 || depth > t->m_field2DC;
        }
        if (outside)
        {
            if (m_effectsPlaying) { m_effectsPlaying = false; stopEffects(); }
        }
        else if (!m_effectsPlaying)
        {
            m_effectsPlaying = true;
            (void)getChild(); // shipped unused virtual query
            startEffects();
        }
    }
    if (t->m_byte338) setSpeedForAllEffects();
    updateEffectProperties();
}
void EffectBundle::updateEffectProperties()
{
    EffectBundleTemplate* t = const_cast<EffectBundleTemplate*>(static_cast<const EffectBundleTemplate*>(L::objectTemplate(this)));
    if (!t->effectPropertiesUsed())
    {
        for (unsigned int i = 0; i != 6; ++i)
            if (t->getPropertyUsed(static_cast<EffectProperty>(i)))
            {
                const float value = t->getPropertyRange(static_cast<EffectProperty>(i)).x;
                switch (i)
                {
                case 0: setAlphaIntensityForAllEffects(value); break;
                case 1: setEmitScaleFactorForAllEffects(value); break;
                case 2: setEmitSpeedScaleFactorForAllEffects(value); break;
                case 3: setAgeScaleFactorForAllEffects(value); break;
                case 4: setStartAgeForAllEffects(value); break;
                case 5: m_soundIntensity = value >= 0 ? (value <= 1 ? value : 1) : 0; break;
                }
            }
        return;
    }
    Vec3 speed = Vec3::getZero(), acceleration = Vec3::getZero(), rotational = Vec3::getZero();
    (void)Vec3::getZero(); (void)Vec3::getZero();
    if (m_hasSpeed) speed = m_speed;
    else
    {
        IObject* root = getRootParent(this);
        if (root && root != this && physics(root))
        {
            speed = physics(root)->getTangentSpeed(getAbsolutePosition());
            acceleration = physics(root)->getTangentAcceleration(getAbsolutePosition());
            const Vec3& positional = physics(root)->getPositionalSpeed();
            rotational = Vec3{speed.x - positional.x, speed.y - positional.y, speed.z - positional.z};
        }
    }
    const Mat4& transform = getAbsoluteTransformation();
    float forward = speed.x*transform.m[8] + speed.y*transform.m[9] + speed.z*transform.m[10];
    float speedMagnitude = -100000, accelerationMagnitude = -100000, rotationMagnitude = -100000;
    const float accelerationLength = length(acceleration);
    m_maxAcceleration = accelerationLength > m_maxAcceleration ? accelerationLength : m_maxAcceleration;
    float altitude = getAbsolutePosition().y;
    if (t->m_field2D8 >= 0)
    {
        altitude = heights()->getWaterLevelInWorldCoords(getAbsolutePosition()) - altitude;
        if (altitude < 0) altitude = 0;
    }
    else if (heightmapCluster)
        altitude -= heights()->getHeightInWorldCoords(getAbsolutePosition());
    float engineIntensity = 0;
    if (getParent() && L::objectTemplate(getParent())->getClassID() == CID_EngineTemplate)
    {
        EngineBase* engine = static_cast<EngineBase*>(getParent());
        if (engine->m_engineRunning) engineIntensity = engine->getEngineIntensity();
    }
    for (unsigned int i = 0; i != 6; ++i)
    {
        float factors[6] = {1,1,1,1,1,1};
        bool used = false;
        const int order[6] = {0,3,4,2,1,5};
        for (int index = 0; index != 6; ++index)
        {
            const int a = order[index];
            const Vec2& stored = t->m_property[i][a];
            if (stored.x == 0 && stored.y == 0) continue;
            float value;
            switch (a)
            {
            case 0:
                if (speedMagnitude < -1000) speedMagnitude = length(speed);
                value = speedMagnitude; break;
            case 3: value = altitude; break;
            case 4: value = engineIntensity; break;
            case 2:
                if (accelerationMagnitude < -1000) accelerationMagnitude = accelerationLength;
                value = accelerationMagnitude; break;
            case 1:
                if (rotationMagnitude < -1000) rotationMagnitude = length(rotational);
                value = rotationMagnitude; break;
            default:
                if (forward < 0) forward = 0;
                value = forward; break;
            }
            factors[a] = t->updateVar(value, t->getProperty(static_cast<EffectProperty>(i), static_cast<EffectAffector>(a)));
            used = true;
        }
        // PE multiplication order is right-to-left across the six factors.
        const float value = t->m_range[i].x + (t->m_range[i].y-t->m_range[i].x)
            * factors[5]*factors[1]*factors[2]*factors[4]*factors[3]*factors[0];
        if (used)
            switch (i)
            {
            case 0: setAlphaIntensityForAllEffects(value); break;
            case 1: setEmitScaleFactorForAllEffects(value); break;
            case 2: setEmitSpeedScaleFactorForAllEffects(value); break;
            case 3: setAgeScaleFactorForAllEffects(value); break;
            case 4: setStartAgeForAllEffects(value); break;
            case 5: m_soundIntensity = value >= 0 ? (value <= 1 ? value : 1) : 0; break;
            }
    }
}
#endif // !DEDICATED
} } }
