// Armor state, damage attribution, effects and wreck lifecycle.
// Source path is attested by __FILE__.

#include "ArmorGLComp.h"

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "ArmorGLCompTemplate.h"
#include "DamageEffects.h"
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"
#include "../../../Core/System.h"                       // WorldPref::getTickCount / getWorldTime
#include "../../../Core/EventManager.h"                 // eventManager - setUseDestroyedLod's reset event
#include "../../../Game/Utility/Vars.h"
#include "../../../Physics/HeightmapCluster.h"          // IHeightmapCluster - water level, ground height
#include "../../../Physics/IPhysicsNode.h"              // getIsSleeping / setSleepiness
#include "../../../Physics/PhysicsUtil.h"               // wakeUpObjects
#include "../../../Scene/Object/ObjectManager.h"        // getObjectsFromGrid
#include "../../../Scene/Object/ObjectPredicator.h"     // ObjectInterfacePredicator
#include "../../../Scene/Object/PlayerManager.h"        // getLocalHumanPlayer (ResetStrategicObject's gate)
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../IGame.h"                                // game->destroyObject
#include "../../IGameLogic.h"                           // gameLogic - isServer / getGameStatus / giveDamage
#include "../../Common/GameEvent.h"                     // revive: ECGameLogic 14 replication
#include "../Fx/ParticleTemplate.h"                     // Rand - UpdateInWaterStatus' jitter
#include "../IObject.h"
#include "../IStrategicObjectComp.h"           // ResetStrategicObject's component
#include "../StrategicObjectComp.h"            // ... and the holder it looks up
#include "../IObjectTemplate.h"
#include "../ObjectLayoutAccess.h"                      // the object's cached component slots
#include "../GeometryPartAccess.h"
#include "../../../Geom/GeometryInstanceData.h"
#include "../../../Geom/DecalSystem/DecalObjectAccess.h"
#include "../../../Physics/ICollisionMesh.h"            // setGeometry (slot 13)
#include "../../../Math/Mat4.h"
#include <cstdio>
#include <cstdlib>
#include <float.h>
#include "../../../Core/Debug.h"                        // updateStrategicObject: Debug(6, ..., 1744)
#include "../../../AI/AIInterface/AIInstance.h"          // updateDestroy: the isBotsGame gate
#include "../../../AI/AIInterface/BFKnowledge/IAIObject.h"
#include "../../../IO/Network/INetworkable.h"           // checkIfSpectacularHit: the seed (+0x04)
#include "../../GameLogic/ScoreSystem/ScoreManager.h"    // getTeamDamageLimit
#include "../Game/Soldier.h"                             // hasRagdollLanded
#include "../IPlayer.h"                                  // getWeakPtr / setHealRepairedAtTick / getId
#include "../../Main/Game.h"                             // isDedicated() - playEffectAllowed (BF2.exe 0x440510)
#include "../Fx/EffectBundle.h"                          // overRideTimeToLive - playEffect (BF2.exe 0x642660)

