// Path: attested by __FILE__; recovered from the symbolized ELF and Win32 PE.
// Projectile launch, update, collision, TTL and cleanup. Layout is in GenericProjectile.h.
// Collision, detonation, penetration, ricochet and seeking use component interfaces.
// Preserve authoritative state, prediction and launch provenance across their calls.
// Specialized client/effect branches retain named [FRONTIER] boundaries.

#include "GenericProjectile.h"

#include <cfloat>                                   // FLT_EPSILON
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "GenericProjectileTemplate.h"
#include "GrenadeTrace.h"
#include "../../GameEvents/MissileInitEvent.h"
#include "../../IGame.h"
#include "../../IGameLogic.h"                       // gameLogic - launch authority/list gate
#include "../../SingletonServer.h"
#include "../ObjectLayoutAccess.h"
#include "../IObjectTemplate.h"
#include "../ICameraObject.h"
#include "../IPlayer.h"                              // getPlayer / the projectileOwner seam
#include "../IWeaponObject.h"
#include "../Networkables/Networkables.h"
#include "../../../AI/AIInterface/BFKnowledge/BFCollisionHandler.h"
#include "../../../Core/Ids.h"
#include "../../../Core/Debug.h"                     // getStickTransformation's retail error arm
#include "../../../Core/Random.h"                    // g_random - retail distribution sampler
#include "../../../Core/System.h"                    // launch clocks / ticks
#include "../../../Core/WeakPtr.h"                   // the +0x174 ctrl read - PCO's idiom
#include "../../../Geom/IGeometryTemplate.h"
#include "../../../IO/Console/IConsoleSaveable.h"    // the queryInterface +0x80 answer
#include "../../../IO/Network/INetworkable.h"
#include "../../../Physics/IPhysicsNode.h"           // underwater forwarding
#include "../../../Core/StringUtil.h"                // compareIgnoreCase - the tracer name test
#include "../../../Physics/PointPhysicsNode.h"       // getPositionalSpeed - the ISoundNode face
#include "../../../Physics/MaterialManager.h"        // the ctor's lazy material resolve
#include "../../../Physics/Material.h"               // handleCollision reads getHasWaterPhysics
#include "../Fx/EffectBundleTemplate.h"              // activate samples the trail template
#include "../../../Physics/PhysicsUtil.h"            // reset/disabled/reenable launch sequence
#include "../../../Scene/Object/ObjectManager.h"     // objectManager - resetProjectile's weak targets
#include "../../../Scene/Object/ObjectUtil.h"        // world::setNetworkableComp
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../Fx/FxTrace.h"                  // [TEMP] BF2_TRACE_FX - detonate's three trace lines