namespace dice {
namespace hfe {

namespace world { extern IBase* heightmapCluster; }     // ds:0x8ABAF24 - Core/Globals.cpp
namespace world { extern IBase* scoreManager; }         // ds:0x8AD1740 - Core/Globals.cpp

namespace world {

// . Despite the name, this searches the ordinary effect queue,
// not the wreck queue. The first case-insensitive match wins; retail has no parallel-vector guard.
bool ArmorGLComp::isSpectacularWreckArmorEffect(const std::string& name)
{
    if (!m_template)
        return false;
    for (int i = 0; i < static_cast<int>(m_template->m_queuedEffectNames.size()); ++i)
    {
        if (equalsIgnoreCase(m_template->m_queuedEffectNames[i], name))
            return m_template->m_queuedEffectFlags[i];
    }
    return false;
}

namespace {
    // This TU's static initialiser (0x821CF00..): the two Vars this class reads.
    float* const g_armorInvunerableTime     = hfe::Vars::getFloat("armor-invunerable-time", 1.0f);          // ds:0x8AAE694 - dice's spelling
    float* const g_armorWaterSafetyCheckTime = hfe::Vars::getFloat("armor-water-safetycheck-time", 2.5f);   // ds:0x8AAE6A0

// Geometry-state access uses setGeomIndex/getGeomCount, distinct from the LOD family.
// PE slots are +0x6C/+0x74; ELF slots are +0x70/+0x78. Geometry records contain LOD records.
#ifdef _MSC_VER
typedef int  (__thiscall *GeometryGetIntFn)(void*);
typedef void (__thiscall *GeometrySetIntFn)(void*, int);
const unsigned int kGeometrySetGeomIndexSlot = 0x6C;
const unsigned int kGeometryGetGeomIndexSlot = 0x70;
const unsigned int kGeometryGetGeomCountSlot = 0x74;
#else
typedef int  (*GeometryGetIntFn)(void*);
typedef void (*GeometrySetIntFn)(void*, int);
const unsigned int kGeometrySetGeomIndexSlot = 0x70;
const unsigned int kGeometryGetGeomIndexSlot = 0x74;
const unsigned int kGeometryGetGeomCountSlot = 0x78;
#endif

int geometryGetGeomCount(void* geometry)
{
    if (!geometry)
        return 0;
    void** const vtable = *reinterpret_cast<void***>(geometry);
    return reinterpret_cast<GeometryGetIntFn>(
        vtable[kGeometryGetGeomCountSlot / 4])(geometry);
}

int geometryGetPartCount(void* geometry)
{
    return geometry_part::count(geometry);
}

// Initializes damage visibility and fade on a valid geometry component.
// The two fade fields are ELF +0x10/+0x14 and PE +0x28/+0x2C because string layouts differ.
// Writes 1 to the first field, then passes the second field to setFade; neither helper null-checks.
#ifdef _MSC_VER
const unsigned int kGeometryShowDamageSlot = 0xC0;
typedef void (__thiscall *GeometryShowDamageFn)(void*, int, int);
#else
const unsigned int kGeometryShowDamageSlot = 0xC4;
typedef void (*GeometryShowDamageFn)(void*, int, int);
#endif

void geometryInitDamageState(void* geometry)
{
    if (!geometry)
        return;

    geom::GeometryInstanceData* const instance = static_cast<geom::GeometryInstanceData*>(geometry);
    instance->m_float28 = 1.0f;
    instance->setOpacity(instance->m_float2C);
}

void geometryShowDamage(void* geometry, int a, int b)
{
    if (!geometry)
        return;

    void** const vtable = *reinterpret_cast<void***>(geometry);
    reinterpret_cast<GeometryShowDamageFn>(
        vtable[kGeometryShowDamageSlot / 4])(geometry, a, b);
}

void geometrySetPartTransform(void* geometry, const Mat4* transform, int part)
{
    geometry_part::setTransform(geometry, transform, part);
}


}

// Initializes armor from its template and caches getStatus after the hit-point/template stores.
// Leaves m_object and unused ring/effect slots uninitialized; attach/init must precede their use.
ArmorGLComp::ArmorGLComp(ArmorGLCompTemplate* objectTemplate)
    : m_refCount(1)
    , m_template(objectTemplate)
    , m_hitPoints(objectTemplate->m_hitPoints)          // template +0xA4
    , m_sendingMessage(false)
    , m_destroyed(false)
    , m_criticallyDamaged(false)
    , m_lastCollisionHeight(-1.0f)
    , m_lastHitMaterialIndex(-1)
    , m_lastGroundHitMaterialIndex(-1)
    , m_lastHitCanTK(true)
    , m_lastHitBoneIndex(-1)
    , m_inDeepWater(false)
    , m_wasInWater(false)
    , m_collisionWithWater(false)
    , m_byte57(false)
    , m_byte58(false)
    , m_byte59(false)
    , m_byte5A(false)
    , m_status(0)
    , m_wreckTime(0.0f)
    , m_destroyedTime(-1.0f)
    , m_colHead(0)
    , m_colTail(0)
    , m_criticalDamageTimer(0.0f)
    , m_deepWaterDamageTimer(0.0f)
    , m_waterDamageTimer(0.0f)
    , m_effectCount(0)
    , m_effects(0)
    , m_lastDamageEffects(0)
    , m_byte188(false)
    , m_wreckEffectCount(0)
    , m_wreckEffects(0)
    , m_dword214(0)
    , m_byte218(false)
    , m_lastDamageType(0)
    , m_invulnerableUntilTick(0)
    , m_byte238(false)
    , m_pendingDestroy(false)
    , m_previousHitPoints(objectTemplate->m_hitPoints)  // template +0xA4, read a second time
    , m_upSideDownTimer(0.0f)
    , m_upSideDownLatch(false)
    , m_accumulatedDamageNorm(0.0f)
    , m_healAccumulated(0)
    , m_lastWaterCheckTime(0.0f)
{
    m_lastHitPlace.x = m_lastHitPlace.y = m_lastHitPlace.z = 0.0f;
    m_lastHitDirection.x = m_lastHitDirection.y = m_lastHitDirection.z = 0.0f;
    m_status = getStatus();                             // the ctor's last act - `mov [edi+5Ch], eax`
}

// Stops attached effects before destroying containers and owned HandledCollision entries.
// A component with no effects can be destroyed before attachment without reading m_object.
ArmorGLComp::~ArmorGLComp()
{
    StopAllEffects();
    for (std::map<int, std::vector<HandledCollision*>*>::iterator it = m_handledCollisions.begin();
         it != m_handledCollisions.end(); ++it)
    {
        std::vector<HandledCollision*>* list = it->second;
        if (!list)
            continue;
        for (unsigned int i = 0; i < list->size(); ++i)
            delete (*list)[i];
        delete list;
    }
}

// IBase + IObjectComponent - slots 0..13. release() dispatches the deleting destructor (slot 73)
// through the vtable and tests `this` after decrementing through it, as shipped.
void ArmorGLComp::addRef()  { ++m_refCount; }
int  ArmorGLComp::getRef()  { return m_refCount; }             // reads +0x04, NOT +0x08

int ArmorGLComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* ArmorGLComp::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectComponent || iid == IID_IArmorGLComp)
        return const_cast<ArmorGLComp*>(this);
    return 0;
}

unsigned int ArmorGLComp::getInterfaceID() const  { return IID_IArmorGLComp; }

// Attaches the object, starts spawn protection and restores intact geometry/collision.
// Borrows the template's effect maps, initializes available geometry damage state and resets the strategic holder.
void ArmorGLComp::init(IObject* object)
{
    m_object = object;

    const unsigned int tickCount = WorldPref::getTickCount();
    const float protectTicks = 30.0f * *g_armorInvunerableTime;
    m_invulnerableUntilTick = tickCount + static_cast<int>(protectTicks);       // cvttss2si

    setUseDestroyedLod(false, true);

    m_wreckEffects = &m_template->m_wreckDamageEffects;   // template +0x1C
    m_effects      = &m_template->m_damageEffects;        // template +0x10

    IObject* root = getRootParent(m_object);
    if (root)
    {
        void* geometry = object_detail::LayoutAccess::geometry(root);          // Object +0x38
        if (geometry)
        {
            geometryInitDamageState(geometry);
            if (m_hitPoints > m_template->m_showDamageAsDirt)                   // template +0x9C
                geometryShowDamage(geometry, 1, 0);
        }
    }

    ResetStrategicObject();
}

// On ordinary destruction during game status 1, lowers hit points to the wreck floor and plays effects.
// Force-destroy skips this transition. Preserve the second playEffect call after setHitPoints.
void ArmorGLComp::destroy()
{
    if (!IObject::useForceDestroy && (!gameLogic || gameLogic->getGameStatus() == 1))
    {
        setHitPoints(-m_template->m_wreckHitPoints);           // xor 0x80000000
        playEffect();
    }
}

void      ArmorGLComp::setObject(IObject* object) { m_object = object; }
IObject*  ArmorGLComp::getObject() const          { return m_object; }

// Returns false if (other, first) is already recorded for materialId; otherwise records the contact
// with a one-second timeout, updates last-hit material and returns true to permit damage dispatch.
bool ArmorGLComp::collision(int first, int materialId, IObject* other)
{
    std::map<int, std::vector<HandledCollision*>*>::iterator it = m_handledCollisions.find(materialId);
    HandledCollision* record;
    if (it == m_handledCollisions.end())
    {
        std::vector<HandledCollision*>* list = new std::vector<HandledCollision*>();
        m_handledCollisions.insert(std::make_pair(materialId, list));
        record = new HandledCollision;
        record->object = other;
        record->timer = 1.0f;
        record->first = first;
        m_handledCollisions[materialId]->push_back(record);
    }
    else
    {
        std::vector<HandledCollision*>* list = it->second;
        for (unsigned int i = 0; i < list->size(); ++i)
        {
            HandledCollision* h = (*list)[i];
            if (h->object == other && h->first == first)
                return false;
        }
        record = new HandledCollision;
        record->object = other;
        record->timer = 1.0f;
        record->first = first;
        m_handledCollisions[materialId]->push_back(record);
    }
    setLastHitMaterialIndex(materialId);
    return true;
}
bool      ArmorGLComp::hasUpdate() const          { return true; }   // `mov eax, 1`

// Advances wreck/destruction, server damage timers, collision history and damage attribution by deltaTime.
// Pending destruction is handled first and ends the tick. Ring expiry carries overshoot into the next entry.
void ArmorGLComp::handleUpdate(float deltaTime)
{
    if (m_pendingDestroy)
    {
        playEffect();
        destroyOrResetObject();
        m_pendingDestroy = false;
        return;
    }

    if (gameLogic->isServer())
        UpdateInWaterStatus();

    if (m_destroyed)
    {
        if (m_hitPoints <= 0.0f)
        {
            const ArmorGLCompTemplate* t = m_template;
            if (t->m_canBeDestroyed && t->m_timeToStayAsWreck != -1.0f)
                updateDestroy();
        }
    }
    else if (gameLogic->isServer())
    {
        m_criticalDamageTimer += deltaTime;
        if (m_criticalDamageTimer >= 1.0f)
        {
            const ArmorGLCompTemplate* t = m_template;
            if (!(t->m_criticalDamage < m_hitPoints) && t->m_hpLostWhileCriticalDamage != 0.0f)
            {
                Vec3 position; position.x = position.y = position.z = 0.0f;
                Vec3 direction = m_lastHitDirection;
                const float amount = m_criticalDamageTimer * t->m_hpLostWhileCriticalDamage;
                gameLogic->giveDamage(m_object, amount, 0, 0, 0, -1, 0, 0,
                                      position, direction, -1, false, true, false);
            }
            m_criticalDamageTimer = 0.0f;
        }
        UpdateUpSideDownDamage(deltaTime);
        UpdateWaterDamage(deltaTime);
    }

    // 4. the ring
    if (m_colHead != m_colTail)
    {
        float remaining = deltaTime;
        for (;;)
        {
            const float value = m_colValues[m_colHead] - remaining;
            m_colValues[m_colHead] = value;
            if (0.0f <= value)
                break;
            m_colHead = (m_colHead + 1) & 0xF;
            if (m_colHead == m_colTail)
                break;
            remaining = -value;                          // the overshoot carries into the next slot
        }
    }

    // 5. the height watermark
    if (!m_byte57 && m_object->getAbsolutePosition().y > m_lastCollisionHeight)
        m_lastCollisionHeight = m_object->getAbsolutePosition().y;
    m_byte57 = false;

    // 6. the attribution timeout
    m_lastHitTimeout -= deltaTime;
    if (m_lastHitTimeout < 0.0f)
    {
        m_lastHitTimeout = 0.0f;
        m_lastHitPlayer = WeakPtr<IPlayer>();
        m_lastHitCanTK = true;
    }

    m_byte238 = false;

    // 8. the handled-collision sweep
    for (std::map<int, std::vector<HandledCollision*>*>::iterator it = m_handledCollisions.begin();
         it != m_handledCollisions.end(); ++it)
    {
        std::vector<HandledCollision*>* list = it->second;
        for (unsigned int i = 0; i < list->size(); )
        {
            HandledCollision* hc = (*list)[i];
            hc->timer -= deltaTime;
            if (hc->timer < 0.0f)
            {
                list->erase(list->begin() + i);
                delete hc;
            }
            else
                ++i;
        }
    }
}

// one message, 39, and it re-applies the destroyed LOD/collision.
void ArmorGLComp::handleMessage(TemplateMessage message, IPlayer*, bool)
{
    if (message == static_cast<TemplateMessage>(39))
        setUseDestroyedLod(true, false);
}

// Hit points, status.

// . Delegates to _setHitPoints, then unconditionally clears +0x25C - the asymmetry
// against damage() (which calls _setHitPoints directly) is deliberate; see the header block.
void ArmorGLComp::setHitPoints(float hitPoints)
{
    _setHitPoints(hitPoints);                            // Applies clamping and state transition.
    m_byte25C = false;
}

float ArmorGLComp::getHitPoints()          { return m_hitPoints; }        // raw, no clamp
bool  ArmorGLComp::isDestroyed()           { return m_destroyed; }        // a stored flag
bool  ArmorGLComp::isCriticalDamaged()     { return m_criticallyDamaged; }
bool  ArmorGLComp::isSendingMessage()      { return m_sendingMessage; }

// 0 alive; 1 a wreck (hit points at or below zero); 2 gone (at or below the wreck
// floor -wreckHitPoints). Also what the constructor caches into m_status.
int ArmorGLComp::getStatus() const
{
    if (m_hitPoints <= 0.0f)
        return (m_hitPoints <= -m_template->m_wreckHitPoints) + 1;
    return 0;
}

bool ArmorGLComp::collision()              { return m_byte57; }           // [INFERRED field:
                                                                          // the byte the tick clears]

// The collision ring - +0x68..+0xEF, 16 object slots and 16 parallel floats, head/tail cursors that
// wrap with `& 0xF`. addColObject stores 1 - SUM(live values); clear is O(1) on the cursors.
void ArmorGLComp::addColObject(IObject* object)
{
    float sum = 0.0f;
    for (int i = m_colHead; i != m_colTail; i = (i + 1) & 0xF)
        sum += m_colValues[i];
    m_colObjects[m_colTail] = object;
    m_colValues[m_colTail]  = 1.0f - sum;
    m_colTail = (m_colTail + 1) & 0xF;
}
IObject** ArmorGLComp::getColObjectList()  { return m_colObjects; }        // `add eax, 68h`
void      ArmorGLComp::clearColObjectList(){ m_colHead = 0; m_colTail = 0; }   // cursors only
bool ArmorGLComp::isInColList(IObject* object)
{
    for (int i = m_colHead; i != m_colTail; i = (i + 1) & 0xF)
        if (m_colObjects[i] == object)
            return true;
    return false;
}

// Template pass-throughs. The four force/explosion ones answer for the wreck when m_destroyed.
float ArmorGLComp::getAngleMod()           { return m_template->m_angleMod; }
float ArmorGLComp::getAttackDamage()       { return m_template->m_attackDamage; }
bool  ArmorGLComp::getDamageFromWater()    { return m_template->m_hpLostWhileInWater > 0.0f; }
float ArmorGLComp::getMaxHitPoints()       { return m_template->m_maxHitPoints; }
float ArmorGLComp::getMaxWreckHitPoints()  { return m_template->m_maxWreckHitPoints; }
float ArmorGLComp::getSpeedMod()           { return m_template->m_speedMod; }
float ArmorGLComp::getExplosionForceMod()  { return m_destroyed ? m_template->m_wreckExplosionForceMod : m_template->m_explosionForceMod; }
float ArmorGLComp::getExplosionForceMax()  { return m_destroyed ? m_template->m_wreckExplosionForceMax : m_template->m_explosionForceMax; }
bool  ArmorGLComp::getCollisionWithWater() { return m_collisionWithWater; }
int   ArmorGLComp::getDefaultMaterial() const { return m_template->m_defaultMaterial; }

// The "last hit" record. Plain stores and loads except where noted.
void  ArmorGLComp::setLastCollisionHeight(float height) { m_lastCollisionHeight = height; }
float ArmorGLComp::getLastCollisionHeight()             { return m_lastCollisionHeight; }

// ignored on a wreck; indices up to 15 are ground materials and also update the ground index;
// index 1 is water and raises the was-in-water latch.
void ArmorGLComp::setLastHitMaterialIndex(int index)
{
    if (m_destroyed)
        return;
    m_lastHitMaterialIndex = index;
    if (index <= 15)
        m_lastGroundHitMaterialIndex = index;
    if (index == 1)
        m_wasInWater = true;
}
int   ArmorGLComp::getLastHitMaterialIndex()       { return m_lastHitMaterialIndex; }
int   ArmorGLComp::getLastGroundHitMaterialIndex() { return m_lastGroundHitMaterialIndex; }

// taken when forced, or on the server.
void ArmorGLComp::setLastHitBoneIndex(int index, bool force)
{
    if (force || gameLogic->isServer())
        m_lastHitBoneIndex = index;
}
int  ArmorGLComp::getLastHitBoneIndex() const   { return m_lastHitBoneIndex; }
const Vec3& ArmorGLComp::getLastHitDirection() const { return m_lastHitDirection; }   // by address (+0x48)
void ArmorGLComp::setLastHitPlace(Vec3 place)   { m_lastHitPlace = place; }
const Vec3& ArmorGLComp::getLastHitPlace()      { return m_lastHitPlace; }              // by address (+0x30)
IPlayer* ArmorGLComp::getLastHitPlayer()        { return m_lastHitPlayer.get(); }       // block ? block->target : 0
void  ArmorGLComp::setLastHitCanTK(bool canTK)  { m_lastHitCanTK = canTK; }
bool  ArmorGLComp::getLastHitCanTK()            { return m_lastHitCanTK; }
IObject* ArmorGLComp::getLastHitWeapon()        { return m_lastHitWeapon.get(); }
IObject* ArmorGLComp::getLastHitVehicle()       { return m_lastHitVehicle.get(); }
void  ArmorGLComp::setLastDamageType(int type)  { m_lastDamageType = type; }
int   ArmorGLComp::getLastDamageType()          { return m_lastDamageType; }

std::list<DamageFromPlayer>*  ArmorGLComp::getDamageFromPlayers()        { return &m_damageFromPlayers; }
std::list<DamageFromPlayer>*  ArmorGLComp::getVehicleDamageFromPlayers() { return &m_vehicleDamageFromPlayers; }
std::list<WeakPtr<IPlayer> >* ArmorGLComp::getTeamDamagePlayers()        { return &m_teamDamagePlayers; }

void  ArmorGLComp::accumulateDamageNorm(float amount) { m_accumulatedDamageNorm += amount; }
float ArmorGLComp::getAccumulatedDamageNorm() const   { return m_accumulatedDamageNorm; }
void  ArmorGLComp::resetAccumulatedDamageNorm()       { m_accumulatedDamageNorm = 0.0f; }   // an integer zero store, same bits
void  ArmorGLComp::ClearEffects()                     { m_effects = 0; }                    // only the intact map

// isInvunerable - 0x821C070, 117 B - dice's spelling. BF2's spawn protection, and one more rule:
// projectiles in flight cannot be damaged (the object, or failing that its root, being a
// GenericProjectileTemplate). The tick comparison is unsigned (`setnbe`) - across a tick-counter
// wrap it misbehaves; as shipped.
bool ArmorGLComp::isInvunerable()
{
    IObject* object = m_object;
    IObject* root   = getRootParent(object);
    if (object)
    {
        if (object->getTemplate()->getClassID() == CID_GenericProjectileTemplate)
            return true;
    }
    else if (root)
    {
        if (root->getTemplate()->getClassID() == CID_GenericProjectileTemplate)
            return true;
    }
    return m_invulnerableUntilTick > static_cast<unsigned int>(WorldPref::getTickCount());
}

// The tick's three per-frame callees, and their helpers.

// Updates water contact immediately while wet, otherwise on a jittered safety-check interval.
// Sleeping or wrecked objects return to the slow cadence. The water-contact branch requires a physics node;
// the earlier null test only clears the wet latch.
void ArmorGLComp::UpdateInWaterStatus()
{
    IObject* object = m_object;
    IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(object));   // +0x44
    if (!node || node->getIsSleeping() || m_destroyed)                       // slot +0x11C
        m_wasInWater = false;

    const Vec3 pos = object->getAbsolutePosition();
    bool timerFired = false;
    if (!m_wasInWater)
    {
        const float worldTime = static_cast<float>(WorldPref::getWorldTime());
        if (worldTime > *g_armorWaterSafetyCheckTime + m_lastWaterCheckTime)
        {
            timerFired = true;
            const unsigned int x = Rand.getRaw();                        // two Lehmer steps, inlined in the ELF
            const float roll = static_cast<float>(((static_cast<int>(x) >> 7) | 1) + 1) * 5.9604645e-08f;
            m_lastWaterCheckTime = worldTime - (roll * 0.30000001f + 0.0f);
        }
    }

    m_collisionWithWater = false;
    m_inDeepWater = false;
    if (m_wasInWater || timerFired)
    {
        const float water = static_cast<IHeightmapCluster*>(heightmapCluster)
                                ->getWaterLevelInWorldCoords(pos.x, pos.z);   // slot +0x98
        if (water > pos.y + m_template->m_waterLevel)                        // template +0x108
        {
            m_collisionWithWater = true;
            if (water > pos.y + m_template->m_deepWaterLevel)                // template +0x10C
                m_inDeepWater = true;
            node->setSleepiness(40);                                         // slot +0x124 - see the note
            m_wasInWater = true;
        }
    }
}

// UpdateWaterDamage - 0x821B550, 549 B, read in full. tick-based, not continuous: while in water
// (and the template loses hit points in water) a countdown runs by dt and each time it crosses zero
// one hpLostWhileInWater is applied through giveDamage and the countdown reloads with 1.0 s. Out of
// water the countdown sits at waterDamageDelay, so the first hit comes after the delay and later
// hits come every second. The deep-water pair mirrors it on its own countdown.
void ArmorGLComp::UpdateWaterDamage(float deltaTime)
{
    const ArmorGLCompTemplate* t = m_template;

    if (m_collisionWithWater && t->m_hpLostWhileInWater > 0.0f)
    {
        m_waterDamageTimer -= deltaTime;
        if (m_waterDamageTimer < 0.0f)
        {
            Vec3 position; position.x = position.y = position.z = 0.0f;
            Vec3 direction = m_lastHitDirection;
            gameLogic->giveDamage(m_object, t->m_hpLostWhileInWater, 0, 0, 0, -1, 0, 0,
                                  position, direction, -1, false, true, false);
            m_waterDamageTimer = 1.0f;
        }
    }
    else
    {
        m_waterDamageTimer = t->m_waterDamageDelay;
    }

    if (m_inDeepWater && t->m_hpLostWhileInDeepWater > 0.0f)
    {
        m_deepWaterDamageTimer -= deltaTime;
        if (m_deepWaterDamageTimer >= 0.0f)
            return;
        Vec3 position; position.x = position.y = position.z = 0.0f;
        Vec3 direction = m_lastHitDirection;
        gameLogic->giveDamage(m_object, t->m_hpLostWhileInDeepWater, 0, 0, 0, -1, 0, 0,
                              position, direction, -1, false, true, false);
        m_deepWaterDamageTimer = 1.0f;
    }
    else
    {
        m_deepWaterDamageTimer = t->m_deepWaterDamageDelay;
    }
}

// UpdateUpSideDownDamage - 0x821B280, 711 B, read in full. Once a second (its own countdown reloads
// with 1.0), if the template loses more than 0.01 hp upside down: the object counts as upside down
// when it is close TO ground (below) and its up axis' Y is under 0.3 (~72 degrees of tilt) - so a
// flipped vehicle in mid-air takes nothing.
// THE first second IS free. m_upSideDownLatch is set every upright tick; the first upside-down
// tick only clears it, and damage starts on the second. Righting the vehicle re-arms the grace.
// The damage tick also pokes the root's physics node awake (setSleepiness 40).
void ArmorGLComp::UpdateUpSideDownDamage(float deltaTime)
{
    m_upSideDownTimer -= deltaTime;
    if (m_upSideDownTimer >= 0.0f)
        return;
    m_upSideDownTimer = 1.0f;

    const ArmorGLCompTemplate* t = m_template;
    if (!(t->m_hpLostWhileUpSideDown > 0.0099999998f))                        // template +0xF4
        return;

    Vec3 normal; normal.x = normal.y = normal.z = 0.0f;
    const Mat4 transform = m_object->getAbsoluteTransformation();            // copied, as shipped
    if (isCloseToGround(normal)
        && transform.m[5] * normal.y + transform.m[4] * normal.x + transform.m[6] * normal.z < 0.30000001f)
    {
        if (m_upSideDownLatch)
        {
            m_upSideDownLatch = false;
        }
        else
        {
            IPhysicsNode* node =
                static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(getRootParent(m_object)));
            if (node)
                node->setSleepiness(40);
            Vec3 direction = m_lastHitDirection;
            Vec3 position; position.x = position.y = position.z = 0.0f;
            gameLogic->giveDamage(m_object, t->m_hpLostWhileUpSideDown, 0, 0, 0, -1, 0, 0,
                                  position, direction, -1, false, true, false);
        }
    }
    else
    {
        m_upSideDownLatch = true;
    }
}

// Returns whether water, terrain or a downward object ray lies within the ground-test range.
// On success normal is always (0,1,0), replacing any sampled terrain normal.
bool ArmorGLComp::isCloseToGround(Vec3& normal)
{
    const Vec3 pos = m_object->getAbsolutePosition();
    const float radius = m_object->getPhysicBoundingRadius();
    IHeightmapCluster* cluster = static_cast<IHeightmapCluster*>(heightmapCluster);

    unsigned char material = 0;
    if (cluster->getWaterLevelInWorldCoords(pos.x, pos.z) > pos.y - radius
        || cluster->getHeightAndNormalInWorldCoords(pos.x, pos.z, normal, material) > pos.y - radius)
    {
        normal.x = 0.0f; normal.y = 1.0f; normal.z = 0.0f;
        return true;
    }

    Vec3 from = pos;
    Vec3 to = pos; to.y = pos.y - (radius + radius);
    if (!checkIfRayHitAnyObject(from, to, m_object, true))
        return false;
    normal.x = 0.0f; normal.y = 1.0f; normal.z = 0.0f;
    return true;
}

// LOD / collision state, and the strategic-object hook.

// . Not for soldiers. Walk the whole root tree twice - LODs then collision - and,
// when both flags are clear (the "just reset" call updateStatus makes), tell the event manager a
// destroyable object came back (event 3/18 with the object's +0x34 [UNRESOLVED] word).
void ArmorGLComp::setUseDestroyedLod(bool useDestroyed, bool fromInit)
{
    if (m_object->getTemplate()->getClassID() == CID_SoldierTemplate)
        return;
    IObject* root = getRootParent(m_object);
    setUseDestroyedLodRecursive(root, useDestroyed);
    setUseDestroyedCollision(root, useDestroyed);
    if (!useDestroyed && !fromInit)
    {
        // `v8 = *result->gap34` at 0x821D0??: a word at m_object +0x34, tested and passed as the
        // event's payload; and `[[+0x14]+0x10]` = the object's template's getClassID.
        IObject* object = m_object;
        void* payload = object_detail::LayoutAccess::aiObject(object);      // IObject +0x34
        if (payload && object->getTemplate()->getClassID() == CID_DestroyableObjectTemplate)
            eventManager->postEvent(static_cast<EventCategory>(3), 18u, payload, 0.0f);   // ELF +0x14
    }
}