namespace dice {
namespace hfe {
namespace ai {
// Stored as an erased pointer; IBFCollisionHandler does not derive from IBase.
extern IBase* g_bfCollisionHandler;
}
namespace world {

extern IBase* materialManager;      // ds:0x8AD2820, defined in Core/Globals.cpp

namespace {

// IScaleable consists of the IBase quartet followed by setScale. Both activate and updateTracer
// query exactly this IID before making the typed call at +0x10.
class IScaleable : public IBase
{
public:
    virtual void setScale(const Vec3&) = 0;
};

float unitRandom(Random& random)
{
    const unsigned int raw = ((random.getRaw() >> 7) | 1u) + 1u;
    return static_cast<float>(static_cast<int>(raw)) * 0.000000059604645f;
}

// activate and the trail-template arm both use the process-global generator, not the Random
// embedded in the distribution. Relative is a second independent draw that chooses the sign.
float sampleRandomDistribution(const RandomDistribution& distribution)
{
    float sample;
    switch (distribution.distribution)
    {
    case CRD_NONE:
        sample = distribution.value;
        break;
    case CRD_UNIFORM:
        sample = distribution.value
            + (distribution.variation - distribution.value) * unitRandom(g_random);
        break;
    case CRD_EXPONENTIAL:
        sample = -distribution.value * static_cast<float>(std::log(unitRandom(g_random)));
        break;
    case CRD_NORMAL:
        sample = distribution.value + g_random.getNormal() * distribution.variation;
        break;
    default:
        sample = 0.0f;
        break;
    }

    if (distribution.relative && unitRandom(g_random) <= 0.5f)
        sample = -sample;
    return sample;
}

// The geometry-template getter moves by one slot across ABIs because the Itanium geometry face has
// one more destructor entry. ELF updateTracer 0x82817D2 calls +0x3C; PE 0x5B1941 calls +0x38.
IObjectTemplate* getGeometryTemplate(IBase* geometry)
{
#if defined(_MSC_VER)
    // Retail offsets describe four-byte entries; native MSVC x64 keeps the PE
    // ordinal, including its single destructor slot, with eight-byte entries.
    enum { GetTemplateIndex = 0x38 / 4 };
    typedef IObjectTemplate* (__thiscall *GetTemplateFn)(void*);
#else
    enum { GetTemplateIndex = 0x3C / 4 };
    typedef IObjectTemplate* (*GetTemplateFn)(void*);
#endif
    void** const vtable = *reinterpret_cast<void***>(geometry);
    GetTemplateFn const getTemplate = reinterpret_cast<GetTemplateFn>(vtable[GetTemplateIndex]);
    return getTemplate(geometry);
}

IPlayerControlObject* findPlayerControlObject(IObject* object)
{
    for (; object; object = object->getParent())
    {
        IPlayerControlObject* const pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
        if (pco)
            return pco;
    }
    return 0;
}

// THE object'S physics node at +0x44 - the same recovered field used by
// DefaultPenetrateComp, DefaultDetonationComp, PreDestinedFollowComp and
// GenericProjectile::reduceVelocityUnderwater. Keeping the access typed here closes the
// associated-tracer and authored-acceleration arms without an ABI-specific vtable seam.
IPhysicsNode* objectPhysicsNode(IObject* object)
{
    return static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(object));
}

// The renormalise-with-epsilon-guard this tree uses everywhere.
void normaliseVec3(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (fabsf(lengthSq - 1.0f) < FLT_EPSILON)
        return;
    if (fabsf(lengthSq) < FLT_EPSILON)
    {
        v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
        return;
    }
    const float inverse = 1.0f / sqrtf(lengthSq);
    v.x *= inverse; v.y *= inverse; v.z *= inverse;
}

// Reports and stops unresolved projectile operations.
void projectileFrontier(const char* method)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::world::GenericProjectile::%s reached\n", method);
    std::fflush(stderr);
    std::abort();
}

} // namespace

// Initializes projectile bases and state, retaining the original uninitialized gaps.
// Lazily fills the template's material cache. Point-family physics receives otherCollisionLod,
// with -1 mapped to 2 here.
GenericProjectile::GenericProjectile(const GenericProjectileTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    typedef object_detail::LayoutAccess L;

    m_refCount = 1;                                  // +0x148
    m_template = objectTemplate;                     // +0x14C
    // m_assistPlayers (+0x16C) default-constructs its 12-byte self-linked sentinel
    // m_launcherWeapon / m_player / m_launcherVehicle default-construct null at +0x170..+0x178.
    m_team = 0;                                      // +0x17C
    m_isActive = false;                              // +0x180
    m_byte181 = false;                               // +0x181
    m_dword198 = -1;                                 // +0x198
    m_vec19C.x = 0.0f; m_vec19C.y = 0.0f; m_vec19C.z = 0.0f;
    m_startPos.x = 0.0f; m_startPos.y = 0.0f; m_startPos.z = 0.0f;
    m_dword1B4 = 0;
    m_dword1B8 = 0;
    m_dword1BC = 0;
    m_damage = objectTemplate->m_projectileDamage;           // the 10.0f minDamage fallback - friend read
    m_dword1C4 = 0;                                  // +0x1C4
    m_tick1C8 = -1000;                               // +0x1C8
    m_vector1CC.x = 0.0f; m_vector1CC.y = 0.0f; m_vector1CC.z = 0.0f;
    m_vec1D8.x = 0.0f; m_vec1D8.y = 0.0f; m_vec1D8.z = 0.0f;
    m_targetObjectWeak = 0;                          // +0x1E4
    m_projectileCamera = 0;                          // +0x1E8
    m_dword1EC = -1;                                 // +0x1EC
    m_associatedTracerWeak = 0;                      // +0x1F0
    m_localPredictedWeak = 0;                        // +0x1F4
    m_interpolationProxyWeak = 0;                    // +0x1F8
    m_byte1FC = false;
    m_byte1FD = false;
    m_dword200 = 0;
    m_countHit = false;
    m_localPredict = false;
    m_byte206 = true;
    m_byte20C = false;
    m_vec210.x = 0.0f; m_vec210.y = 0.0f; m_vec210.z = 0.0f;
    m_trailEffectObject = 0;
    m_geometrySwap224 = 0;
    m_geometrySwap228 = 0;
    m_vec22C.x = 0.0f; m_vec22C.y = 0.0f; m_vec22C.z = 0.0f;

    m_collisionComp = 0;                             // +0x150..+0x168, the seven
    m_detonationComp = 0;
    m_seekComp = 0;
    m_followComp = 0;
    m_ricochetComp = 0;
    m_penetrateComp = 0;
    m_soundComp = 0;

    // the template's lazy material cache (friend writes through the const pointer, as shipped)
    GenericProjectileTemplate* tpl = const_cast<GenericProjectileTemplate*>(m_template);
    if (!tpl->m_field198)
        tpl->m_field198 = static_cast<MaterialManager*>(materialManager)
                              ->getMaterialPtr(static_cast<unsigned int>(tpl->m_material));

    // the point-physics collision LOD
    const int physicsType = tpl->m_physicsType;
    if (physicsType == 3 || physicsType == 4)
    {
        PointPhysicsNode* const node = static_cast<PointPhysicsNode*>(L::physicsNode(this));
        if (node)
        {
            int lod = tpl->m_otherCollisionLod;
            if (lod == -1)
                lod = 2;
            node->setOtherCollisionLod(lod);
        }
    }
}

// D2/D1 - 0x827FE70/0x827FFF0, 381 B: revector all three vptrs, drop seven weak blocks in reverse
// field order (+0x1F8, +0x1F4, +0x1F0, +0x1E4, +0x178, +0x174, +0x170), clear the assist list,
// chain ~IProjectileObject (compiler-emitted) and ~Bundle. The four erased late blocks are dropped
// explicitly; the three typed launcher/owner WeakPtrs and list clean themselves up in source order.
namespace {
void dropWeakBlock(void* block)
{
    if (!block)
        return;
    int* refCount = static_cast<int*>(block);
    if ((*refCount)-- == 1)
        ::operator delete(block);
}

// IProjectileObject::getWeakPtr - its non-virtual 74-byte producer is not declared on the
// deliberately narrow interface header yet. This is the same {owner ref, returned ref} operation
// as IObject::getWeakPtr and IPlayer::getWeakPtr, applied to the public control-block field.
WeakPtr<IProjectileObject> getProjectileWeakPtr(IProjectileObject* projectile)
{
    if (!projectile->m_weakControl)
    {
        projectile->m_weakControl = static_cast<WeakPtr<IProjectileObject>::Ctrl*>(
            ::operator new(sizeof(WeakPtr<IProjectileObject>::Ctrl)));
        projectile->m_weakControl->refCount = 1;
        projectile->m_weakControl->target = projectile;
    }

    WeakPtr<IProjectileObject> result;
    result.m_ctrl = projectile->m_weakControl;
    ++projectile->m_weakControl->refCount;
    return result;
}

void assignProjectileWeakBlock(void*& destination,
                               const WeakPtr<IProjectileObject>& incoming)
{
    WeakPtr<IProjectileObject>::Ctrl* block = incoming.m_ctrl;
    if (block)
        ++block->refCount;
    dropWeakBlock(destination);
    destination = block;
}

void assignObjectWeakBlock(void*& destination, IObject* object)
{
    WeakPtr<IObject> incoming = object ? object->getWeakPtr() : WeakPtr<IObject>();
    WeakPtr<IObject>::Ctrl* block = incoming.m_ctrl;
    if (block)
        ++block->refCount;
    dropWeakBlock(destination);
    destination = block;
}

// The tracer cache stores IGeometryTemplate behind an unrelated IObjectTemplate declaration.
IBase* createTracerGeometry(IObjectTemplate* erasedTemplate)
{
    return static_cast<IBase*>(
        reinterpret_cast<geom::IGeometryTemplate*>(erasedTemplate)->createInstance());
}
} // namespace

GenericProjectile::~GenericProjectile()
{
    dropWeakBlock(m_interpolationProxyWeak);
    dropWeakBlock(m_localPredictedWeak);
    dropWeakBlock(m_associatedTracerWeak);
    dropWeakBlock(m_targetObjectWeak);
    // The three typed WeakPtr members at +0x170..+0x178 drop automatically, in reverse order,
    // after the destructor body; the assist-player list follows them, matching the retail order.
}

void  GenericProjectile::addRef() { ++m_refCount; }
int   GenericProjectile::getRef() { return m_refCount; }
int   GenericProjectile::release()
{
    const bool last = (m_refCount == 1);
    const int  r    = --m_refCount;
    if (last)
        delete this;
    return r;
}

// four answers, in the ELF's check order.
void* GenericProjectile::queryInterface(unsigned int iid) const
{
    GenericProjectile* self = const_cast<GenericProjectile*>(this);
    if (iid == IID_IBase || iid == world::IID_IObject)
        return static_cast<IObject*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    if (iid == world::IID_IProjectileObject)
        return static_cast<IProjectileObject*>(self);
    return 0;
}

// Initializes base/components, clears object flags, stamps network state and resolves tracer caches.
// Preserve both tracer getter calls: they populate client caches even though the return values are discarded.
// Optionally finds the projectile-camera child after determining explosion type.
void GenericProjectile::init()
{
    typedef object_detail::LayoutAccess L;

    Bundle::init();
    updateFlags(0, 0xC00);
    updateFlagsRecursive(0, 0xC00);
    if (getNetworkable())
        m_byte181 = true;
    updateGeometry();
    const_cast<GenericProjectileTemplate*>(m_template)->getTracerTemplate();      // client: resolves the cache
    const_cast<GenericProjectileTemplate*>(m_template)->getWaterTracerTemplate(); // client: resolves the cache
    initComponents();
    determineExplosionType();

    if (m_template->m_useProjectileCamera)                       // the projectile-camera gate - friend read
    {
        for (IObject* child = const_cast<IObject*>(getChild()); child;
             child = L::sibling(child))
        {
            if (L::objectTemplate(child)->getClassID() == CID_CameraTemplate)
            {
                m_projectileCamera = static_cast<ICameraObject*>(
                    child->queryInterface(world::IID_ICameraObject));
                break;
            }
        }
    }
}

// the seven queryComponent hops, then IProjectileSoundComp::initSoundComp on
// the projectile's SimpleObjectTemplate. The derived interface lets C++ preserve PE +0x20 versus
// ELF +0x24 without a raw vtable index.
void GenericProjectile::initComponents()
{
    m_collisionComp = static_cast<IProjectileCollisionComp*>(
        queryComponent(IID_IProjectileCollisionComp, IID_IProjectileCollisionComp));
    m_detonationComp = static_cast<IProjectileDetonationComp*>(
        queryComponent(IID_IProjectileDetonationComp, IID_IProjectileDetonationComp));
    m_seekComp = static_cast<IProjectileSeekComp*>(
        queryComponent(IID_IProjectileSeekComp, IID_IProjectileSeekComp));
    m_followComp = static_cast<IProjectileFollowComp*>(
        queryComponent(IID_IProjectileFollowComp, IID_IProjectileFollowComp));
    m_ricochetComp = static_cast<IProjectileRicochetComp*>(
        queryComponent(IID_IProjectileRicochetComp, IID_IProjectileRicochetComp));
    m_penetrateComp = static_cast<IProjectilePenetrateComp*>(
        queryComponent(IID_IProjectilePenetrateComp, IID_IProjectilePenetrateComp));
    m_soundComp = static_cast<IProjectileSoundComp*>(
        queryComponent(IID_IProjectileSoundComp, IID_IProjectileSoundComp));
    if (m_soundComp)
        m_soundComp->initSoundComp(m_template);
}

// gate byte, then the standard networkable attach off the template's info name.
void GenericProjectile::initNetworkableComp()
{
    if (m_localPredict)
        return;
    if (!m_template)
        return;
    if (m_template->getNetworkableInfo().length())
        setNetworkableComp(this, CID_GenericProjectileNetworkable);
}

// destroy - 0x8282D70, 138 B: resetProjectile(!force) first, then release the two geometry-swap
// slots when they hold something other than the live geometry, then SimpleObject::destroy.
void GenericProjectile::destroy(bool force)
{
    typedef object_detail::LayoutAccess L;

    if (grenade_trace::selected(this))
    {
        grenade_trace::log(this, "destroy", "force=%d", force ? 1 : 0);
        grenade_trace::retire(this);
    }
    resetProjectile(!force);
    if (m_geometrySwap224 && static_cast<void*>(m_geometrySwap224) != L::geometry(this))
    {
        m_geometrySwap224->release();
        m_geometrySwap224 = 0;
    }
    if (m_geometrySwap228 && static_cast<void*>(m_geometrySwap228) != L::geometry(this))
    {
        m_geometrySwap228->release();
        m_geometrySwap228 = 0;
    }
    SimpleObject::destroy(force);
}

// disableCachedObject - 0x8280400, 274 B - THE warm-UP park. resetProjectile(false) first (the
// inactive path for a never-activated carcass), the SimpleObject chain, then the template's two
// tracer caches lazy-resolve (name -> template, only while unset and the name is non-empty), a
// tracer-capable round drops its geometry, and the seven comp pointers are zeroed.
void GenericProjectile::disableCachedObject()
{
    resetProjectile(false);
    SimpleObject::disableCachedObject();

    // the +0x208/+0x20C -> +0x210/+0x214 swap-template lazy pair - friend writes, as shipped
    GenericProjectileTemplate* tpl = const_cast<GenericProjectileTemplate*>(m_template);
    if (!tpl->m_field210 && tpl->m_string208.length())
        tpl->m_field210 = world::objectTemplateManager->getTemplate(tpl->m_string208);
    if (!tpl->m_field214 && tpl->m_string20C.length())
        tpl->m_field214 = world::objectTemplateManager->getTemplate(tpl->m_string20C);

    // The gate is hasTracer (ELF primary +0x244; PE IProjectileObject face +0xD0). It is false on a
    // dedicated server, where tracer templates are never resolved.
    if (hasTracer())
        setComponent(geom::IID_IGeometry, 0);

    m_collisionComp = 0;
    m_detonationComp = 0;
    m_seekComp = 0;
    m_followComp = 0;
    m_ricochetComp = 0;
    m_penetrateComp = 0;
    m_soundComp = 0;
}

// clear the networkable-attach gate, the Bundle chain, re-query the components.
void GenericProjectile::enableCachedObject()
{
    m_localPredict = false;
    Bundle::enableCachedObject();
    initComponents();
}

// resetProjectile - ELF 0x8283240, 735 B. The two prediction targets are destroyed before the
// active test. The active teardown then restores state, resets components, unregisters the live
// projectile, tears down trail/camera/tracer state, updates the touch manager and stops children,
// in the shipped order. The no-bot build keeps the AI collision-handler pointer null; its otherwise
// untyped non-null callback remains a branch-local stop rather than an ABI guess.
void GenericProjectile::resetProjectile(bool a2)
{
    if (m_localPredictedWeak)
    {
        IObject* const target =
            static_cast<WeakPtr<IObject>::Ctrl*>(m_localPredictedWeak)->target;
        if (target)
            objectManager->destroyObject(target);
    }
    if (m_interpolationProxyWeak)
    {
        IObject* const target =
            static_cast<WeakPtr<IObject>::Ctrl*>(m_interpolationProxyWeak)->target;
        if (target)
            objectManager->destroyObject(target);
    }

    if (!m_isActive)
        return;

    m_byte181 = true;
    m_tick1C8 = -1000;
    m_isActive = false;
    m_byte1FC = false;
    m_dword1C4 = 0;

    dropWeakBlock(m_targetObjectWeak);
    m_targetObjectWeak = 0;

    // The collision component's resetProjectile slot, not preResetProjectile: StickyCollisionComp
    // re-enables its parked physics here.
    if (m_collisionComp)
        m_collisionComp->resetProjectile(a2);
    if (m_soundComp)
        m_soundComp->reset();

    if (m_template->m_addToProjectileList)
        gameLogic->removeProjectile(this);

    // . Drop this projectile from the AI's collision tracking before the trail goes.
    if (ai::g_bfCollisionHandler)
        reinterpret_cast<ai::IBFCollisionHandler*>(ai::g_bfCollisionHandler)
            ->handleProjectileTimeout(static_cast<IProjectileObject*>(this), this);

    destroyTrailEffect();

    IObject* const launcherWeapon = getLauncherWeapon();
    if (launcherWeapon)
    {
        IWeaponObject* const weapon = static_cast<IWeaponObject*>(
            launcherWeapon->queryInterface(IID_IWeaponObject));
        if (weapon)
            weapon->projectileWasReset(this);
    }

    disableProjectileCamera();

    IProjectileObject* const tracerFace = m_associatedTracerWeak
        ? static_cast<WeakPtr<IProjectileObject>::Ctrl*>(m_associatedTracerWeak)->target
        : 0;
    if (tracerFace)
    {
        // GenericProjectile is the sole IProjectileObject implementor in both shipped images.
        // The ELF calls resetProjectile through the +0x13C subobject and then adjusts that same
        // pointer back to the primary IObject before GameServer::destroyObject.
        GenericProjectile* const tracer = static_cast<GenericProjectile*>(tracerFace);
        tracer->resetProjectile(false);
        // ..0x8283419 / PE 0x5B31C1..0x5B31E4 re-resolve after reset.
        IProjectileObject* const currentTracer = m_associatedTracerWeak
            ? static_cast<WeakPtr<IProjectileObject>::Ctrl*>(m_associatedTracerWeak)->target
            : 0;
        game->destroyObject(static_cast<IObject*>(static_cast<GenericProjectile*>(currentTracer)));
        dropWeakBlock(m_associatedTracerWeak);
        m_associatedTracerWeak = 0;
    }

    if (m_template->m_byte1FA && SingletonServer::m_iNametagManager)
        SingletonServer::m_iNametagManager->removeItem(2, this);

    m_byte206 = true;
    stopEffects();
}

// . This recovers the authoritative componentless and component-bearing spine:
// stopped-projectile state, position/trail snapshots, component update order, seek/follow
// convergence, associated-tracer convergence and authored acceleration.
void GenericProjectile::handleUpdate(float dt, unsigned int flags)
{
    if ((flags & 4u) || m_byte181)
        return;

    float timeUntilCanDetonate = 0.0f;
    if (m_detonationComp)
        timeUntilCanDetonate = m_detonationComp->getTimeUntilCanDetonate();

    if (getDetonateAfterProjectileStopped() && m_byte20C
        && timeUntilCanDetonate <= 0.0f)
    {
        const Vec3& position = getAbsolutePosition();
        const float dx = position.x - m_vec210.x;
        const float dy = position.y - m_vec210.y;
        const float dz = position.z - m_vec210.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (getDetonateAfterProjectileStoppedMinSpeed() * dt > distanceSquared
            && !getDetonate())
        {
            updateFlags(0, 0x800u);
            m_byte20C = false;
            detonate(false);
        }
    }

    m_vec210 = getAbsolutePosition();
    if (m_trailEffectObject)
        m_trailEffectObject->setAbsolutePosition(getAbsolutePosition());

    if (gameLogic->isServer() || m_template->getNetworkableInfo().empty())
    {
        IObject* target = 0;
        int seekTickOffset = 0;

        // THE seek component picks THE target AND THE follow component chases IT, AND they ARE
        // wired together right here - not through each other. The seek comp updates first, and the
        // target it names plus the tracking delay it reports are then pushed into the follow comp
        // and, further down, into the detonation comp. A projectile with a follow comp but no seek
        // comp therefore chases a null target with a zero delay, which is exactly how a
        // PreDestinedFollowComp round behaves.
        if (m_seekComp)
        {
            m_seekComp->updateComponent(dt, flags);
            target = m_seekComp->getTargetObject();
            seekTickOffset = m_seekComp->getTrackingDelayTick();
        }

        // order IS load-bearing: the delay tick goes in before the target. setTargetObject is a
        // 159-byte body in both implementers and must see the delay already set.
        // The delay is made absolute here - launch tick plus the seek comp's tracking delay - so
        // the follow comp receives a tick to start steering at, not a duration.
        if (m_followComp)
        {
            m_followComp->setDelayTick(
                static_cast<unsigned int>(m_dword1BC + seekTickOffset));
            m_followComp->setTargetObject(target);
            m_followComp->updateComponent(dt, flags);
        }

        if (m_collisionComp)
            m_collisionComp->updateComponent(dt, flags);

        if (m_dword1BC + m_template->m_armingDelayTicks
            <= static_cast<int>(WorldPref::getTickCount()))
        {
            if (m_detonationComp)
            {
                m_detonationComp->setTargetObject(target);
                m_detonationComp->updateComponent(dt, flags);
            }
        }
        (void)seekTickOffset;
    }

    if (m_localPredict && m_collisionComp)
        m_collisionComp->updateComponent(dt, flags);

    IProjectileObject* const tracerFace = m_associatedTracerWeak
        ? static_cast<WeakPtr<IProjectileObject>::Ctrl*>(m_associatedTracerWeak)->target
        : 0;
    if (tracerFace)
    {
        // THE tracer follows ITS round, AND ONE weapon IN THE game IS special-cased BY name.
        // The ELF compares this projectile's template name against the literal
        // "AAV_TYPE95guns_Projectile" and gives that one - and only that one - the convergence
        // behaviour; every other projectile just copies its velocity onto the tracer.
        // The ELF queries IID_IObject off the stored face; this tree reaches the same object by
        // the downcast handleCollision already uses on the identical pointer a few lines above.
        IObject* const tracerObject = static_cast<GenericProjectile*>(tracerFace);
        IPhysicsNode* const tracerNode = objectPhysicsNode(tracerObject);
        IPhysicsNode* const ourNode = objectPhysicsNode(this);

        const Vec3& here = getAbsolutePosition();
        const float travelled = sqrtf(
            (m_startPos.x - here.x) * (m_startPos.x - here.x) +
            (m_startPos.y - here.y) * (m_startPos.y - here.y) +
            (m_startPos.z - here.z) * (m_startPos.z - here.z));

        const std::string special("AAV_TYPE95guns_Projectile");
        if (compareIgnoreCase(getTemplate()->getName(), special) != 0)
        {
            // The ordinary path: hand the tracer our current velocity.
            if (ourNode)
                m_vec22C = ourNode->getPositionalSpeed();
            if (tracerNode)
                tracerNode->setPositionalSpeed(m_vec22C);
        }
        else if (travelled > m_template->m_tracerConvergeDistance)
        {
            // Past the convergence distance the tracer is simply snapped onto us.
            tracerObject->setAbsolutePosition(getAbsolutePosition());
            if (ourNode && tracerNode)
                tracerNode->setPositionalSpeed(ourNode->getPositionalSpeed());
        }
        else
        {
            // gated ON A function-local static in the shipped body - `converge` - which nothing
            // recovered ever writes, so as shipped this arm never runs. Reproduced as a named
            // constant so the dead branch stays visible rather than being silently dropped.
            static const bool converge = false;
            if (converge && tracerNode)
            {
                // Lerp the tracer toward us, weighted by how far we still have to travel.
                float blend = travelled / m_template->m_tracerConvergeDistance;
                if (blend < 0.0f)
                    blend = 0.0f;
                else if (blend > 1.0f)
                    blend = 1.0f;
                const float remaining = 1.0f - blend;

                const Vec3& tracerPos = tracerObject->getAbsolutePosition();
                const Vec3& ourPos = getAbsolutePosition();
                Vec3 target;
                target.x = ourPos.x + remaining * (tracerPos.x - m_startPos.x);
                target.y = ourPos.y + remaining * (tracerPos.y - m_startPos.y);
                target.z = ourPos.z + remaining * (tracerPos.z - m_startPos.z);
                tracerObject->setAbsolutePosition(target);
            }
        }

        // The tracer then faces its own velocity. Rows are right = (0,1,0) x direction and
        // up = direction x right - neither renormalised, as shipped - then the direction and the
        // tracer's current translation. A direction within 0.0001 of vertical takes a fixed basis.
        if (tracerNode)
        {
            Vec3 direction = tracerNode->getPositionalSpeed();
            const float speedSquared = direction.y * direction.y + direction.x * direction.x
                                     + direction.z * direction.z;
            if (speedSquared > 0.025f)
            {
                normaliseVec3(direction);

                Vec3 right;
                Vec3 up;
                if (fabsf(direction.x) < 0.000099999997f && fabsf(direction.z) < 0.000099999997f)
                {
                    right.x = 1.0f; right.y = 0.0f; right.z = 0.0f;
                    up.x = 0.0f;    up.y = 0.0f;
                    direction.x = 0.0f;
                    direction.z = 0.0f;
                    if (direction.y <= 0.0f)
                    {
                        direction.y = -1.0f;
                        up.z = 1.0f;
                    }
                    else
                    {
                        direction.y = 1.0f;
                        up.z = -1.0f;
                    }
                }
                else
                {
                    right.x = direction.z;
                    right.y = 0.0f;
                    right.z = -direction.x;
                    up.x = direction.y * right.z - direction.z * right.y;
                    up.y = direction.z * right.x - direction.x * right.z;
                    up.z = direction.x * right.y - direction.y * right.x;
                }

                const Mat4& current = tracerObject->getAbsoluteTransformation();
                Mat4 frame = Mat4::getIdentity();
                frame.m[0] = right.x;     frame.m[1] = right.y;     frame.m[2] = right.z;
                frame.m[4] = up.x;        frame.m[5] = up.y;        frame.m[6] = up.z;
                frame.m[8] = direction.x; frame.m[9] = direction.y; frame.m[10] = direction.z;
                frame.m[12] = current.m[12];
                frame.m[13] = current.m[13];
                frame.m[14] = current.m[14];
                tracerObject->setAbsoluteTransformation(frame);
            }
        }
    }

    // authored acceleration with A speed CAP. A template with a non-zero m_field1EC pushes the
    // projectile along its own facing every tick - but only while the component of its velocity
    // along that facing is still under m_field1F0, which GenericProjectileTemplate.h already
    // records as `maxSpeed`. That is what stops an accelerating rocket running away.
    // It sets the acceleration, not the speed - IPhysicsNode +0x78, not +0x70.
    if (m_template->m_field1EC > 0.0f)
    {
        IPhysicsNode* const node = objectPhysicsNode(this);
        if (node)
        {
            const Mat4& transform = getAbsoluteTransformation();
            Vec3 forward;
            forward.x = transform.m[8];
            forward.y = transform.m[9];
            forward.z = transform.m[10];

            Vec3 velocity = node->getPositionalSpeed();
            const float speed = sqrtf(velocity.x * velocity.x +
                                      velocity.y * velocity.y +
                                      velocity.z * velocity.z);
            normaliseVec3(velocity);

            const float along = speed * (velocity.x * forward.x +
                                         velocity.y * forward.y +
                                         velocity.z * forward.z);
            if (m_template->m_field1F0 >= along)
            {
                Vec3 acceleration;
                acceleration.x = forward.x * m_template->m_field1EC;
                acceleration.y = forward.y * m_template->m_field1EC;
                acceleration.z = forward.z * m_template->m_field1EC;
                node->setPositionalAcceleration(acceleration);
            }
        }
    }
}

// . Messages 24 and 29 share the TTL arm. The local-predict branch repeats
// getLocalPredict(), and the server/client destroy choice retains the original recursive message
// dispatch rather than folding the states together.
void GenericProjectile::handleMessage(TemplateMessage message, IPlayer* player, bool flag)
{
    typedef object_detail::LayoutAccess L;

    switch (static_cast<int>(message))
    {
    case 0:
        if (!m_byte181)
            startStartEffect();
        return;

    case 24:
    case 29:
        if (getLocalPredict())
        {
            if (getLocalPredict() && m_byte206)
                handleMessage(static_cast<TemplateMessage>(32), player, flag);
            return;
        }

        // getNetworkable(), NOT isCreatedByLocalPlayer() - ELF 0x8281707 calls +0xD8 (slot 54).
        // with the two sites below; see the note above detonate().
        if (!m_byte181 && (gameLogic->isServer() || !getNetworkable()))
        {
            IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(this));
            if (node)
            {
                const Vec3& speed = node->getPositionalSpeed();
                const float speedMagnitude = static_cast<float>(std::sqrt(
                    speed.x * speed.x + speed.y * speed.y + speed.z * speed.z));
                if (speedMagnitude > 1.0f
                    && m_tick1C8 + 10 < static_cast<int>(WorldPref::getTickCount()))
                {
                    m_dword1C4 = 0;
                    const float* const transform = getAbsoluteTransformation().m;
                    m_vector1CC.x = -transform[8];
                    m_vector1CC.y = -transform[9];
                    m_vector1CC.z = -transform[10];
                    dropWeakBlock(m_targetObjectWeak);
                    m_targetObjectWeak = 0;
                    m_vec1D8 = getAbsolutePosition();
                }
            }
        }

        if (!m_template->m_hasOnTimeEffect || m_byte181)
            handleMessage(static_cast<TemplateMessage>(32), player, flag);
        else
            handleMessage(static_cast<TemplateMessage>(31), player, flag);
        return;

    case 31:
        if (!m_byte181)
        {
            if (getDetonateAfterProjectileStopped())
            {
                m_byte20C = true;
                updateFlags(0x800u, 0);
            }
            else
            {
                detonate(false);
            }
        }
        return;

    case 32:
        if (!m_byte1FC)
        {
            // Clients hide networked rounds; ghost removal owns their destruction.
            if (!gameLogic->isServer() && getNetworkable())
                hideProjectile();
            else
                game->destroyObject(this);
        }
        return;

    default:
        return;
    }
}

// . This covers the complete componentless server arm and the typed
// collision-component dispatch. The seek-only target-position calculation feeds a dead local in
// the shipped function; its currently untyped manager callback is isolated to that non-null arm.
bool GenericProjectile::handleCollision(IObject* other,
                                        const Vec3& vecA,
                                        const Vec3& vecB,
                                        const Vec3& vecC,
                                        int intA,
                                        int materialId,
                                        CollisionFaceInfo* faceA,
                                        CollisionFaceInfo* faceB)
{
    if (m_byte181)
        return true;

    if (m_byte1FC)
    {
        objectManager->postMessage(this, static_cast<TemplateMessage>(32), 0.0f);
        return false;
    }

    IProjectileObject* const tracerFace = m_associatedTracerWeak
        ? static_cast<WeakPtr<IProjectileObject>::Ctrl*>(m_associatedTracerWeak)->target
        : 0;
    if (tracerFace)
    {
        GenericProjectile* const tracer = static_cast<GenericProjectile*>(tracerFace);
        tracer->resetProjectile(false);
        // ..0x828369D / PE 0x5B32AE..0x5B32CD re-resolve after reset.
        IProjectileObject* const currentTracer = m_associatedTracerWeak
            ? static_cast<WeakPtr<IProjectileObject>::Ctrl*>(m_associatedTracerWeak)->target
            : 0;
        game->destroyObject(static_cast<IObject*>(static_cast<GenericProjectile*>(currentTracer)));
        dropWeakBlock(m_associatedTracerWeak);
        m_associatedTracerWeak = 0;
    }

    // Updates the collision material only while dry or when hitting water, preserving the water
    // impact material for an already-submerged projectile.
    Material* const collisionMaterial =
        static_cast<MaterialManager*>(materialManager)->getMaterialPtr(
            static_cast<unsigned int>(materialId));
    if (getUnderWater() == 0.0f
        || (collisionMaterial && collisionMaterial->getHasWaterPhysics()))
    {
        m_dword1C4 = materialId;
    }

    m_vector1CC = vecB;
    m_tick1C8 = static_cast<int>(WorldPref::getTickCount());
    const Vec3& absolutePosition = getAbsolutePosition();
    m_vec1D8.x = absolutePosition.x + vecC.x;
    m_vec1D8.y = absolutePosition.y + vecC.y;
    m_vec1D8.z = absolutePosition.z + vecC.z;
    assignObjectWeakBlock(m_targetObjectWeak, other);

    if (grenade_trace::selected(this))
    {
        grenade_trace::log(this, "collision",
            "other=%u material=%d recorded_material=%d int_a=%d "
            "a=(%.9g,%.9g,%.9g) b=(%.9g,%.9g,%.9g) c=(%.9g,%.9g,%.9g) "
            "impact=(%.9g,%.9g,%.9g)",
            other ? other->getObjectId() : 0u, materialId, m_dword1C4, intA,
            vecA.x, vecA.y, vecA.z, vecB.x, vecB.y, vecB.z, vecC.x, vecC.y, vecC.z,
            m_vec1D8.x, m_vec1D8.y, m_vec1D8.z);
    }

    // Missile debugging still performs the target/position calls even though their result is discarded.
    // [PLATFORM] This build reads the debug counter through GameLogic instead of the ELF's direct global load.
    if (gameLogic->getDebugMissiles() && m_seekComp)
    {
        IObject* const seekTarget = m_seekComp->getTargetObject();
        if (seekTarget)
        {
            const Vec3 debugTargetPosition = seekTarget->getAbsolutePosition();
            (void)debugTargetPosition;
        }
    }

    if (!m_collisionComp)
        return false;

    if (!getLocalPredict() || (!m_followComp && !m_seekComp))
    {
        return m_collisionComp->handleCollision(
            other, vecA, vecB, vecC, intA, materialId, m_template->m_material,
            faceA, faceB);
    }
    return false;
}