// , read in full. Recursive over children (child, then each sibling). For the
// owning object only: poke its decal object and, when going destroyed, wake everything within its
// physics radius. For every object with a geometry: switch to LOD 1 / LOD 0 when it has more than
// one, else set/clear flag bit 2; and on a spectacular death that hides children, zero every part
// transform but the first. Geometry slots cross-checked against client PE 0x685770:
// count +0x5C at 0x68582E/0x685856; set pointer +0x4C at 0x685846, not palette +0x54.
void ArmorGLComp::setUseDestroyedLodRecursive(IObject* object, bool useDestroyed)
{
    if (!object)
        return;

    if (getObject() == object)
    {
        if (object->getDecalObject())
            geom::decal_object::clear(object->getDecalObject());
        if (useDestroyed)
            wakeUpObjects(object->getAbsolutePosition(), object->getPhysicBoundingRadius(), object, true);
    }

    void* geometry = object_detail::LayoutAccess::geometry(object);          // +0x38
    if (geometry)
    {
        if (useDestroyed)
        {
            if (geometryGetGeomCount(geometry) > 1) object->SetActiveLod(static_cast<ObjectLodType>(1));
            else                                   object->updateFlags(0, 2);
        }
        else
        {
            if (geometryGetGeomCount(geometry) > 1) object->SetActiveLod(static_cast<ObjectLodType>(0));
            else                                   object->updateFlags(2, 0);
        }
    }

    if (useDestroyed && geometry && m_byte238 && m_template->m_hideChildrenOnSpectacularDeath)
    {
        for (int i = 1; i < geometryGetPartCount(geometry); ++i)
            geometrySetPartTransform(geometry, &Mat4::getZero(), i);
    }

    for (IObject* child = object->getChild(); child; child = child->getSibling())
        setUseDestroyedLodRecursive(child, useDestroyed);
}

// Recursively switches collision geometry and mirrors the result into flag 0x100.
// For repairable wrecks/buildings, lifts nearby soldiers onto the changed mesh using a two-unit downward ray
// at LOD 2, then resets their physics. A missed ray leaves the soldier in place.
// The optional CollisionFaceInfo output is unused here.
void ArmorGLComp::setUseDestroyedCollision(IObject* object, bool useDestroyed)
{
    if (!object)
        return;

    ICollisionMesh* mesh = static_cast<ICollisionMesh*>(object_detail::LayoutAccess::collisionMesh(object));   // +0x40
    if (mesh)
    {
        if (mesh->setGeometry(useDestroyed))                                 // slot 13
            object->updateFlags(0x100, 0);
        else
            object->updateFlags(0, 0x100);

        const bool destroyable = object->getTemplate()->getClassID() == CID_DestroyableObjectTemplate;
        if ((m_template->m_canBeRepairedWhenWreck && (object_detail::LayoutAccess::flags(m_object) & 8u) == 0)
            || destroyable)
        {
            ObjectInterfacePredicator predicator(0x100u, IID_ISoldier);
            std::vector<IObject*> soldiers;
            const float radius = object->getPhysicBoundingRadius();
            objectManager->getObjectsFromGrid(object->getAbsolutePosition(), radius, soldiers, &predicator);
            for (unsigned int i = 0; i < soldiers.size(); ++i)
            {
                IObject* const soldier = soldiers[i];
                const Vec3& soldierPosition = soldier->getAbsolutePosition();          // +0x6C

                Vec3 offset;  offset.x = 0.0f; offset.y =  1.0f; offset.z = 0.0f;
                Vec3 from;
                from.x = soldierPosition.x + offset.x;
                from.y = offset.y + soldierPosition.y;
                from.z = soldierPosition.z + offset.z;
                // a delta, not an end point - two units straight down.
                Vec3 delta;   delta.x  = 0.0f; delta.y  = -2.0f; delta.z  = 0.0f;

                Vec3 hitNormal;   hitNormal.x = 0.0f;   hitNormal.y = 0.0f;   hitNormal.z = 0.0f;
                Vec3 hitPosition; hitPosition.x = 0.0f; hitPosition.y = 0.0f; hitPosition.z = 0.0f;
                float planeDist;                     // uninitialised in the image - pure outs
                float along;
                int   material = 1;                  // pre-set to 1, as shipped

                // the CollisionFaceInfo out is a stack local the image never reads back; null
                // here, the same way checkIfObjectIntersects passes it.
                if (mesh->getDistancePeek(2, false,
                                          object->getAbsoluteTransformation(),      // slot 33 +0x84
                                          object->getInvAbsoluteTransformation(),   // slot 34 +0x88
                                          from, delta, hitNormal, hitPosition,
                                          planeDist, along, &material, 0,
                                          true, false))                             // slot 24 +0x60
                {
                    Vec3 lift;  lift.x = 0.0f; lift.y = 1.0f; lift.z = 0.0f;
                    Vec3 pushed;
                    pushed.x = hitPosition.x + lift.x;
                    pushed.y = lift.y + hitPosition.y;
                    pushed.z = hitPosition.z + lift.z;
                    soldier->setAbsolutePosition(pushed);                            // slot 28 +0x70

                    IPhysicsNode* const node = static_cast<IPhysicsNode*>(
                        object_detail::LayoutAccess::physicsNode(soldier));           // raw +0x44
                    if (node)
                        node->reset(true);                                            // +0x118
                }
            }
        }
    }

    setUseDestroyedCollision(object->getChild(), useDestroyed);
    setUseDestroyedCollision(object->getSibling(), useDestroyed);
}

// Clears the strategic holder's destroyed-piece count when a local human and component exist.
// The lookup key combines the object's name and holder index. A missing holder returns silently.
void ArmorGLComp::ResetStrategicObject()
{
    if (!playerManager->getLocalHumanPlayer())
        return;

    // Both arguments are the same id, which is what makes Object::queryComponent hand back its
    // cached +0x74 pointer directly instead of routing through queryInterface.
    IStrategicObjectComp* strategicObject = static_cast<IStrategicObjectComp*>(
        m_object->queryComponent(IID_IStrategicObjectComp, IID_IStrategicObjectComp));
    if (!strategicObject)
        return;

    // TWO strings, NOT ONE, and that is what the ELF builds: a copy of the name the component
    // returns by reference, then `name + digit` - whose libstdc++ operator+ is itself a copy plus
    // an append(1, c). Reproduced rather than folded into one append, because folding would drop a
    // copy the shipped code makes.
    const std::string name = strategicObject->getStrategicObjectName();          // slot +0x38
    const std::string key  = name + static_cast<char>(strategicObject->getIndex() + '0');   // +0x50

    std::map<std::string, StrategicObjectHolder*>* holders =
        gameLogic->getStrategicObjectMap();
    std::map<std::string, StrategicObjectHolder*>::iterator it = holders->find(key);
    if (it != holders->end())
        it->second->setNrDestroyedObjects(0);          // holder +0x10
}

// . Anything that is not a soldier: destroy it. A soldier: destroy it unless it is
// a PlayerControlObject with an occupying player - in which case nothing is done here (the reset
// happens on the player's death path).
void ArmorGLComp::destroyOrResetObject()
{
    if (m_object->getTemplate()->getClassID() != CID_SoldierTemplate)
    {
        game->destroyObject(m_object);
        return;
    }
    IPlayerControlObject* pco =
        static_cast<IPlayerControlObject*>(m_object->queryInterface(IID_IPlayerControlObject));
    if (!pco || !pco->getOccupingPlayer())                                   // slot +0x10
        game->destroyObject(m_object);
}

// . For each spawned effect id, find the child of the owning object with that id
// (walking the sibling chain) and force-destroy it - IObject slot +0x34 is destroy(bool), called
// with true. "Stop" means "remove" for a spawned effect object.
void ArmorGLComp::StopAllEffects()
{
    for (int i = 0; i < m_effectCount; ++i)
    {
        const unsigned int id = m_effectIds[i];
        IObject* found = 0;
        for (IObject* child = m_object->getChild(); child; child = child->getSibling())
        {
            if (child->getObjectId() == id)                                  // +0x10
            {
                found = child;
                break;
            }
        }
        if (found)
            found->destroy(true);                                        // IObject slot +0x34 = destroy(bool), arg 1
    }
}

// Restores full-health state, clears damage history and rearms spawn protection.
void ArmorGLComp::reset()
{
    m_lastHitPlayer = WeakPtr<IPlayer>();
    m_lastHitMaterialIndex = -1;
    m_effectCount = 0;
    m_lastCollisionHeight = -1.0f;
    m_criticalDamageTimer = 0.0f;
    m_waterDamageTimer = 0.0f;
    m_deepWaterDamageTimer = 0.0f;
    m_lastGroundHitMaterialIndex = -1;
    m_byte57 = false;
    m_wasInWater = false;
    m_collisionWithWater = false;
    m_inDeepWater = false;
    m_lastHitCanTK = true;
    updateStatus(m_template->m_maxHitPoints, true);
    m_wreckTime = 0.0f;
    m_destroyedTime = -1.0f;
    m_hitPoints = m_template->m_maxHitPoints;
    m_healAccumulated = 0;
    m_damageFromPlayers.clear();
    m_teamDamagePlayers.clear();
    m_byte25C = true;
    m_lastWaterCheckTime = 0.0f;
    m_invulnerableUntilTick = WorldPref::getTickCount()
                            + static_cast<int>(30.0f * *g_armorInvunerableTime);
}


// Armor damage-state settings. phy-wreck usage remains [UNRESOLVED].
namespace {
    float* const g_armorInvunerableTimeAfterWreck = hfe::Vars::getFloat("armor-invunerable-time-afterwreck", 1.0f);   // ds:0x8AAE698
    float* const g_armorMaxSoldierWreckTime       = hfe::Vars::getFloat("armor-max-soldier-wrecktime", 60.0f);        // ds:0x8AAE69C

    // the debug byte calcDamage / damage / GenericFireArm read and nothing writes.
    const bool g_damageDebugLocalPlayer = false;