// Detonates the projectile and reports impact through GameLogic.
// Networkable presence controls impact/position/destruction handling; it is not a local-player ownership test.
void GenericProjectile::detonate(bool forced)
{
    // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - the three BF2_FX_TRACE lines, FxTrace.h
    BF2_FX_TRACE("projectile detonate: %s forced=%d networked=%d server=%d tracer=%d",
                 m_template->getName().c_str(), forced ? 1 : 0,
                 m_template->getNetworkableInfo().empty() ? 0 : 1,
                 gameLogic->isServer() ? 1 : 0, m_byte1FC ? 1 : 0);
    if (!forced && !m_template->getNetworkableInfo().empty() && !gameLogic->isServer())
        return;

    if (m_byte1FC)
    {
        objectManager->postMessage(this, static_cast<TemplateMessage>(32), 0.5f);
        return;
    }

    destroyTrailEffect();

    const int currentTick = static_cast<int>(WorldPref::getTickCount());
    BF2_FX_TRACE("projectile arming: %s activated=%d delayTicks=%d now=%d armed=%d detonationComp=%d",
                 m_template->getName().c_str(), m_dword1BC, m_template->m_armingDelayTicks,
                 currentTick, m_dword1BC + m_template->m_armingDelayTicks <= currentTick ? 1 : 0,
                 m_detonationComp ? 1 : 0);
    if (m_dword1BC + m_template->m_armingDelayTicks <= currentTick)
    {
        // Keep the position reference live across collision-position relocation below.
        const Vec3& position = getAbsolutePosition();
        BF2_FX_TRACE("projectile impact arm: %s taken=%d pos=(%.1f %.1f %.1f)",
                     m_template->getName().c_str(),
                     ((!gameLogic->isServer() && getNetworkable())
                      || m_tick1C8 > static_cast<int>(WorldPref::getTickCount()) + 1) ? 1 : 0,
                     position.x, position.y, position.z);
        if ((!gameLogic->isServer() && getNetworkable())
            || m_tick1C8 > static_cast<int>(WorldPref::getTickCount()) + 1)
        {
            // THE impact report FOR A detonation that never collided here. This arm runs on a
            // client for any networked round (the server did the colliding), or when the collision
            // stamp is in the future - i.e. nothing has actually hit anything yet - so the material
            // and surface data come from the projectile itself rather than from a collision.
            // Arguments 8, 9 and 10 are all null: there is no other object, no projectile face
            // and no collision face to report. The Vec3 argument is a zero vector.
            const Mat4& transform = getAbsoluteTransformation();
            Vec3 forward;
            forward.x = transform.m[8];
            forward.y = transform.m[9];
            forward.z = transform.m[10];

            gameLogic->doImpact(getMaterial(), m_dword1C4,
                                static_cast<MaterialCollisionType>(getDefaultCollisionType(0)),
                                position, m_vector1CC, forward,
                                this, 0, 0, 0, Vec3::getZero(), false);
        }
        else if (m_tick1C8 != -1000)
        {
            setAbsolutePosition(m_vec1D8);
        }

        if (m_detonationComp)
        {
            // resolve the IObject weak target. The producer
            // uses WeakPtr<IObject>::Ctrl; its target is +4 on x86, +8 on native x64.
            IObject* const target = m_targetObjectWeak
                ? static_cast<WeakPtr<IObject>::Ctrl*>(m_targetObjectWeak)->target
                : 0;
            m_detonationComp->detonate(target, m_dword1C4, position,
                                       static_cast<unsigned int>(m_tick1C8),
                                       m_vector1CC, 0);
        }
    }
    else if (!m_followComp)
    {
        return;
    }

    if (grenade_trace::selected(this))
    {
        grenade_trace::log(this, "detonate", "forced=%d cached_position=(%.9g,%.9g,%.9g)",
            forced ? 1 : 0, m_absoluteTransformation.m[12],
            m_absoluteTransformation.m[13], m_absoluteTransformation.m[14]);
    }
    hideProjectile();
    const float destroyDelay = getNetworkable() && gameLogic->isServer() ? 0.5f : 0.0f;
    objectManager->postMessage(this, static_cast<TemplateMessage>(32), destroyDelay);

    // . The same removal resetProjectile does, and the last thing detonate does.
    if (ai::g_bfCollisionHandler)
        reinterpret_cast<ai::IBFCollisionHandler*>(ai::g_bfCollisionHandler)
            ->handleProjectileTimeout(static_cast<IProjectileObject*>(this), this);
}

// . The ordinary server path hides self/children, disables and zeros physics,
// destroys an associated tracer if present, and stops all child effects.
void GenericProjectile::hideProjectile()
{
    typedef object_detail::LayoutAccess L;

    m_byte181 = true;
    updateFlags(0, 0x1502u);
    updateFlagsRecursive(0, 0x1402u);
    disablePhysics(this, false);

    IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(this));
    if (node)
    {
        node->setPositionalSpeed(Vec3::getZero());
        node->setPositionalAcceleration(Vec3::getZero());
    }

    IProjectileObject* const tracerFace = m_associatedTracerWeak
        ? static_cast<WeakPtr<IProjectileObject>::Ctrl*>(m_associatedTracerWeak)->target
        : 0;
    if (tracerFace)
    {
        GenericProjectile* const tracer = static_cast<GenericProjectile*>(tracerFace);
        tracer->resetProjectile(false);
        // ..0x82831DC / PE 0x5B2FE0..0x5B3003 re-resolve after reset.
        IProjectileObject* const currentTracer = m_associatedTracerWeak
            ? static_cast<WeakPtr<IProjectileObject>::Ctrl*>(m_associatedTracerWeak)->target
            : 0;
        game->destroyObject(static_cast<IObject*>(static_cast<GenericProjectile*>(currentTracer)));
        dropWeakBlock(m_associatedTracerWeak);
        m_associatedTracerWeak = 0;
    }

    stopEffects();
}

// activate - ELF 0x8280700 (3,467 B), PE 0x5B1FF0 (920 B). The common launch state, physics,
// timers, flags, component dispatch, and immediate start-effect child messages are recovered in
// shipped order. The two authored effect arms whose concrete runtime interfaces are not present in
// this tree remain branch-local named stops; the stock dedicated-server rifle projectile has both
// caches null and does not enter either stop. [RETIRE-WHEN: genericprojectile-class]
void GenericProjectile::activate()
{
    typedef object_detail::LayoutAccess L;

    if (m_isActive)
        return;

    m_isActive = true;
    setObjectInDestroyList(false);
    m_playedImpactEffectAtTick = 0;              // the impact-effect throttle, cleared on launch

    unsigned int setFlags = 0;
    unsigned int clearFlags = 0;
    if (((!gameLogic->isServer() && getNetworkable()) || getLocalPredict())
        && (m_followComp || m_seekComp))
    {
        clearFlags |= 0x100u;
    }
    else
    {
        setFlags |= 0x100u;
    }

    m_exactLaunchTime = static_cast<float>(g_system->getExactTime());
    m_worldLaunchTime = static_cast<float>(WorldPref::getWorldTime());
    m_byte181 = false;

    if (m_detonationComp)
        m_detonationComp->activate();

    m_dword1BC = WorldPref::getTickCount();
    m_tick1C8 = -1000;
    m_dword1C4 = 0;
    m_damage = m_template->m_projectileDamage;
    m_vector1CC.x = 0.0f;
    m_vector1CC.y = 0.0f;
    m_vector1CC.z = 1.0f;

    dropWeakBlock(m_targetObjectWeak);
    m_targetObjectWeak = 0;
    m_vec1D8.x = 0.0f;
    m_vec1D8.y = 0.0f;
    m_vec1D8.z = 0.0f;
    m_dword200 = 0;
    m_countHit = false;
    m_byte20C = false;

    if (m_template->m_invisible)
    {
        clearFlags |= 0x1402u;
    }
    else
    {
        setFlags |= 0x402u;
        if (L::geometry(this))
            setFlags |= 0x1000u;
    }

    resetPhysics(this, false);
    if (isPhysicsDisabled(this))
        reenablePhysics(this, false);

    if (m_soundComp)
        m_soundComp->activate();
    if (m_seekComp)
        m_seekComp->activate();
    if (m_followComp)
        m_followComp->activate();

    const float startDelay = m_template->m_field1D4;
    if (startDelay <= 0.0f)
        startStartEffect();
    else
        objectManager->postMessage(this, static_cast<TemplateMessage>(0), startDelay);

    float timeToLive = sampleRandomDistribution(m_template->m_timeToLive);
    if (getLocalPredict())
        timeToLive = 1.0f;
    objectManager->postMessage(this, static_cast<TemplateMessage>(29), timeToLive);

    if (m_template->m_tracerScaler > 0.0f)
    {
        IBase* const geometry = static_cast<IBase*>(L::geometry(this));
        if (geometry)
        {
            IScaleable* const scaleable = static_cast<IScaleable*>(
                geometry->queryInterface(geom::IID_IScaleable));
            if (scaleable)
            {
                Vec3 scale;
                scale.x = m_template->m_tracerSizeModifier;
                scale.y = m_template->m_tracerSizeModifier;
                scale.z = m_template->m_minTracerScaler * m_template->m_tracerSizeModifier;
                scaleable->setScale(scale);
            }
            setFlags |= 0x800u;
        }
    }

    if ((m_collisionComp && m_collisionComp->hasUpdate())
        || (m_detonationComp && m_detonationComp->hasUpdate())
        || (m_seekComp && m_seekComp->hasUpdate())
        || (m_followComp && m_followComp->hasUpdate()))
    {
        setFlags |= 0x800u;
    }

    ++m_dword1B8;
    if (m_template->m_addToProjectileList)
        gameLogic->addProjectile(this);

    if (m_collisionComp)
        m_collisionComp->activate();

    // Chooses the wet/dry trail by projectile depth. Samples the effect distribution even though only
    // the -1 sentinel is used; the draw must advance the shared generator whether a trail is created or not.
    IObjectTemplate* const trailTemplate = getUnderWater() > 0.0f
        ? m_template->m_field210
        : m_template->m_field214;
    if (trailTemplate)
    {
        // The shipped code reads the distribution straight off the template with no type check,
        // so retail's field was typed as the concrete effect template. The static_cast reproduces
        // that assumption; an authored trail naming a non-EffectBundle template would have been
        // just as wrong in retail. Reading it by member rather than by ELF offset is deliberate -
        // +0x2E0 is past this class's STL members and does not survive the MSVC rebuild.
        const EffectBundleTemplate* const effectTemplate =
            static_cast<const EffectBundleTemplate*>(trailTemplate);
        if (sampleRandomDistribution(effectTemplate->m_dist2E0) != -1.0f)
            m_trailEffectObject = trailTemplate->createObject();
    }

    playTrailEffect();
    // This is the self-inclusive free helper, not Object's descendant-only virtual. The root must
    // receive 0x800 when SeekClosest hasUpdate(), or neither this motor nor seek/follow ever ticks.
    ::dice::hfe::world::updateFlagsRecursive(this, setFlags, clearFlags);
}

// . Every child receives message 68 and then message 0 before the walk
// advances to its sibling. The stock AK101 projectile's looping Sound child relies on both calls.
void GenericProjectile::startStartEffect()
{
    for (IObject* child = const_cast<IObject*>(getChild()); child;
         child = child->getSibling())
    {
        child->handleMessage(static_cast<TemplateMessage>(68), 0, false);
        child->handleMessage(static_cast<TemplateMessage>(0), 0, false);
    }
}

// . Stop messages mirror startStartEffect but use 1 then 69.
void GenericProjectile::stopEffects()
{
    for (IObject* child = const_cast<IObject*>(getChild()); child;
         child = child->getSibling())
    {
        child->handleMessage(static_cast<TemplateMessage>(1), 0, false);
        child->handleMessage(static_cast<TemplateMessage>(69), 0, false);
    }
}

// . The effect object owns its shutdown/destroy sequence; clear the field only after
// both calls, exactly as shipped.
void GenericProjectile::destroyTrailEffect()
{
    if (m_trailEffectObject)
    {
        m_trailEffectObject->handleMessage(static_cast<TemplateMessage>(1), 0, false);
        m_trailEffectObject->destroy(true);
        m_trailEffectObject = 0;
    }
}

// Restarts an existing trail, forcing its up axis vertical while retaining heading.
// Sets flag 2048 on the projectile and recursively, then initializes and starts the effect again.
// Returns immediately without a trail object.
void GenericProjectile::playTrailEffect()
{
    if (!m_trailEffectObject)
        return;

    updateFlags(2048, 0);
    updateFlagsRecursive(2048, 0);

    Mat4 basis = getAbsoluteTransformation();

    // The flatten: the middle row is replaced by world up, not merely stripped of X and Z. The
    // 1.0f store into m[5] is real - flt_888CA8C, the same constant the two normalise guards use -
    // so the projectile's own Y scale is discarded outright.
    // That distinction is not cosmetic. Keeping m[5] would mirror the whole basis for a round
    // rolled past vertical (negative m[5]) and collapse it to zero for one pitched exactly vertical.
    basis.m[4] = 0.0f;
    basis.m[5] = 1.0f;
    basis.m[6] = 0.0f;

    Vec3 row1;
    row1.x = basis.m[4]; row1.y = basis.m[5]; row1.z = basis.m[6];
    Vec3 row2;
    row2.x = basis.m[8]; row2.y = basis.m[9]; row2.z = basis.m[10];

    Vec3 row0;
    row0.x = row1.y * row2.z - row1.z * row2.y;
    row0.y = row1.z * row2.x - row1.x * row2.z;
    row0.z = row1.x * row2.y - row1.y * row2.x;
    normaliseVec3(row0);

    row2.x = row0.y * row1.z - row0.z * row1.y;
    row2.y = row0.z * row1.x - row1.z * row0.x;
    row2.z = row1.y * row0.x - row1.x * row0.y;
    normaliseVec3(row2);

    basis.m[0] = row0.x; basis.m[1] = row0.y; basis.m[2] = row0.z;
    basis.m[8] = row2.x; basis.m[9] = row2.y; basis.m[10] = row2.z;

    m_trailEffectObject->setAbsoluteTransformation(basis);
    m_trailEffectObject->init();
    m_trailEffectObject->handleMessage(static_cast<TemplateMessage>(0), 0, false);
}

// . The geometry-template slot is the one genuine ABI difference and is
// isolated above; every other operation is a typed interface call or a recovered layout access.
void GenericProjectile::updateTracer()
{
    typedef object_detail::LayoutAccess L;

    if (m_template->m_tracerScaler <= 0.0f)
        return;

    IBase* const geometry = static_cast<IBase*>(L::geometry(this));
    if (!geometry)
        return;
    if (getGeometryTemplate(geometry) != m_template->m_tracerTemplate)
        return;

    IScaleable* const scaleable = static_cast<IScaleable*>(
        geometry->queryInterface(geom::IID_IScaleable));
    if (!scaleable || !L::physicsNode(this))
        return;

    Vec3 scale;
    scale.x = m_template->m_tracerSizeModifier;
    scale.y = m_template->m_tracerSizeModifier;
    scale.z = ((m_template->m_maxTracerScaler + m_template->m_minTracerScaler) * 0.5f)
        * m_template->m_tracerSizeModifier;
    scaleable->setScale(scale);
}

// Refreshes the tracer, then calls SimpleObject's visual update with both arguments unchanged.
// Keep the direct/qualified calls; virtual redispatch would change the target or recurse.
void GenericProjectile::handleVisualUpdate(float deltaTime, float interpolation)
{
    updateTracer();                                          // a direct call
    SimpleObject::handleVisualUpdate(deltaTime, interpolation);
}

// . The missile arm is server-only and requires both an owner and a
// networkable. Ordinary dedicated-server bullets have no networkable, so updateTracer remains the
// unconditional final operation exactly as shipped.
void GenericProjectile::initOnLaunch()
{
    m_startPos = getAbsolutePosition();
    if (getPlayer() && gameLogic->isServer() && getNetworkable())
    {
        sendMissileInitEvent();
        postInit();
    }
    updateTracer();
}