    // Allows client effects unless force-destroying, dedicated, precaching or outside game status 1.
    // The object parameter is unused. gameLogic is dereferenced for isPrecaching before its later null test;
    // preserve this order and the client's runtime dedicated check.
    bool playEffectAllowed(IObject* /*object*/)
    {
        if (IObject::useForceDestroy)                            // byte 0xA0F72C / ELF 0x8AB0A20
            return false;
        if (isDedicated())                                       // BF2.exe 0x440510
            return false;
        if (gameLogic->isPrecaching())                           // +0x228 unguarded
            return false;
        if (gameLogic && gameLogic->getGameStatus() != 1)        // +0x158 1 = GameStatus_Playing
            return false;
        return true;
    }

    // Adds a scaled matrix row with a float temporary and float-rounded component sums.
    // [PLATFORM] The client rounds each step; the ELF keeps extended x87 intermediates.
    inline void addScaledRow(Vec3& sum, const float* row, float scale)
    {
        Vec3 product;
        product.x = scale * row[0];
        product.y = scale * row[1];
        product.z = scale * row[2];
        sum.x = product.x + sum.x;
        sum.y = product.y + sum.y;
        sum.z = product.z + sum.z;
    }

    // ..770 / PE 0x62FC0E..620: store the destruction factor before
    // submitting its product with the independent cull factor. The caller supplies the
    // geometry guard and clamp; neither is repeated here. The shared actual mesh base
    // preserves the x86 renderer carrier and supplies native pointer/string ownership.
    void geometrySetDamageFade(void* geometry, float fraction)
    {
        geom::GeometryInstanceData* const instance = static_cast<geom::GeometryInstanceData*>(geometry);
        instance->m_float28 = fraction;
        instance->setOpacity(fraction * instance->m_float2C);
    }

    IPlayerControlObject* findPco(IObject* start)
    {
        for (IObject* o = start; o; o = o->getParent())
            if (void* p = o->queryInterface(IID_IPlayerControlObject))
                return static_cast<IPlayerControlObject*>(p);
        return 0;
    }

    // The Lehmer step checkIfSpectacularHit inlines on a throwaway seed - the same 16-bit-halves
    // form Random::getRaw (Fx/ParticleTemplate.cpp) and Trigger::tryRandomTrigger carry.
    inline unsigned int lehmerStep(unsigned int x, unsigned int factor)
    {
        const unsigned int lo = (x & 0xFFFFu) * factor;
        const unsigned int hi = (x >> 16) * factor;
        const int carry = static_cast<int>(lo >> 16) + static_cast<int>(hi);
        unsigned int r = (lo & 0xFFFFu)
                       + ((static_cast<unsigned int>(carry) & 0x7FFFu) << 16)
                       + static_cast<unsigned int>(carry >> 15);
        const int reduced = static_cast<int>(r - 0x7FFFFFFFu);
        return reduced < 0 ? r : static_cast<unsigned int>(reduced);
    }
}

// the ledger record - getWeakPtr (IPlayer +0x1C), then the two values.
DamageFromPlayer::DamageFromPlayer(IPlayer* p, float a, double t)
    : player(p->getWeakPtr()), amount(a), worldTime(t)
{
}

// Applies damage from perpetrator, softening overkill into the wreck band according to physics/template flags.
// Rotates last-hit direction into object space and accumulates forward-side damage.
// Readers of the two side accumulators remain [UNRESOLVED].
void ArmorGLComp::damage(float amount, IPlayer* perpetrator)
{
    if (amount == 0.0f)
        return;
    if (!(-m_template->m_wreckHitPoints < m_hitPoints))
        return;
    if (isInvunerable())
        return;
    if (!gameLogic->isServer())
        return;

    const float hitPoints = m_hitPoints;
    IObject* obj = m_object;
    if (!(hitPoints <= 0.0f || amount <= hitPoints))
    {
        float a = 0.1f, b = 0.1f;
        if ((object_detail::LayoutAccess::flags(obj) & 8) == 0)
        {
            a = 0.5f;
            b = 0.8f;
        }
        if (!m_template->m_canBeDestroyed)
        {
            a = 0.9f;
            b = 0.9f;
        }
        const float maxOver = b * m_template->m_wreckHitPoints;
        float over = a * fabsf(hitPoints - amount);
        if (over > maxOver)
            over = maxOver;
        amount = hitPoints + over;
    }

    // world -> local rotation of the last-hit place: the transpose of the absolute transformation
    const Mat4& t = obj->getAbsoluteTransformation();                          // slot 33
    Mat4 copy = t;
    Mat4 transposed;
    transposed.m[0] = copy.m[0];  transposed.m[1] = copy.m[4];  transposed.m[2] = copy.m[8];   transposed.m[3] = copy.m[12];
    transposed.m[4] = copy.m[1];  transposed.m[5] = copy.m[5];  transposed.m[6] = copy.m[9];   transposed.m[7] = copy.m[13];
    transposed.m[8] = copy.m[2];  transposed.m[9] = copy.m[6];  transposed.m[10] = copy.m[10]; transposed.m[11] = copy.m[14];
    transposed.m[12] = copy.m[3]; transposed.m[13] = copy.m[7]; transposed.m[14] = copy.m[11]; transposed.m[15] = copy.m[15];
    const float px = m_lastHitPlace.x, py = m_lastHitPlace.y, pz = m_lastHitPlace.z;
    const float lx = transposed.m[4] * py + transposed.m[0] * px + transposed.m[8] * pz;
    const float ly = transposed.m[5] * py + transposed.m[1] * px + transposed.m[9] * pz;
    const float lz = py * transposed.m[6] + px * transposed.m[2] + pz * transposed.m[10];
    m_lastHitPlace.x = lx;
    m_lastHitPlace.y = ly;
    m_lastHitPlace.z = lz;
    if (lz > 1.0f)
    {
        if (lx <= 0.0f)
            m_dword22C = amount + m_dword22C;
        else
            m_dword230 = amount + m_dword230;
    }

    _setHitPoints(m_hitPoints - amount);
    m_healAccumulated = 0;
    bool spectacular = false;
    if (perpetrator)
        spectacular = checkIfSpectacularHit(amount);
    m_byte238 = spectacular;
    if (perpetrator && g_damageDebugLocalPlayer)
        m_byte238 = true;
    damageCheckPerputrator(perpetrator, amount);
}

// damageCheckPerputrator - 0x8218420, 864 B (dice's spelling). The damage ledgers.
// Every hit by a player appends {player, amount, worldTime} to m_damageFromPlayers. A soldier then
// checks whether this player is already on its team-damage list and, if not, whether their total
// against it has reached the score manager's team-damage limit - if so they are added (the list
// GameLogicDamage and ServerGameLogic::giveDamage consult). A vehicle appends the same record to
// m_vehicleDamageFromPlayers instead - unless no seat is occupied AND the root PCO's default team
// is 0, in which case that ledger is cleared: damage to an unowned, unmanned vehicle is nobody's.
void ArmorGLComp::damageCheckPerputrator(IPlayer* player, float amount)
{
    if (!player)
        return;
    m_damageFromPlayers.push_back(DamageFromPlayer(player, amount, WorldPref::getWorldTime()));

    if (m_object->getTemplate()->getClassID() == CID_SoldierTemplate)
    {
        for (std::list<WeakPtr<IPlayer> >::iterator it = m_teamDamagePlayers.begin();
             it != m_teamDamagePlayers.end(); ++it)
        {
            if (it->get() == player)
                return;
        }
        float sum = 0.0f;
        for (std::list<DamageFromPlayer>::iterator r = m_damageFromPlayers.begin();
             r != m_damageFromPlayers.end(); ++r)
        {
            if (r->player.get() == player)
                sum = sum + r->amount;
        }
        const float limit = reinterpret_cast<ScoreManager*>(scoreManager)->getTeamDamageLimit();   // +0x44
        if (sum >= getMaxHitPoints() * limit / 100.0f)
            m_teamDamagePlayers.push_back(player->getWeakPtr());                // +0x1C
        return;
    }

    IPlayerControlObject* pco = findPco(m_object);
    if (!pco)
        return;
    IPlayerControlObject* rootPco = findPco(getRootParent(m_object));
    std::map<int, IPlayerControlObject*>* pcos = rootPco->getPcos();           // +0x60 unchecked
    bool occupied = false;
    for (std::map<int, IPlayerControlObject*>::iterator seat = pcos->begin(); seat != pcos->end(); ++seat)
    {
        if (seat->second->getOccupingPlayer())                                 // +0x10
        {
            occupied = true;
            break;
        }
    }
    if (!occupied && !rootPco->getDefaultTeam())                               // +0x94
    {
        m_vehicleDamageFromPlayers.clear();
        return;
    }
    m_vehicleDamageFromPlayers.push_back(DamageFromPlayer(player, amount, WorldPref::getWorldTime()));
}

// _setHitPoints - 0x821CB00, 535 B. The ONE writer of m_hitPoints on the damage path.
// A rise above ceil(current) that stays within the template maximum is a heal / repair, and on the
// server every occupant is told (setHealRepairedAtTick: a soldier (tick, false, false), each seat
// of a vehicle (tick, false, true)). Then the clamp: [-wreckHitPoints, maxHitPoints], and a
// template that cannot be destroyed is floored at -wreckHitPoints a second time (the first clamp
// only applies below the maximum). Then updateStatus(true) and playEffect.
void ArmorGLComp::_setHitPoints(float hitPoints)
{
    const float current = m_hitPoints;
    m_previousHitPoints = m_hitPoints;
    const float ceiling = ceilf(current);
    ArmorGLCompTemplate* t = m_template;
    if (hitPoints > ceiling && !(t->m_maxHitPoints < hitPoints))
    {
        IPlayerControlObject* pco = findPco(m_object);
        if (pco && gameLogic->isServer())
        {
            if (m_object->getTemplate()->getClassID() == CID_SoldierTemplate)
            {
                if (IPlayer* p = pco->getOccupingPlayer())                     // +0x10
                    p->setHealRepairedAtTick(WorldPref::getTickCount(), false, false);   // +0x228
            }
            else
            {
                IPlayerControlObject* rootPco = findPco(getRootParent(m_object));
                std::map<int, IPlayerControlObject*>* pcos = rootPco->getPcos();   // unchecked
                for (std::map<int, IPlayerControlObject*>::iterator seat = pcos->begin();
                     seat != pcos->end(); ++seat)
                {
                    if (IPlayer* p = seat->second->getOccupingPlayer())
                        p->setHealRepairedAtTick(WorldPref::getTickCount(), false, true);
                }
            }
        }
        t = m_template;
    }

    float value = t->m_maxHitPoints;
    if (hitPoints <= value)
    {
        value = -t->m_wreckHitPoints;
        if (value <= hitPoints)
            value = hitPoints;
    }
    m_hitPoints = value;
    if (!t->m_canBeDestroyed)
    {
        float floor = -t->m_wreckHitPoints;
        if (floor <= m_hitPoints)
            floor = m_hitPoints;
        m_hitPoints = floor;
    }
    updateStatus(m_hitPoints, true);
    playEffect();
}