// . The repeated isChildSoldierActive call and the true arm's missing
// null guard on the occupying object's PCO are both shipped behaviour. getWeapons is deliberately
// called only for its side effect before the event is assembled.
void GenericProjectile::sendMissileInitEvent()
{
    IObject* const launcherWeapon = getLauncherWeapon();
    if (!launcherWeapon)
        return;

    IPlayer* const player = getPlayer();
    if (!player)
        return;

    IObject* const launcherVehicle = getLauncherVehicle();
    if (!launcherVehicle)
        return;

    IPlayerControlObject* const vehiclePco = findPlayerControlObject(launcherVehicle);
    if (!vehiclePco)
        return;

    bool useDefaultVehicle = false;
    if (vehiclePco->isChildSoldierActive()
        || launcherVehicle == player->getDefaultVehicle())
    {
        useDefaultVehicle = true;
    }

    if (vehiclePco->isChildSoldierActive())
    {
        IPlayerControlObject* const occupyingPco =
            findPlayerControlObject(vehiclePco->getOccupingObject());
        // No null guard in either port: preserve the retail precondition.
        occupyingPco->getWeapons();
    }
    else
    {
        vehiclePco->getWeapons();
    }

    IWeaponObject* const weapon = static_cast<IWeaponObject*>(
        launcherWeapon->queryInterface(IID_IWeaponObject));
    // No query-result guard in either shipped body.
    const int weaponIndex = weapon->getNetworkSafeItemIndex();
    if (weaponIndex == -1)
        return;

    io::INetworkable* const networkable = getNetworkable();
    MissileInitEvent* const event = new MissileInitEvent(
        networkable->m_networkableId, player->getId(), weaponIndex, useDefaultVehicle);
    game->sendGameEventToAllConnected(event);
}

// . Camera and touch registration are independent conditions.
void GenericProjectile::postInit()
{
    if (getPlayer() && m_template->m_useProjectileCamera)
        useProjectileCamera();

    if (m_template->m_byte1FA && SingletonServer::m_iNametagManager)
        SingletonServer::m_iNametagManager->addTouchItem(static_cast<IObject*>(this));
}

// . The player guard precedes the launcher-vehicle/PCO walk. The PCO is only an
// eligibility test here; enableProjectileCamera repeats the weapon-side walk itself.
void GenericProjectile::useProjectileCamera()
{
    if (!getPlayer())
        return;

    IObject* const launcherVehicle = getLauncherVehicle();
    if (launcherVehicle && findPlayerControlObject(launcherVehicle))
        enableProjectileCamera();
}

// . Preserve the repeated active-camera lookup before saving its id.
void GenericProjectile::enableProjectileCamera()
{
    if (!m_projectileCamera)
        return;

    IObject* const launcherWeapon = getLauncherWeapon();
    if (!launcherWeapon)
        return;

    IPlayerControlObject* const pco = findPlayerControlObject(launcherWeapon);
    if (!pco || !pco->getActiveCamera())
        return;

    m_dword1EC = pco->getActiveCamera()->getCameraId();
    pco->addExternalCamera(m_projectileCamera);
    pco->setActiveCamera(m_projectileCamera->getCameraId());
}

// (IProjectileObject face +0x38). Both ports first resolve the
// launcher-weapon weak pointer only as a gate, then resolve it a second time for the parent-chain
// PCO walk. The camera null test deliberately follows that walk, and getActiveCamera is called only
// when both the PCO and projectile camera exist.
bool GenericProjectile::isInProjectileCamera()
{
    IPlayerControlObject* pco = 0;
    if (getLauncherWeapon())
        pco = findPlayerControlObject(getLauncherWeapon());

    return pco && m_projectileCamera
        && pco->getActiveCamera() == m_projectileCamera;
}

// . The saved id is cleared whenever a projectile camera exists, even when the
// launcher or PCO can no longer be resolved.
void GenericProjectile::disableProjectileCamera()
{
    if (!m_projectileCamera)
        return;

    IObject* const launcherWeapon = getLauncherWeapon();
    if (launcherWeapon)
    {
        IPlayerControlObject* const pco = findPlayerControlObject(launcherWeapon);
        if (pco)
        {
            if (pco->getActiveCamera() == m_projectileCamera)
                pco->setActiveCamera(m_dword1EC);
            pco->removeExternalCamera(m_projectileCamera);
        }
    }
    m_dword1EC = -1;
}

// three strstr probes over the template's name, folded into +0x188 of... the
// explosion type slot: 3 = teargas, 1 = flashbang, 2 = molotovcocktail, 0 = anything else. The
// ELF's repeated c_str() flattenings collapse to plain reads here.
void GenericProjectile::determineExplosionType()
{
    const char* const name = m_template->getName().c_str();
    int type;
    if (std::strstr(name, "teargas"))
        type = 3;
    else if (std::strstr(name, "flashbang"))
        type = 1;
    else
        type = 2 * (std::strstr(name, "molotovcocktail") != 0);
    m_explosionType = type;                          // +0x220 the ctor-skipped slot, seeded here
}

// / 0x8285230, 50 B - both through the detonation comp, both no-ops without one.
bool GenericProjectile::getDetonate() const
{
    if (m_detonationComp)
        return m_detonationComp->getDetonate();
    return false;
}

void GenericProjectile::setDetonate(bool detonate)
{
    if (m_detonationComp)
        m_detonationComp->setDetonate(detonate);
}

// / 0x82842F0 and PE equivalents. Both are exact optional seek-component
// delegates; a projectile without a seek component reports zero.
float GenericProjectile::getMaxTargetDistance() const
{
    return m_seekComp ? m_seekComp->getMaxTargetDistance() : 0.0f;
}

float GenericProjectile::getMaxTargetAngle() const
{
    return m_seekComp ? m_seekComp->getMaxTargetAngle() : 0.0f;
}

// / 0x8284880. The boolean handed to the seek component is the literal true;
// FixedTargetComp ignores it, while the second seek implementation observes it.
void GenericProjectile::setTargetObject(IObject* target)
{
    if (m_seekComp)
        m_seekComp->setTargetObject(target, true);
}

IObject* GenericProjectile::getTargetObject()
{
    return m_seekComp ? m_seekComp->getTargetObject() : 0;
}

// / 0x82852A0. Both are exact optional collision-component delegates; the source-level
// virtual calls preserve the ABI-specific interface destructor-slot difference.
void GenericProjectile::stick(IObject* host, const Mat4& transform)
{
    if (m_collisionComp)
        m_collisionComp->stick(host, transform);
}

void GenericProjectile::stickAbsolute(const Mat4& transform)
{
    if (m_collisionComp)
        m_collisionComp->stickAbsolute(transform);
}

// / 0x8285300; the PE bodies confirm the same two dispatches. Return types are pinned
// by the StickyProjectileEvent constructor caller: the first result is its IObject* target and the
// second is passed by const Mat4&. Typed dispatch preserves the one-slot MSVC/Itanium destructor
// difference in the collision interface.
IObject* GenericProjectile::getStickOnParent() const
{
    return m_collisionComp ? m_collisionComp->getHostObject() : 0;
}

const Mat4& GenericProjectile::getStickTransformation() const
{
    if (m_collisionComp)
        return m_collisionComp->getStickTransformation();

    Debug(DEBUGTYPE_6,
          "Game/Objects/Projectiles/GenericProjectile.cpp", 1933, "")
        << "Default impl";
    return Mat4::getIdentity();
}

bool GenericProjectile::getPlayMaterialEffectAtSurface() const
{
    return m_detonationComp
        ? m_detonationComp->getPlayMaterialEffectAtSurface()
        : false;
}

bool GenericProjectile::hasExplosion() const
{
    return m_detonationComp ? m_detonationComp->hasExplosion() : false;
}

int GenericProjectile::getExplosionMaterial() const
{
    return m_detonationComp ? m_detonationComp->getExplosionMaterial() : 0;
}

float GenericProjectile::getExplosionRadius() const
{
    return m_detonationComp ? m_detonationComp->getExplosionRadius() : 0.0f;
}

float GenericProjectile::getExplosionInnerConeRadius() const
{
    return m_detonationComp ? m_detonationComp->getExplosionInnerConeRadius() : 0.0f;
}

// ..0x8285180 / PE 0x5B2340..0x5B2D80. Every accessor in this block is the same
// exact nullable component dispatch: the attached detonation component supplies the authored
// value, while a projectile without one returns the neutral scalar used by retail. The complete
// typed interface replaces the old PE/ELF-specific +0xCC/+0xD0 tail-slot seam.
float GenericProjectile::getFlashbangRadiusWithNightVision() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangRadiusWithNightVision() : 0.0f;
}

float GenericProjectile::getExplosionSoldierLineOfSight() const
{
    return m_detonationComp ? m_detonationComp->getExplosionSoldierLineOfSight() : 0.0f;
}

float GenericProjectile::getExplosionSoldierLineOfSightTimer() const
{
    return m_detonationComp ? m_detonationComp->getExplosionSoldierLineOfSightTimer() : 0.0f;
}

float GenericProjectile::getExplosionSoldierLineOfSightHealRate() const
{
    return m_detonationComp ? m_detonationComp->getExplosionSoldierLineOfSightHealRate() : 0.0f;
}

float GenericProjectile::getExplosionLineOfSightMinimumDamage() const
{
    return m_detonationComp ? m_detonationComp->getExplosionLineOfSightMinimumDamage() : 0.0f;
}

float GenericProjectile::getExplosionConeAngle() const
{
    return m_detonationComp ? m_detonationComp->getExplosionConeAngle() : 0.0f;
}

float GenericProjectile::getExplosionYMod() const
{
    return m_detonationComp ? m_detonationComp->getExplosionYMod() : 0.0f;
}

float GenericProjectile::getExplosionForce() const
{
    return m_detonationComp ? m_detonationComp->getExplosionForce() : 0.0f;
}

float GenericProjectile::getExplosionDamage() const
{
    return m_detonationComp ? m_detonationComp->getExplosionDamage() : 0.0f;
}

float GenericProjectile::getFlashbangBillboardMinAlpha() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangBillboardMinAlpha() : 0.0f;
}

float GenericProjectile::getFlashbangBillboardMaxAlpha() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangBillboardMaxAlpha() : 0.0f;
}

float GenericProjectile::getFlashbangBillboardMaxRampTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangBillboardMaxRampTime() : 0.0f;
}

float GenericProjectile::getFlashbangBillboardMinHoldTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangBillboardMinHoldTime() : 0.0f;
}

float GenericProjectile::getFlashbangBillboardMaxHoldTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangBillboardMaxHoldTime() : 0.0f;
}

float GenericProjectile::getFlashbangBillboardMinHealTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangBillboardMinHealTime() : 0.0f;
}

float GenericProjectile::getFlashbangBillboardMaxHealTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangBillboardMaxHealTime() : 0.0f;
}

float GenericProjectile::getFlashbangAdditiveGlowMinAlpha() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangAdditiveGlowMinAlpha() : 0.0f;
}

float GenericProjectile::getFlashbangAdditiveGlowMaxAlpha() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangAdditiveGlowMaxAlpha() : 0.0f;
}

float GenericProjectile::getFlashbangAdditiveGlowMaxRampTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangAdditiveGlowMaxRampTime() : 0.0f;
}

float GenericProjectile::getFlashbangAdditiveGlowMinHoldTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangAdditiveGlowMinHoldTime() : 0.0f;
}

float GenericProjectile::getFlashbangAdditiveGlowMaxHoldTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangAdditiveGlowMaxHoldTime() : 0.0f;
}

float GenericProjectile::getFlashbangAdditiveGlowMinHealTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangAdditiveGlowMinHealTime() : 0.0f;
}

float GenericProjectile::getFlashbangAdditiveGlowMaxHealTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangAdditiveGlowMaxHealTime() : 0.0f;
}

float GenericProjectile::getFlashbangMotionBlurMinAlpha() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangMotionBlurMinAlpha() : 0.0f;
}

float GenericProjectile::getFlashbangMotionBlurMaxAlpha() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangMotionBlurMaxAlpha() : 0.0f;
}

float GenericProjectile::getFlashbangMotionBlurMaxRampTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangMotionBlurMaxRampTime() : 0.0f;
}

float GenericProjectile::getFlashbangMotionBlurMinHoldTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangMotionBlurMinHoldTime() : 0.0f;
}

float GenericProjectile::getFlashbangMotionBlurMaxHoldTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangMotionBlurMaxHoldTime() : 0.0f;
}

float GenericProjectile::getFlashbangMotionBlurMinHealTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangMotionBlurMinHealTime() : 0.0f;
}

float GenericProjectile::getFlashbangMotionBlurMaxHealTime() const
{
    return m_detonationComp ? m_detonationComp->getFlashbangMotionBlurMaxHealTime() : 0.0f;
}

bool GenericProjectile::hasCollisionEffect() const
{
    return m_collisionComp ? m_collisionComp->hasCollisionEffect() : false;
}

float GenericProjectile::getHealEffect(Vec3& effect) const
{
    return m_collisionComp ? m_collisionComp->getHealEffect(effect) : 0.0f;
}

int GenericProjectile::getDefaultCollisionType(int type) const
{
    return m_collisionComp ? m_collisionComp->getDefaultCollisionType(type) : 0;
}

int GenericProjectile::getExplosionType() const
{
    return m_explosionType;
}

bool GenericProjectile::getDamageMandownSoldiers() const
{
    return m_template->m_damageMandownSoldiers != 0;
}

bool GenericProjectile::getIsActive() const { return m_isActive; }
void GenericProjectile::setTeam(int team)   { m_team = team; }
int  GenericProjectile::getTeam()           { return m_team; }

// . WeakPtr assignment preserves the retain-before-release self-assignment rule.
void GenericProjectile::setPlayer(IPlayer* player)
{
    m_player = player ? player->getWeakPtr() : WeakPtr<IPlayer>();
}

// getPlayer - 0x8282650, 23 B, the weak
// read on +0x174 (ctrl refcount +0, target +4) - null until setPlayer runs at fire time, so every
// warm-up drain build answers 0 here, exactly as retail's do.
IPlayer* GenericProjectile::getPlayer() const
{
    return m_player.get();
}

// / 0x82876E0. The setter takes the list by value in the mangled name and then performs
// a normal source-level list assignment; the returned pointer is the projectile-owned list.
void GenericProjectile::setAssistPlayers(std::list<WeakPtr<IPlayer> > players)
{
    m_assistPlayers = players;
}

std::list<WeakPtr<IPlayer> >* GenericProjectile::getAssistPlayers()
{
    return &m_assistPlayers;
}

void GenericProjectile::setLauncherWeapon(IObject* weapon)
{
    m_launcherWeapon = weapon ? weapon->getWeakPtr() : WeakPtr<IObject>();
}

IObject* GenericProjectile::getLauncherWeapon() const
{
    return m_launcherWeapon.get();
}

// / 0x82827F0. Preserve the shipped null-arm defect visible in both ports:
// setLauncherVehicle(0) clears the launcher weapon weak pointer (+0x170), not the vehicle field.
void GenericProjectile::setLauncherVehicle(IObject* vehicle)
{
    if (vehicle)
        m_launcherVehicle = vehicle->getWeakPtr();
    else
        m_launcherWeapon = WeakPtr<IObject>();
}

IObject* GenericProjectile::getLauncherVehicle() const
{
    return m_launcherVehicle.get();
}

// world::projectileOwner - THE seam resolved. Camera.cpp and
// ResponsePhysicsManager.cpp both declare `IPlayer* projectileOwner(void*)` for what retail
// dispatches as IProjectileObject subtable slot +0x54; the subtable read (this sitting, banked in
// IProjectileObject.h) names that slot: GenericProjectile::getPlayer. The void* the callers hold
// is the queryInterface(IID_IProjectileObject) result - the IProjectileObject subobject - and
// GenericProjectile is the interface's only implementor in this tree (and in the ELF's vtable
// census), so the typed downcast below IS the retail dispatch, minus the thunk.
IPlayer* projectileOwner(void* projectile)
{
    GenericProjectile* self =
        static_cast<GenericProjectile*>(static_cast<IProjectileObject*>(projectile));
    return self->getPlayer();
}

// Returns remaining lifetime from a GenericProjectile interface face.
// Requires the interface's sole known implementer, GenericProjectile; typed conversion adjusts the base.
float projectileTimeToLive(void* projectile)
{
    GenericProjectile* self =
        static_cast<GenericProjectile*>(static_cast<IProjectileObject*>(projectile));
    return self->getTimeToLive();
}

void GenericProjectile::setStartPos(Vec3 startPos)
{
    m_startPos = startPos;
}

Vec3 GenericProjectile::getStartPos()
{
    return m_startPos;
}

// The two ISoundNode slots - the same shapes RotationalBundle ships.
const Mat4& GenericProjectile::getTransform()
{
    return getAbsoluteTransformation();
}

const Math::Vec3* GenericProjectile::getVelocity()
{
    typedef object_detail::LayoutAccess L;
    PointPhysicsNode* const node = static_cast<PointPhysicsNode*>(L::physicsNode(this));
    if (node)
        return &node->getPositionalSpeed();
    return 0;
}

// / 0x8284660. The interval stays a virtual template read; hasTracer tests the resolved
// tracer-template cache itself (not merely the authored name).
int GenericProjectile::getTracerInterval() const
{
    return m_template->getTracerInterval();
}

bool GenericProjectile::hasTracer() const
{
    return m_template->m_tracerTemplate != 0;
}

// Replaces available tracer geometry, raises its visibility flags and records tracer state.
void GenericProjectile::turnIntoTracer(bool tracer)
{
    IObjectTemplate* const tracerTemplate = m_template->getTracerTemplate();
    if (tracerTemplate)
    {
        IBase* const geometry = createTracerGeometry(tracerTemplate);
        if (geometry)
        {
            setComponent(geom::IID_IGeometry, geometry);
            updateFlags(0x1802u, 0);
            geometry->release();
        }
    }
    m_byte1FC = tracer;
}

// , 133 B. The interface owns the control block; this object keeps
// one weak reference at +0x1F0, then flags the geometry/state change on self and descendants.
void GenericProjectile::setAssociatedTracer(IProjectileObject* tracer)
{
    WeakPtr<IProjectileObject> weak = getProjectileWeakPtr(tracer);
    assignProjectileWeakBlock(m_associatedTracerWeak, weak);
    updateFlags(0x800u, 0);
    updateFlagsRecursive(0x800u, 0);
}

// / 0x8284A90: direct forwarding to the attached physics node, neutral zero when absent.
void GenericProjectile::setUnderWater(float depth)
{
    typedef object_detail::LayoutAccess L;
    IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(this));
    if (node)
        node->setUnderWater(depth);
}

float GenericProjectile::getUnderWater()
{
    typedef object_detail::LayoutAccess L;
    IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(this));
    return node ? node->getUnderWater() : 0.0f;
}

// the template's +0x204 scalar (0.5f by default).
float GenericProjectile::getWaterVelocityDecreaseMod() const
{
    return m_template->m_field204;
}

// . The tracer-scale gate is an ordered floating comparison; NaN is
// therefore false. Retail assumes the constructor-supplied template is non-null.
bool GenericProjectile::getIsTracer() const
{
    return m_template->m_tracerScaler > 0.0f;
}

// . This is also Object's live culling/bounds vslot: an active tracer
// publishes its authored visual length, while every other projectile delegates to Object.
float GenericProjectile::getVisualBoundingRadius() const
{
    if (getIsTracer() && m_isActive)
        return m_template->m_maxTracerScaler;
    return Object::getVisualBoundingRadius();
}

// / 0x82878D0. The same byte suppresses initNetworkableComp for local prediction.
void GenericProjectile::setLocalPredict(bool localPredict)
{
    m_localPredict = localPredict;
}

bool GenericProjectile::getLocalPredict() const
{
    return m_localPredict;
}

// Adopts a locally predicted GenericProjectile, destroying any previous stand-in and disabling its self-drive.
// Preserve repeated weak-target reads and separate flag clears (0x100 then 2).
// A null argument only drops the weak reference; it does not destroy the target or stop effects.
void GenericProjectile::setLocalPredictedProjectile(IObject* projectile)
{
    if (!projectile)
    {
        dropWeakBlock(m_localPredictedWeak);         // WeakPtr<IObject>() assignment
        m_localPredictedWeak = 0;
        return;
    }

    // The stand-in this projectile already owns is destroyed outright - see the double-read note.
    if (m_localPredictedWeak
        && static_cast<WeakPtr<IObject>::Ctrl*>(m_localPredictedWeak)->target)
    {
        objectManager->destroyObject(
            m_localPredictedWeak
                ? static_cast<WeakPtr<IObject>::Ctrl*>(m_localPredictedWeak)->target
                : 0);
    }

    // WeakPtr assignment, in this file's raw-block idiom: retain the incoming block, release the
    // held one, store, then drop the reference the returned temporary carried.
    WeakPtr<IObject> incoming = projectile->getWeakPtr();
    if (incoming.m_ctrl)
        ++incoming.m_ctrl->refCount;
    dropWeakBlock(m_localPredictedWeak);
    m_localPredictedWeak = incoming.m_ctrl;
    // ~WeakPtr on `incoming` releases the ref getWeakPtr handed back, leaving the field holding one.

    static_cast<GenericProjectile*>(projectile)->m_byte206 = false;   // see the header block

    stopEffects();
    updateFlags(0, 0x100u);
    updateFlags(0, 2u);
}