// Transitions between intact, wrecked and destroyed states, updating geometry, messages and explosions.
// Spectacular destruction defers removal and returns with m_sendingMessage still set.
// Repair restores non-soldier wrecks; cached status updates notify the strategic holder once per direction.
void ArmorGLComp::updateStatus(float hitPoints, bool notify)
{
    bool rootIsLive = false;
    IObject* obj = m_object;
    m_sendingMessage = true;
    IObject* root = getRootParent(obj);
    if (root && (object_detail::LayoutAccess::flags(root) & 0x20) == 0)
        rootIsLive = true;

    ArmorGLCompTemplate* t;
    float hp;
    if (hitPoints <= 0.0f && !m_destroyed)
    {
        if (m_byte238)
        {
            t = m_template;
            if (t->m_destroyOnSpectacularDeath)
            {
                m_pendingDestroy = true;
                return;                                                        // m_sendingMessage stays set
            }
        }
        getRootParent(m_object)->updateFlags(2048, 0);                         // slot 24
        m_destroyed = true;
        t = m_template;
        m_criticallyDamaged = true;
        m_byte58 = true;
        if (t->m_canBeDestroyed)
        {
            const unsigned int tick = WorldPref::getTickCount();
            const float window = 30.0f * *g_armorInvunerableTimeAfterWreck;
            m_invulnerableUntilTick = static_cast<int>(window) + tick;
        }
        m_lastHitTimeout = 10.0f;
        m_wreckTime = static_cast<float>(WorldPref::getWorldTime());
        if (m_template->m_wreckDelay <= 0.0099999998f)
            setUseDestroyedLod(true, false);
        else
            objectManager->postMessage(m_object, static_cast<TemplateMessage>(39), m_template->m_wreckDelay);   // +0xD0
        onDestroyedObjectAI();
        if (notify && rootIsLive)
            m_object->handleMessage(static_cast<TemplateMessage>(24), 0, false);   // slot 65
        t = m_template;
        if (t->m_explosionDamage > 0.0f)
        {
            IPlayer* lastHit = m_lastHitPlayer.get();
            Vec3 pos = m_object->getAbsolutePosition();
            gameLogic->handleExplosion(pos, m_template->m_explosionRadius, m_template->m_explosionMaterial,
                                       m_template->m_explosionDamage, lastHit, this, -1, 0, 0,
                                       m_template->m_explosionForce, m_object, 0, 1.0f, m_lastHitCanTK,
                                       true);                                  // +0xE8
            if (m_template->m_useMMOnDeath)
            {
                Vec3 normal = { 0.0f, 0.0f, 0.0f };
                unsigned char material = 0;
                const Vec3 p = m_object->getAbsolutePosition();
                const float ground = static_cast<IHeightmapCluster*>(heightmapCluster)
                                         ->getHeightAndNormalInWorldCoords(p.x, p.z, normal, material);   // +0x84
                const float radius = m_object->getPhysicBoundingRadius();      // slot 38
                if (ground >= p.y - radius)
                {
                    Vec3 at = { p.x, ground, p.z };
                    Vec3 zero = { 0.0f, 0.0f, 0.0f };
                    Vec3 negated = { -normal.x, -normal.y, -normal.z };
                    gameLogic->doImpact(m_template->m_explosionMaterial, material,
                                        static_cast<MaterialCollisionType>(0), at, normal, negated,
                                        m_object, 0, 0, 0, zero, false);       // +0x10C
                }
            }
            if (gameLogic->isServer())
            {
                IPhysicsNode* node =
                    static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(m_object));
                if (node)
                {
                    float force = m_template->getExplosionForce();             // slot 30
                    if (force > m_template->getExplosionForceMax())             // slot 32
                        force = m_template->getExplosionForceMax();
                    Vec3 impulse = { 0.0f, force, 0.0f };
                    node->setIsAwake();                                        // +0x130
                    Vec3 zero = { 0.0f, 0.0f, 0.0f };
                    node->addAccelerationAtRelativePosition(zero, impulse);    // +0x94
                }
            }
        }
        t = m_template;
        hp = m_hitPoints;
    }
    else if (hitPoints > 0.0f && m_destroyed)
    {
        if (m_object->getTemplate()->getClassID() != CID_SoldierTemplate)
        {
            setUseDestroyedLod(false, false);
            m_destroyed = false;
            m_byte188 = false;
            if (rootIsLive)
                m_object->handleMessage(static_cast<TemplateMessage>(25), 0, false);
        }
        if (void* geometry = object_detail::LayoutAccess::geometry(m_object))
            geometryInitDamageState(geometry);                                 // [+0x10] = 1.0, slot +0xF8
        m_wreckTime = 0.0f;
        m_destroyedTime = -1.0f;
        t = m_template;
        hp = m_hitPoints;
    }
    else
    {
        t = m_template;
        if (-t->m_wreckHitPoints < m_hitPoints)
            hp = m_hitPoints;
        else
        {
            if (object_detail::LayoutAccess::aiObject(m_object))
            {
                if (m_object->getTemplate()->getClassID() != CID_DestroyableObjectTemplate)
                    m_object->setComponent(ai::IID_IAIObject, 0);              // slot 22
                t = m_template;
            }
            if (t->m_wreckExplosionDamage > 0.0f)
            {
                IPlayer* lastHit = m_lastHitPlayer.get();
                Vec3 pos = m_object->getAbsolutePosition();
                gameLogic->handleExplosion(pos, m_template->m_wreckExplosionRadius,
                                           m_template->m_wreckExplosionMaterial,
                                           m_template->m_wreckExplosionDamage, lastHit, this, -1, 0, 0,
                                           m_template->m_wreckExplosionForce, m_object, 0, 1.0f,
                                           m_lastHitCanTK, true);
                t = m_template;
            }
            hp = m_hitPoints;
        }
    }

    const float criticalDamage = t->m_criticalDamage;
    m_sendingMessage = false;
    m_criticallyDamaged = criticalDamage > hp;
    if (m_destroyed)
    {
        if (getStatus() != m_status && !m_byte59)
        {
            m_byte59 = true;
            m_byte5A = false;
            updateStrategicObject(true);
            m_status = getStatus();
        }
    }
    else
    {
        if (getStatus() != m_status && !m_byte5A)
        {
            m_byte59 = false;
            m_byte5A = true;
            updateStrategicObject(false);
            m_status = getStatus();
        }
    }
}

// Advances wreck expiry, soldier ragdoll/seat waiting and the final destruction delay.
// The server destroys or resets expired objects. Non-soldier wrecks lose hit points while waiting;
// soldier geometry fades during the final interval.
void ArmorGLComp::updateDestroy()
{
    const float worldTime = static_cast<float>(WorldPref::getWorldTime());
    ArmorGLCompTemplate* t = m_template;
    bool ready = worldTime > t->m_timeToStayAsWreck + m_wreckTime;
    const bool belowFloor = -t->m_wreckHitPoints >= m_hitPoints;
    if (m_object->getTemplate()->getClassID() != CID_SoldierTemplate)
    {
        if (belowFloor)
            ready = true;
    }
    else
    {
        IObject* obj = m_object;
        if (!obj->getParent())
        {
            if (!static_cast<Soldier*>(m_object)->hasRagdollLanded())
                ready = false;
            obj = m_object;
        }
        IPlayerControlObject* pco = static_cast<IPlayerControlObject*>(
            obj->queryInterface(IID_IPlayerControlObject));
        if (pco && pco->getOccupingPlayer())
            ready = false;
        if (worldTime > *g_armorMaxSoldierWreckTime + m_wreckTime)
            ready = true;
    }
    if (ready && m_destroyedTime == -1.0f)
        m_destroyedTime = worldTime;

    bool expired = false;
    if (ready)
    {
        float left = m_template->m_timeToStayAfterDestroyed + m_destroyedTime - worldTime;
        if (left <= 0.0f)
            left = 0.0f;
        IObject* obj = m_object;
        expired = left <= 0.0f;
        if (object_detail::LayoutAccess::geometry(obj))
        {
            if (obj->getTemplate()->getClassID() == CID_SoldierTemplate)
            {
                float fraction = left / m_template->m_timeToStayAfterDestroyed;
                if (0.0f > fraction)
                    fraction = 0.0f;
                else if (fraction > 1.0f)
                    fraction = 1.0f;
                geometrySetDamageFade(object_detail::LayoutAccess::geometry(m_object), fraction);
            }
        }
    }

    if (!gameLogic->isServer())
        return;
    if (expired)
    {
        destroyOrResetObject();
        return;
    }
    if (m_object->getTemplate()->getClassID() == CID_SoldierTemplate)
        return;
    float bleed = m_template->m_wreckHitPoints / (30.0f * m_template->m_timeToStayAsWreck);
    IObject* obj = m_object;
    if (ai::g_aiInstance && ai::aiInstance()->isBotsGame())                     // slot +0x1C
    {
        if ((object_detail::LayoutAccess::flags(obj) & 8) != 0)
            bleed = bleed * 3.0f;
    }
    Vec3 direction = m_lastHitDirection;
    Vec3 zero = { 0.0f, 0.0f, 0.0f };
    gameLogic->giveDamage(obj, bleed, 0, 0, 0, -1, 0, 0, zero, direction, -1, false, true, false);   // +0xE4
}

// fullDestroy - 0x821BDD0, 373 B. A soldier whose vehicle was not its default one is destroyed
// outright: every occupant (the one soldier, or every seat of the root) gets
// setHealRepairedAtTick(0, false, false) on the server, the hit points go to zero and
// updateStatus(true) runs the wreck arm.
void ArmorGLComp::fullDestroy()
{
    IPlayerControlObject* pco = findPco(m_object);
    if (pco && gameLogic->isServer())
    {
        if (m_object->getTemplate()->getClassID() == CID_SoldierTemplate)
        {
            if (IPlayer* p = pco->getOccupingPlayer())
                p->setHealRepairedAtTick(0, false, false);
        }
        else
        {
            IPlayerControlObject* rootPco = findPco(getRootParent(m_object));
            std::map<int, IPlayerControlObject*>* pcos = rootPco->getPcos();   // unchecked
            for (std::map<int, IPlayerControlObject*>::iterator seat = pcos->begin();
                 seat != pcos->end(); ++seat)
            {
                if (IPlayer* p = seat->second->getOccupingPlayer())
                    p->setHealRepairedAtTick(0, false, false);
            }
        }
    }
    m_hitPoints = 0.0f;
    updateStatus(m_hitPoints, true);
}