// . The bare weak read on +0x1F4; use the actual control
// member so the shipped x86 target +4 becomes +8 on native x64, as its producer requires.
IObject* GenericProjectile::getLocalPredictedProjectile() const
{
    if (!m_localPredictedWeak)
        return 0;
    return static_cast<WeakPtr<IObject>::Ctrl*>(m_localPredictedWeak)->target;
}

// / 0x8287830 and PE 0x5B4C70 / 0x5B4C80. The score system uses this plain
// tick latch to suppress counting the same projectile more than once during a world tick.
void GenericProjectile::setLastCountedHitAtTick(unsigned int tick)
{
    m_dword200 = static_cast<int>(tick);
}

unsigned int GenericProjectile::getLastCountedHitAtTick()
{
    return static_cast<unsigned int>(m_dword200);
}

// / 0x8287880. The launch pipeline clears this byte in activate() and postFire marks
// only authoritative (non-dummy) projectiles for hit accounting.
void GenericProjectile::setCountHit(bool countHit)
{
    m_countHit = countHit;
}

bool GenericProjectile::getCountHit()
{
    return m_countHit;
}

// (14 B) / 0x8287910 (17 B). A plain accessor pair on +0x208 - and the pair that keeps
// GameLogicCollision::serverHandleCollisionForProjectile from playing an impact effect twice for one
// round: it refuses when the current tick is within ten of the stored value, and stamps it on the
// way past.
// THE stored value IS A latch read AS A timestamp. Nothing here validates it, and `activate`
// clears it while the constructor deliberately does not - so a pooled projectile arrives with the
// previous life's tick until it is relaunched.
unsigned int GenericProjectile::getPlayedImpactEffectAtTick() const
{
    return m_playedImpactEffectAtTick;
}

void GenericProjectile::setPlayedImpactEffectAtTick(unsigned int tick)
{
    m_playedImpactEffectAtTick = tick;
}

// Returns true until the template's timeToRemoveTK window expires after launch.
// The exact -1 sentinel disables the window and always allows team kills. Requires a valid template.
bool GenericProjectile::canTK()
{
    if (m_template->m_timeToRemoveTK == -1.0f)
        return true;
    return m_template->m_timeToRemoveTK > (static_cast<float>(WorldPref::getWorldTime()) - m_worldLaunchTime);
}

// Returns damage at point using linear falloff from m_startPos; water multiplies distance by 100.
// Minimum damage >= current damage wins at every range. Preserve repeated getMinDamage calls.
// Ricochet and penetration update m_startPos, restarting the distance measurement.
float GenericProjectile::getDamage(Vec3 point)
{
    const float damage = m_damage;                                       // +0x1C0

    if (m_template->getMinDamage() >= damage)
        return m_template->getMinDamage();

    if (damage > m_template->getMinDamage() && m_template->m_distToStartLoseDamage > 0.0f)
    {
        const float dx = point.x - m_startPos.x;
        const float dy = point.y - m_startPos.y;
        const float dz = point.z - m_startPos.z;
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);

        if (getUnderWater() > 0.0f)                                      // +0x264
            distance = distance * 100.0f;                                // see the header block

        if (distance > m_template->m_distToStartLoseDamage)
        {
            if (distance <= m_template->m_distToMinDamage)
            {
                return damage
                     - (damage - m_template->getMinDamage())
                       / (m_template->m_distToMinDamage - m_template->m_distToStartLoseDamage)
                       * (distance - m_template->m_distToStartLoseDamage);
            }
            return m_template->getMinDamage();
        }
    }
    return damage;
}

// Returns a fresh lifetime sample plus launch time minus current world time, as a float.
// [PLATFORM] This body follows ELF rounding of world time to float and draws a relative sign after
// the sampled value; the PE keeps world-time precision and draws the sign first.
float GenericProjectile::getTimeToLive()
{
    return sampleRandomDistribution(m_template->m_timeToLive)
         + m_worldLaunchTime
         - static_cast<float>(WorldPref::getWorldTime());
}

// Attempts ricochet, allowing the component to update launch time, origin and damage by reference.
// Samples lifetime only when a component exists. A bouncing collision component also reports success
// when the solver declines; message 31 is sent only if neither path bounces.
bool GenericProjectile::ricochet(const Vec3& impactOffset, const Vec3& incomingVelocity,
                                 const Vec3& surfaceNormal, float retainedFraction)
{
    bool bounced = false;

    if (m_ricochetComp)
    {
        const float sampledLifetime = sampleRandomDistribution(m_template->m_timeToLive);

        bounced = m_ricochetComp->ricochet(
            impactOffset, incomingVelocity, surfaceNormal, retainedFraction,
            m_template->m_projectileDamage, sampledLifetime,
            m_exactLaunchTime, m_startPos, m_damage);
        if (bounced)
            return true;
    }

    if (m_collisionComp && m_collisionComp->getIsBouncing())     // ELF +0x48
        return true;

    handleMessage(static_cast<TemplateMessage>(31), 0, false);
    return bounced;
}

// reduceVelocityUnderwater - 0x82855C0, 117 B. for penetrate(), its only caller.
// One multiply of the physics node's speed by the template's waterVelocityDecreaseMod (+0x204,
// default 0.5) - a round that punches into water keeps half its speed.
// THE ELF builds A second copy OF THE speed ON THE stack AND never uses IT (v7[4..6] beside the
// scaled v7[0..2]). Dead stores with no observable effect; not reproduced, recorded here instead.
// It reads the current speed through the node rather than any cached launch velocity, so repeated
// entries compound.
void GenericProjectile::reduceVelocityUnderwater()
{
    IPhysicsNode* const node = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(this));
    if (!node)
        return;

    const Vec3& speed = node->getPositionalSpeed();                  // +0x60
    const float mod = m_template->m_field204;                        // +0x204
    const Vec3 reduced = { speed.x * mod, speed.y * mod, speed.z * mod };
    node->setPositionalSpeed(reduced);                               // +0x68
}

// Attempts penetration, updating launch time, origin and damage through the component.
// Water crossings additionally reposition, change trail/tracer and reduce speed; ordinary penetration returns afterward.
// Preserve both material lookups, including the unguarded lookup when penetration failed.
// Trail choice uses vertical velocity with a dry-name fallback, unlike activate's depth test.
// The original discarded getTimeUntilCanDetonate call remains omitted because its interface slot is undeclared.
bool GenericProjectile::penetrate(IObject* target, const Vec3& offset,
                                  int materialA, int materialB, float retainedFraction)
{
    typedef object_detail::LayoutAccess L;

    bool penetrated = false;

    if (m_penetrateComp)
    {
        const float sampledLifetime = sampleRandomDistribution(m_template->m_timeToLive);

        penetrated = m_penetrateComp->penetrate(
            target, offset, materialA, materialB, retainedFraction,
            m_template->m_projectileDamage, sampledLifetime,
            m_exactLaunchTime, m_startPos, m_damage);
    }

    // unconditional lookup, unguarded dereference - see the header block.
    if (penetrated && static_cast<MaterialManager*>(materialManager)
            ->getMaterialPtr(static_cast<unsigned int>(materialB))->getHasWaterPhysics())
    {
        const Vec3& position = getAbsolutePosition();                     // +0x6C
        const Vec3 exitPoint = { offset.x + position.x,
                                 offset.y + position.y,
                                 offset.z + position.z };
        setAbsolutePosition(exitPoint);                                   // +0x70
    }

    // (the vestigial m_detonationComp getTimeUntilCanDetonate() call sits here - see the block)

    if (!penetrated)
    {
        handleMessage(static_cast<TemplateMessage>(31), 0, false);
        return false;
    }

    // the second lookup, and this one is an exact == 1 test.
    if (static_cast<MaterialManager*>(materialManager)
            ->getMaterialPtr(static_cast<unsigned int>(materialB))->getHasWaterPhysics() != 1)
        return true;

    destroyTrailEffect();

    float verticalSpeed = 0.0f;
    if (IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(this)))
        verticalSpeed = node->getPositionalSpeed().y;                     // +0x60

    // The downward arm falls through to the above-water name when the underwater one is empty;
    // the upward arm never consults the underwater one at all.
    IObjectTemplate* trailTemplate = 0;
    if (verticalSpeed < 0.0f && !m_template->m_string20C.empty())
        trailTemplate = m_template->m_field214;
    else if (!m_template->m_string208.empty())
        trailTemplate = m_template->m_field210;

    if (trailTemplate)
        m_trailEffectObject = trailTemplate->createObject();              // +0x54

    playTrailEffect();

    // the tracer geometry swap - both tracer templates must exist for either arm to run
    if (m_template->m_tracerTemplate && m_template->m_waterTracerTemplate)
    {
        IBase* swapTo = 0;
        if (verticalSpeed < 0.0f
            && !m_template->m_waterTracerTemplateName.empty()
            && m_geometrySwap228 != L::geometry(this))
        {
            swapTo = m_geometrySwap228;
            m_geometrySwap224 = 0;
        }
        else if (!m_template->m_tracerTemplateName.empty()
                 && m_geometrySwap224 != L::geometry(this))
        {
            swapTo = m_geometrySwap224;
            m_geometrySwap228 = 0;
        }

        if (swapTo)
        {
            setComponent(geom::IID_IGeometry, swapTo);                    // +0x58
            updateFlags(6146u, 0);                                        // +0x60
        }
    }

    if (0.0f > verticalSpeed)
        reduceVelocityUnderwater();
    return true;
}

// getProjectileDamage - 0x8282580, 20 B. `return m_template->+0x1DC;`.
// IT returns float, NOT int. The ELF leaves the value on the x87 stack (`fld dword ptr [eax+1DCh]`),
// so the header's `int` was wrong; every caller compares it against 0, which hid the difference.
// this names A template field. GenericProjectileTemplate.h carries +0x1DC as
// "[UNRESOLVED name - getMinDamage's fallback value]". It is the projectile's damage: getMinDamage
// falls back to it when m_minDamage is the -1.0f sentinel, and this getter returns it outright.
float GenericProjectile::getProjectileDamage() const
{
    return m_template->m_projectileDamage;                       // +0x1DC
}

// Direct template forwards used by the TTL/update state machine.
bool GenericProjectile::getDetonateAfterProjectileStopped() const
{
    return m_template->m_detonateAfterProjectileStopped != 0;
}

float GenericProjectile::getDetonateAfterProjectileStoppedMinSpeed() const
{
    return m_template->m_detonateAfterProjectileStoppedMinSpeed;
}

// Both shipped bodies are a constant floating-point zero.
float GenericProjectile::getMinDetonationSpeed() const
{
    return 0.0f;
}

// . The owner weak reference is resolved before consulting the local-player manager.
bool GenericProjectile::isCreatedByLocalPlayer() const
{
    IPlayer* const owner = getPlayer();
    return owner && owner == playerManager->getLocalHumanPlayer();
}

} // namespace world
} // namespace hfe
} // namespace dice