// checkIfSpectacularHit - 0x821C7F0, 297 B. A one-in-three roll on a throwaway seed - the
// object's networkable id pushed through two Lehmer steps (the same step Random::getRaw takes on
// its persistent state), scaled to [1, 3] and ceil'd - and, when it lands on 3, the hit is
// spectacular if it leaves less than a fifth of the template hit points or takes more than a
// quarter of them.
bool ArmorGLComp::checkIfSpectacularHit(float amount)
{
    const float remaining = m_hitPoints - amount;
    io::INetworkable* networkable = m_object->getNetworkable();                // slot 54
    unsigned int seed = networkable->m_networkableId;                          // +0x04 raw, as shipped
    seed = lehmerStep(seed, 24112u);
    seed = lehmerStep(seed, 26143u);
    const int raw = static_cast<int>(((seed >> 7) | 1u) + 1u);
    const float rolled = ceilf(static_cast<float>(raw) * 0.000000059604645f * 3.0f + 0.0f);
    if (static_cast<int>(rolled) == 3)
    {
        if (fabsf(remaining) > m_template->getHitPoints() / 5.0f              // slot 14
            || amount > m_template->getHitPoints() * 0.25f)
            return true;
    }
    return false;
}

// updateStrategicObject - 0x821C2E0, 543 B. The strategic-object holder's destroyed-part count,
// +1 when the object became a wreck and -1 when it was repaired. Gated on a local human player -
// dead on a dedicated server - and built exactly like ResetStrategicObject above.
void ArmorGLComp::updateStrategicObject(bool destroyed)
{
    if (!playerManager->getLocalHumanPlayer())
        return;
    IStrategicObjectComp* strategicObject = static_cast<IStrategicObjectComp*>(
        m_object->queryComponent(IID_IStrategicObjectComp, IID_IStrategicObjectComp));
    if (!strategicObject)
        return;
    const std::string name = strategicObject->getStrategicObjectName();        // slot +0x38
    const std::string key  = name + static_cast<char>(strategicObject->getIndex() + '0');   // +0x50
    std::map<std::string, StrategicObjectHolder*>* holders =
        gameLogic->getStrategicObjectMap();
    std::map<std::string, StrategicObjectHolder*>::iterator it = holders->find(key);
    if (it == holders->end())
    {
        Debug(DEBUGTYPE_6, "Game/Objects/GameLogicComp/ArmorGLComp.cpp", 1744, "")
            << "Couldn't update number of destroyed parts!";
    }
    else if (destroyed)
        it->second->setNrDestroyedObjects(it->second->getNrDestroyedObjects() + 1);   // holder +0x10
    else
        it->second->setNrDestroyedObjects(it->second->getNrDestroyedObjects() - 1);
}

// updateWreckStatus - 0x8219DC0, 349 B. The wreck-floor half of updateStatus's third arm on its
// own: below -wreckHitPoints drop the AI object (not for destroyables) and fire the wreck
// explosion. [UNRESOLVED caller in this tree.]
void ArmorGLComp::updateWreckStatus()
{
    ArmorGLCompTemplate* t = m_template;
    if (!(-t->m_wreckHitPoints >= m_hitPoints))
        return;
    IObject* obj = m_object;
    if (object_detail::LayoutAccess::aiObject(obj))
    {
        if (obj->getTemplate()->getClassID() != CID_DestroyableObjectTemplate)
            m_object->setComponent(ai::IID_IAIObject, 0);
        t = m_template;
    }
    if (t->m_wreckExplosionDamage > 0.0f)
    {
        IPlayer* lastHit = m_lastHitPlayer.get();
        Vec3 pos = m_object->getAbsolutePosition();
        gameLogic->handleExplosion(pos, m_template->m_wreckExplosionRadius, m_template->m_wreckExplosionMaterial,
                                   m_template->m_wreckExplosionDamage, lastHit, this, -1, 0, 0,
                                   m_template->m_wreckExplosionForce, m_object, 0, 1.0f, m_lastHitCanTK,
                                   true);
    }
}

// Notifies AI of destruction and replaces the object's AI component with its wreck component.
// Soldiers report the occupant's vehicle AI; destroyable objects instead report the first
// damage-ledger player and keep their component. Objects without AI are ignored.
void ArmorGLComp::onDestroyedObjectAI()
{
    IObject* obj = m_object;
    ai::IAIObject* aiObject = static_cast<ai::IAIObject*>(object_detail::LayoutAccess::aiObject(obj));
    if (!aiObject)
        return;
    const bool soldier = obj->getTemplate()->getClassID() == CID_SoldierTemplate;
    bool notify = false;
    if (object_detail::LayoutAccess::parent(obj) || !soldier)
        notify = true;
    void* target = aiObject;
    if (soldier)
    {
        if (IPlayerControlObject* pco = findPco(m_object))
        {
            if (IPlayer* occupant = pco->getOccupingPlayer())
                target = object_detail::LayoutAccess::aiObject(occupant->getVehicle());   // +0x48 +0x34
        }
    }
    else if (obj->getTemplate()->getClassID() == CID_DestroyableObjectTemplate)
    {
        notify = false;
        int id = -1;
        if (!m_damageFromPlayers.empty())
        {
            WeakPtr<IPlayer> first = m_damageFromPlayers.front().player;
            if (IPlayer* p = first.get())
                id = p->getId();                                                // +0x54
        }
        struct { int id; void* aiObject; } payload = { id, aiObject };
        eventManager->postEvent(ECAI, 17, &payload, 0.0f);
    }
    if (notify)
    {
        eventManager->postEvent(ECAI, 14, target, 0.0f);
        ai::IAIObject* replacement = aiObject->getWreckObject(m_object);
        m_object->setComponent(ai::IID_IAIObject, 0);
        if (replacement)
        {
            m_object->setComponent(ai::IID_IAIObject, replacement);
            replacement->release();
        }
    }
}

// The four last-hit setters. Three are WeakPtr takes (159 B each: ask the object for its weak
// reference, swap it in, drop the old block); setLastHitDirection (265 B) stores a normalised copy
// - and only on the server, or when forced, and only while the armor still has hit points.
// setLastHitPlayer does nothing once the object is a wreck, and it also reloads the last-hit
// timeout to 2.0 seconds. The weapon and vehicle setters have no such gate.
void ArmorGLComp::setLastHitDirection(const Vec3& direction, bool force)
{
    if (!(force || gameLogic->isServer()))
        return;
    if (!(m_hitPoints > 0.0f))
        return;
    m_lastHitDirection = direction;
    const float lenSq = direction.z * direction.z + direction.y * direction.y + direction.x * direction.x;
    if (fabsf(lenSq - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(lenSq) >= FLT_EPSILON)
        {
            const float k = 1.0f / sqrtf(lenSq);
            m_lastHitDirection.x = m_lastHitDirection.x * k;
            m_lastHitDirection.y = m_lastHitDirection.y * k;
            m_lastHitDirection.z = k * m_lastHitDirection.z;
        }
        else
        {
            m_lastHitDirection.x = 0.0f;
            m_lastHitDirection.y = 0.0f;
            m_lastHitDirection.z = 0.0f;
        }
    }
}

void ArmorGLComp::setLastHitPlayer(IPlayer* player)
{
    if (m_destroyed)
        return;
    if (player)
        m_lastHitPlayer = player->getWeakPtr();                                // +0x1C
    else
        m_lastHitPlayer = WeakPtr<IPlayer>();
    m_lastHitTimeout = 2.0f;
}

void ArmorGLComp::setLastHitWeapon(IObject* weapon)
{
    if (weapon)
        m_lastHitWeapon = weapon->getWeakPtr();                                // slot 7
    else
        m_lastHitWeapon = WeakPtr<IObject>();
}

void ArmorGLComp::setLastHitVehicle(IObject* vehicle)
{
    if (vehicle)
        m_lastHitVehicle = vehicle->getWeakPtr();
    else
        m_lastHitVehicle = WeakPtr<IObject>();
}

// Returns effects only when the percentage damage state changes; otherwise returns null.
// Selects the smallest state key >= ceil(hitPoints / applicable maximum * 100), stopping prior effects on change.
// Repair into destruction states 0/-100 does not replay them. The parameter is unused.
// Preserve repeated virtual maximum queries and the >=0 denominator test, including NaN behavior.
// [PLATFORM] Client arithmetic rounds to float; ELF intermediates and map layout differ.
DamageEffects* ArmorGLComp::getEffect(int /*state*/)
{
    if (!m_effects || m_effects->empty())                                       // [+0x180], size
        return 0;

    const int previousState = static_cast<int>(ceilf(m_previousHitPoints       // PE +0x254 / ELF +0x23C
        / (m_previousHitPoints >= 0.0f ? m_template->getMaxHitPoints()          // +0x40
                                       : m_template->getWreckHitPoints())       // +0x30
        * 100.0f));
    const int currentState = static_cast<int>(ceilf(m_hitPoints                // +0x10
        / (m_hitPoints >= 0.0f ? m_template->getMaxHitPoints()
                               : m_template->getWreckHitPoints())
        * 100.0f));

    if (m_effects->rbegin()->first < currentState)                              // above the top key
    {
        StopAllEffects();
        return 0;
    }

    bool changed = false;
    std::map<int, DamageEffects*>::reverse_iterator found = m_effects->rbegin();
    for (std::map<int, DamageEffects*>::reverse_iterator it = m_effects->rbegin();
         it != m_effects->rend(); ++it)
    {
        if (currentState > it->first)
        {
            if (previousState <= it->first)                                     // came up through it
                changed = true;
            break;
        }
        if (previousState > it->first)                                          // came down through it
            changed = true;
        found = it;
    }

    // a repair into a destroyed state is swallowed (PE 0x688005..0x68802A returns 0 outright; the
    // ELF clears the flag, 0x821A11B, and returns 0 below - the same answer).
    if (previousState < currentState && (found->first == 0 || found->first == -100))
        changed = false;
    if (!changed)
        return 0;

    m_byte188 = found->first == 0 || found->first == -100;                     // +0x188
    StopAllEffects();
    return found->second;
}

// Plays newly selected damage-state effects after the client gate, suppressing duplicate results.
// Intact effects attach to the owner and are tracked; destroyed-state effects use world transforms,
// retain their own lifetime and are untracked. Template/position/rotation lists must remain parallel.
// The 32nd tracked effect triggers the fatal capacity diagnostic after its ID is stored.
// The client may read m_byte25C before its first write; preserve that behavior and the effect/message order.
// [PLATFORM] Dedicated builds reject effects at the gate; client calculations are authoritative.
void ArmorGLComp::playEffect()
{
    if (!playEffectAllowed(m_object))
        return;
    if (!m_effects || m_effects->empty())                                       // [+0x180], size
        return;

    DamageEffects* const effects = getEffect(0);
    if (effects == m_lastDamageEffects)                                         // +0x184
        return;
    if (!gameLogic->isServer() && m_byte188 && m_byte25C)                       // +0x188 +0x25C (PE +0x274)
        return;

    if (effects)
    {
        int count = 0;
        const std::list<const IObjectTemplate*>& templates  = effects->getEffects(m_byte238);
        const std::list<Vec3>&                   positions  = effects->getPositions(m_byte238);
        const std::list<Vec3>&                   rotations  = effects->getDirections(m_byte238);
        std::list<Vec3>::const_iterator position = positions.begin();
        std::list<Vec3>::const_iterator rotation = rotations.begin();
        for (std::list<const IObjectTemplate*>::const_iterator it = templates.begin();
             it != templates.end(); ++it, ++position, ++rotation)
        {
            const IObjectTemplate* const effectTemplate = *it;
            IObject* const effect = effectTemplate->createObject();
            if (!effect)
                continue;

            if (!m_byte188)
            {
                // an attached effect of a live state: tracked, relative to the owner
                m_effectIds[count] = effect->getObjectId();                     // +0x10
                ++count;
                if (count > 31)                                                 // after the store
                    Debug(DEBUGTYPE_6, "Game/Objects/GameLogicComp/ArmorGLComp.cpp", 1082, "")
                        << "TBM: can't have more then 32 damage Effects on the same damage level";
                if (effectTemplate->getClassID() == CID_EffectBundleTemplate)
                    static_cast<EffectBundle*>(effect)->overRideTimeToLive(true);
                effect->init();
                m_object->addChild(effect);
                Mat4 transform = Mat4::getIdentity();
                setRotationZXY(transform, -rotation->z, -rotation->y, -rotation->x);
                transform.m[12] = position->x;
                transform.m[13] = position->y;
                transform.m[14] = position->z;
                effect->setRelativeTransformation(transform);
            }
            else
            {
                // a destroyed state (0 / -100): world space, unattached, untracked
                effect->init();
                if (m_template->getAlignLastEffectToHitDirection())             // template +0x120
                {
                    Mat4 transform = effect->getParent() ? effect->getRelativeTransformation()
                                                         : effect->getAbsoluteTransformation();
                    Vec3& right   = *reinterpret_cast<Vec3*>(&transform.m[0]);
                    Vec3& up      = *reinterpret_cast<Vec3*>(&transform.m[4]);
                    Vec3& forward = *reinterpret_cast<Vec3*>(&transform.m[8]);
                    Vec3& origin  = *reinterpret_cast<Vec3*>(&transform.m[12]);
                    const Vec3 hitDirection = m_lastHitDirection;                // +0x48
                    if (fabsf(hitDirection.x) < 0.000099999997f
                        && fabsf(hitDirection.y) < 0.000099999997f)
                    {
                        // along +-Z the fixed frame puts the direction's sign in UP and forward
                        // becomes +-X - unlike the general arm, which puts the direction itself in
                        // forward. A NaN z takes the + frame. As shipped (0x68835D..0x6883AC).
                        right.x = 0.0f;  right.y = 1.0f;  right.z = 0.0f;
                        up.x = 0.0f;     up.y = 0.0f;
                        forward.y = 0.0f; forward.z = 0.0f;
                        if (hitDirection.z < 0.0f)
                        {
                            forward.x = -1.0f;
                            up.z = -1.0f;
                        }
                        else
                        {
                            forward.x = 1.0f;
                            up.z = 1.0f;
                        }
                    }
                    else
                    {
                        forward = hitDirection;
                        right.makeOrthonormalBasis(forward, up);    // result ignored (0x6D8E40)
                    }
                    // the row's position carried into the owner's frame; the owner's rotation is
                    // NOT applied to the aligned rows - they stay the world-space frame built above
                    const Mat4& owner = m_object->getAbsoluteTransformation();
                    origin.x = owner.m[12];
                    origin.y = owner.m[13];
                    origin.z = owner.m[14];
                    addScaledRow(origin, &owner.m[8], position->z);
                    addScaledRow(origin, &owner.m[4], position->y);
                    addScaledRow(origin, &owner.m[0], position->x);
                    effect->setAbsoluteTransformation(transform);
                }
                else
                {
                    // retail default-constructs this (0x440B20 = the identity); the tree's
                    // aggregate Mat4 spells that as a copy of getIdentity
                    Mat4 transform = Mat4::getIdentity();
                    setRotationZXY(transform, -rotation->z, -rotation->y, -rotation->x);
                    // the offset is rotated by the effect's own rotation before the owner's
                    // transformation is applied - the origin starts at the identity's (0, 0, 0)
                    Vec3& origin = *reinterpret_cast<Vec3*>(&transform.m[12]);
                    addScaledRow(origin, &transform.m[8], position->z);
                    addScaledRow(origin, &transform.m[4], position->y);
                    addScaledRow(origin, &transform.m[0], position->x);
                    const Mat4& owner = m_object->getAbsoluteTransformation();
                    const Mat4 local = transform;                               // copy,
                    transform.mult(local, owner);                               // then 0x6D9840
                    effect->setAbsoluteTransformation(transform);
                }
            }
            effect->handleMessage(static_cast<TemplateMessage>(0), 0, false);  // 0 = start
        }
        m_effectCount = count;                                                  // +0x17C
    }
    m_lastDamageEffects = effects;
}

// Repairs by amount percent of template maximum on the server, subject to wreck-repair policy.
// Clears damage attribution after more than 50 accumulated points or when already at maximum health,
// then applies the heal through _setHitPoints for rounding/clamping.
void ArmorGLComp::heal(float amount)
{
    if (!gameLogic->isServer())                                          // +0x13C
        return;
    if (!(m_template->m_canBeRepairedWhenWreck || !m_destroyed))          // template +0x110 / +0x15
        return;

    const float hp = amount / 100.0f * m_template->m_maxHitPoints;       // +0xA8
    m_healAccumulated += hp;                                             // +0x24C
    if (m_healAccumulated > 50.0f || getHitPoints() >= m_template->getMaxHitPoints())   // +0x40
    {
        m_damageFromPlayers.clear();                                     // +0x220
        m_healAccumulated = 0.0f;
    }
    _setHitPoints(hp + m_hitPoints);
}

// , 197 B. Positions are already in the repair
// points' space. An empty list permits repair everywhere; otherwise any inclusive sphere wins.
// Negative radii and unordered distances fail the comparison, without added input guards.
bool ArmorGLComp::isInsideRepairRadius(const Vec3& position)
{
    if (m_template->m_repairPoints.empty())
        return true;
    for (std::size_t i = 0; i < m_template->m_repairPoints.size(); ++i)
    {
        const RepairPoint& point = m_template->m_repairPoints[i];
        const float x = position.x - point.position.x;
        const float y = position.y - point.position.y;
        const float z = position.z - point.position.z;
        if (std::sqrt(z * z + y * y + x * x) <= point.radius)
            return true;
    }
    return false;
}

// Revives occupied destroyed objects on the server; soldiers must fit prone one metre above their position.
// Adds amount directly to hit points. Crossing the wreck threshold restores maximum health and broadcasts
// the network ID; preserve status-update order and repeated networkable queries.
void ArmorGLComp::revive(float amount)
{
    if (getStatus() != 1)
        return;

    IPlayerControlObject* const pco = findPco(m_object);
    if (!pco || !pco->isOccupied())
        return;
    if (!gameLogic->isServer())
        return;

    if (m_object->getTemplate()->getClassID() == CID_SoldierTemplate)
    {
        ISoldier* const soldier = static_cast<ISoldier*>(m_object->queryInterface(IID_ISoldier));
        if (soldier)
        {
            Mat4 transform = m_object->getAbsoluteTransformation();
            transform.m[13] += 1.0f;
            if (!soldier->testIfValidTransformation(SoldierPose_Prone, transform, false))
                return;
        }
    }

    const float revivedHitPoints = m_hitPoints + amount;
    if (revivedHitPoints <= m_template->m_wreckHitPoints)
    {
        updateStatus(0.0f, true);
        m_previousHitPoints = m_hitPoints;
        m_hitPoints = revivedHitPoints;
        return;
    }

    setSoldierIsRevived();
    if (m_object->getNetworkable())
    {
        unsigned short networkableId =
            static_cast<unsigned short>(m_object->getNetworkable()->m_networkableId);
        game->sendGameEventToAllPlaying(
            new PostRemoteEvent(ECGameLogic, 14u, true,
                                &networkableId, 2u, 0.0f));
    }
    m_healAccumulated = 0.0f;
    m_damageFromPlayers.clear();
}

// setSoldierIsRevived - ELF 0x8218B00, 186 B / PE 0x6320C0, 198 B. The class-id mismatch is a
// shipped hard invariant (ELF abort, PE int 3). Restore full health, run the ordinary intact-LOD
// helper (which immediately returns for a soldier), clear the wreck/effect latches, deliver TM 25,
// reset geometry damage state when geometry exists, and clear both destruction timestamps.
void ArmorGLComp::setSoldierIsRevived()
{
    if (m_object->getTemplate()->getClassID() != CID_SoldierTemplate)
    {
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::abort();
#endif
    }

    m_hitPoints = m_template->m_maxHitPoints;
    setUseDestroyedLod(false, false);
    m_destroyed = false;
    m_byte188 = false;
    m_object->handleMessage(static_cast<TemplateMessage>(25), 0, false);
    if (void* geometry = object_detail::LayoutAccess::geometry(m_object))
        geometryInitDamageState(geometry);
    m_wreckTime = 0.0f;
    m_destroyedTime = -1.0f;
}

} // namespace world
} // namespace hfe
} // namespace dice
