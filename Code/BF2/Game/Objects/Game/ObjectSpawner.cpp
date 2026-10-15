// Object spawning, placement, occupancy checks and lifecycle. [INFERRED] Source placement.

#include "ObjectSpawner.h"
#include "../../../IO/Stream/IStream.h"
#include <sstream>

#include "../../../Core/Ids.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h" // Spawned vehicle interface.
#include "../../../Core/SettingsRepostitory.h"      // settings->intGet - calcSpawnDelay's dead read
#include "../../IGame.h"                            // game->queryInterface(IID_IGameServer)
#include "../../IGameLogic.h"                       // gameLogic - add/removeObjectSpawner, getGameStatus
#include "../ObjectSpawnerTemplate.h"
#include "../ObjectLayoutAccess.h"                  // createdOnLoad (+0x78), physicsNode (+0x44)
#include "../ObjectTemplate.h"                      // getCollisionMesh on the spawned template
#include "../Base/SimpleObjectTemplate.h"
#include "../../../Physics/CollisionManager.h"      // collisionManager->getCollisionMeshPart
#include "../../../Physics/ICollisionMesh.h"
#include "../../../Physics/IPhysicsNode.h"          // setIsAwake - destroy's wake-up
#include "../../../Scene/Object/ObjectManager.h"    // objectManager->getObjectFromGrid(id)
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/PlayerManager.h"    // playerManager->getNumberOfPlayers()
#include "../../../Scene/Object/ObjectPredicator.h"  // the interface / flag / ignore predicators (spawnObject, the snaps)
#include "../../../Physics/HeightmapCluster.h"       // IHeightmapCluster::getHeightInWorldCoords - the terrain snap
#include "../../../Math/LocalBoundingBox.h"          // the mesh / object bounding boxes the snaps read
#include "../../../Core/System.h"                    // g_system->getExactTime() - setTimeSpawned
#include "../../Utility/Vars.h"                      // the three objectspawner-* cvars
#include "../Base/SimpleObject.h"                    // m_replicationState (+0x134) - the hold arm writes it
#include "../GameLogicComp/IArmorGLComp.h"           // the armor interface at IObject +0x3C
#include "../IPlayer.h"                             // getVehicle - moveIntersectingSoldiers' occupant test
#include "../GameLogicComp/ArmorGLComp.h"            // armorTemplateCanBeDestroyed
#include "../GameLogicComp/ArmorGLCompTemplate.h"
#include "EngineBase.h"                              // m_input - the hold arm's engine test
#include <cmath>                                     // sqrtf / fabsf

#include <cstdlib>
#include <string>

namespace dice {
namespace hfe {

extern IGameLogic* gameLogic;                       // ds:0x8ACBFA0 - Core/Globals.cpp

namespace world {


// Initializes spawn-slot bookkeeping and, on servers, effect lookup and countdown state.
// Client construction leaves spawnDelay and effectOnSpawnTemplate uninitialized.
ObjectSpawner::ObjectSpawner(const ObjectSpawnerTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
{
    m_refCount = 1;                                  // +0x13C
    for (int i = 0; i < 3; ++i)                      // +0x140 +0x148 / +0x150
    {
        m_collisionMeshes[i].mesh  = 0;
        m_collisionMeshes[i].valid = true;
    }
    m_template = objectTemplate;                     // +0x158
    m_controlPointId = objectTemplate->m_controlPointId;   // +0x15C <- template +0x1BC (its -1 default)
    // m_spawnedObjectIds (+0x160..+0x168): empty
    m_lastSpawnedId = -1;                            // +0x16C
    // m_timeToLive (+0x174..+0x17C): empty
    m_active = objectTemplate->m_activeOnStart;      // +0x180 <- template +0x1C4
    m_team = static_cast<int>(objectTemplate->m_team);          // +0x184 <- template +0x198
    m_teamOnStart = static_cast<int>(objectTemplate->m_team);   // +0x188 <- template +0x198
    // m_spawnedTemplates (+0x18C/+0x190): the SGI header the ELF allocates is the empty map
    m_nrOfObjectToSpawn = objectTemplate->m_nrOfObjectToSpawn;  // +0x198 <- template +0x19C
    m_useButtonPressed = false;                      // +0x19C
    m_spawnOffset = objectTemplate->m_spawnOffset;   // +0x1A0 <- template +0x1AC
    m_holdObject = objectTemplate->m_holdObject;     // +0x1B0 <- template +0x1C0
    _bool1B1 = false;                                // +0x1B1
    m_visibleTeam = 1;                               // +0x1B4
    m_blockedSpawnCount = 0;                                   // +0x1B8

    if (!game->queryInterface(IID_IGameServer))
        return;

    Object::updateFlags(0x800u, 2u);
    m_effectOnSpawnTemplate =
        objectTemplateManager->getTemplate(objectTemplate->m_effectOnSpawn);   // +0x24 (string)
    const unsigned int slots = objectTemplate->m_maxNrOfObjectSpawned;         // +0x1A0
    m_spawnedObjectIds.resize(slots, 0);             // the ELF's erase / _M_fill_insert pair
    m_timeToLive.resize(slots, 0.0f);
    for (unsigned int i = 0; i < slots; ++i)
    {
        m_spawnedObjectIds[i] = -1;
        m_timeToLive[i] = objectTemplate->m_timeToLive;                        // +0x190
    }
    if (m_active)
    {
        if (objectTemplate->m_spawnDelayAtStart)                               // +0x1C3
            calcSpawnDelay();
        else
            m_spawnDelay = -1.0f;                                              // +0x170
    }
}

// (D2) / 0x818E340 (D1) / 0x818E460 (D0), 281 / 281 / 288 B: vptr restores; erase and
// free the +0x18C map; free the two vectors; delete each cached collision mesh (ICollisionMesh slot
// 5, the deleting dtor), last slot first; SimpleObject's dtor (D0 adds operator delete).
ObjectSpawner::~ObjectSpawner()
{
    for (int i = 2; i >= 0; --i)
    {
        if (m_collisionMeshes[i].mesh)
            delete m_collisionMeshes[i].mesh;
    }
}

// The quartet - refcount at +0x13C.
void ObjectSpawner::addRef()  { ++m_refCount; }
int  ObjectSpawner::getRef()  { return m_refCount; }
int  ObjectSpawner::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

// , five cases: IBase / IObject -> this; ITeamObject / IObjectSpawner -> the +0x138 sub-object;
// IConsoleSaveable -> +0x80; else 0.
void* ObjectSpawner::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)   return static_cast<IObject*>(const_cast<ObjectSpawner*>(this));
    if (iid == IID_IObject) return static_cast<IObject*>(const_cast<ObjectSpawner*>(this));
    if (iid == IID_ITeamObject || iid == IID_IObjectSpawner)
        return static_cast<IObjectSpawner*>(const_cast<ObjectSpawner*>(this));      // this + 0x138
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<ObjectSpawner*>(this)); // this + 0x80
    return 0;
}

// SimpleObject::init, the createdOnLoad byte (+0x78) = 1, then
// gameLogic->addObjectSpawner(this) (+0x1DC) - the level's spawner registry, keyed by object id.
void ObjectSpawner::init()
{
    SimpleObject::init();
    object_detail::LayoutAccess::createdOnLoad(this) = true;
    gameLogic->addObjectSpawner(this);
}

// destroy - 0x818E580, 248 B: leave the registry (removeObjectSpawner, +0x1E4); wake every object
// this spawner issued (getObjectFromGrid(id) -> its physics node (+0x44) -> setIsAwake, +0x130 -
// so a held-asleep vehicle starts simulating when its spawner goes); tell the held object's PCO -
// the first IPlayerControlObject found on it or its parent chain - setObjectSpawnerHolding(false)
// [the RW-1 arm, banked above]; then SimpleObject::destroy.
void ObjectSpawner::destroy(bool force)
{
    gameLogic->removeObjectSpawner(this);
    for (unsigned int i = 0; i < m_spawnedObjectIds.size(); ++i)   // size RE-read each iteration
    {
        const int id = m_spawnedObjectIds[i];
        if (id == -1)
            continue;
        IObject* spawned = objectManager->getObjectFromGrid(static_cast<unsigned int>(id));   // +0x30
        if (spawned)
        {
            IPhysicsNode* node = static_cast<IPhysicsNode*>(
                object_detail::LayoutAccess::physicsNode(spawned));
            if (node)
                node->setIsAwake();
        }
    }
    IObject* held = objectManager->getObjectFromGrid(static_cast<unsigned int>(m_lastSpawnedId));
    if (held)
    {
        void* pco = 0;
        for (IObject* o = held; o; o = o->getParent())
        {
            pco = o->queryInterface(IID_IPlayerControlObject);
            if (pco)
                break;
        }
        if (pco)
            static_cast<IPlayerControlObject*>(pco)->setObjectSpawnerHolding(false);   // ipco +0xC0
    }
    SimpleObject::destroy(force);
}

// the team, and while the game is in status 3 also the start team, with the
// countdown parked (-1.0f) - a mid-game team change (control point captured) resets the spawner.
void ObjectSpawner::setTeam(int team)
{
    m_team = team;
    if (gameLogic->getGameStatus() == 3)             // +0x158
    {
        m_teamOnStart = team;
        m_spawnDelay = -1.0f;
    }
}
int ObjectSpawner::getTeam() { return m_team; }

// store, then recompute the countdown unless it is parked at -1.
void ObjectSpawner::setActive(bool active)
{
    const float pending = m_spawnDelay;
    m_active = active;
    if (pending != -1.0f)
        calcSpawnDelay();
}
bool ObjectSpawner::getActive() { return m_active; }

// compare-then-store (a no-op store when equal - preserved).
void ObjectSpawner::setControledById(int id)
{
    if (m_controlPointId != id)
        m_controlPointId = id;
}
int  ObjectSpawner::getControledById()  { return m_controlPointId; }
// both fields.
void ObjectSpawner::setTeamOnStart(unsigned int team)
{
    m_teamOnStart = static_cast<int>(team);
    m_team = static_cast<int>(team);
}
int  ObjectSpawner::getTeamOnStart()    { return m_teamOnStart; }
// dispatches the virtual setControledById (slot 100) - a subclass override would see both.
void ObjectSpawner::setControlPointId(int id) { setControledById(id); }
int  ObjectSpawner::getControlPointId() { return m_controlPointId; }
void ObjectSpawner::setVisibleTeam(int team) { m_visibleTeam = team; }
int  ObjectSpawner::getVisibleTeam()    { return m_visibleTeam; }
void ObjectSpawner::setSpawnOffset(Vec3 offset) { m_spawnOffset = offset; }
Vec3 ObjectSpawner::getSpawnOffset()    { return m_spawnOffset; }

// reset - 0x818FE00, 223 B: back to the template's start state - active, lastSpawned -1,
// nrOfObjectToSpawn, holdObject, team = teamOnStart, updateFlags(0x800, 2) (through the vtable),
// every slot id -1 with ttl = the template's timeToLive, and the same active/spawnDelayAtStart
// fork as the ctor's server arm.
void ObjectSpawner::reset()
{
    m_active = m_template->m_activeOnStart;          // +0x1C4
    m_lastSpawnedId = -1;
    m_nrOfObjectToSpawn = m_template->m_nrOfObjectToSpawn;   // +0x19C
    m_holdObject = m_template->m_holdObject;         // +0x1C0
    m_team = m_teamOnStart;
    updateFlags(0x800u, 2u);                         // slot 24 (+0x60) - virtual here
    for (unsigned int i = 0; i < m_spawnedObjectIds.size(); ++i)   // size re-read each iteration
    {
        m_spawnedObjectIds[i] = -1;
        m_timeToLive[i] = m_template->m_timeToLive;  // +0x190
    }
    if (m_active)
    {
        if (m_template->m_spawnDelayAtStart)         // +0x1C3
            calcSpawnDelay();
        else
            m_spawnDelay = -1.0f;
    }
}

// Retains the player/settings queries but calculates min + (max - min) * 0.0f.
void ObjectSpawner::calcSpawnDelay()
{
    playerManager->getNumberOfPlayers();             // result unused
    int maxPlayers = 16;
    settings->intGet("GSMaxNrOfPlayers", maxPlayers);   // read, unused
    m_spawnDelay = m_template->m_minSpawnDelay
                 + (m_template->m_maxSpawnDelay - m_template->m_minSpawnDelay) * 0.0f;
}

// getCollisionMesh - 0x818EF90, 307 B: the per-team lazy cache. The int argument is ignored -
// the slot index is m_team (read three times, as shipped). While the slot is still "valid" and
// empty: the template's object template for that team (m_objectTemplates[team].resolved) names
// its collision mesh (SimpleObjectTemplate::getCollisionMesh, +0x9C - "" when unresolved), and
// collisionManager->getCollisionMeshPart(name, 0) fills the slot; a null result marks the slot
// invalid so nobody retries. abort() if there is no collision manager.
ICollisionMesh* ObjectSpawner::getCollisionMesh(int /*unusedIndex*/)
{
    const int team = m_team;
    if (m_collisionMeshes[team].valid)
    {
        if (!collisionManager)
            std::abort();
        if (!m_collisionMeshes[team].mesh)
        {
            const IObjectTemplate* spawnTemplate = m_template->m_objectTemplates[team].resolved;
            std::string meshName;
            if (spawnTemplate)
                meshName = static_cast<const SimpleObjectTemplate*>(spawnTemplate)->getCollisionMesh();   // +0x9C
            m_collisionMeshes[m_team].mesh = collisionManager->getCollisionMeshPart(meshName, 0);   // +0x2C
            if (!m_collisionMeshes[m_team].mesh)
                m_collisionMeshes[m_team].valid = false;
        }
    }
    return m_collisionMeshes[m_team].mesh;
}

// 28 / 17 B: the template's useButtonRadius (+0x1A8) - a positive radius IS the use button.
bool  ObjectSpawner::getHasUseButton()    { return m_template->m_useButtonRadius > 0.0f; }
float ObjectSpawner::getUseButtonRadius() { return m_template->m_useButtonRadius; }
int   ObjectSpawner::getLastSpawned() const { return m_lastSpawnedId; }
void  ObjectSpawner::dropSpawnedObject()  { m_holdObject = false; }
int   ObjectSpawner::getHoldObjectId()    { return m_holdObject ? m_lastSpawnedId : -1; }

// Server spawn scheduling, geometry snapping and soldier displacement helpers.
namespace {
int*   const g_objectSpawnerDrawLines   = Vars::getInt("objectspawner-draw-lines", 0);        // ds:0x8AAD438
int*   const g_objectSpawnerNumTries    = Vars::getInt("objectspawner-num-tries", 7);         // ds:0x8AAD43C
float* const g_objectSpawnerInnerCircle = Vars::getFloat("objectspawner-inner-circle", 0.3f); // ds:0x8AAD440

// Reads the concrete armor component's template to test whether it can be destroyed.
bool armorTemplateCanBeDestroyed(IArmorGLComp* armor)
{
    return static_cast<ArmorGLComp*>(armor)->getTemplateForSpawner()->getCanBeDestroyed();
}

// The first armor component on the object OR ITS parent chain - the ELF's recurring inline walk
// (`while (!(comp = obj+0x3C)) obj = obj+0x28`) - as the armor interface. Null when nobody has one.
inline IArmorGLComp* armorOf(IObject* object)
{
    for (IObject* o = object; o; o = o->getParent())
        if (IObjectComponent* comp = object_detail::LayoutAccess::armorComponent(o))
            return static_cast<IArmorGLComp*>(comp);
    return 0;
}

// The first IPlayerControlObject on the object or its parent chain - the other recurring walk.
inline IPlayerControlObject* pcoOf(IObject* object)
{
    for (IObject* o = object; o; o = o->getParent())
        if (void* pco = o->queryInterface(IID_IPlayerControlObject))
            return static_cast<IPlayerControlObject*>(pco);
    return 0;
}

inline Vec3 V3(float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }   // Vec3 is an aggregate here

inline float distanceBetween(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dz * dz + dy * dy + dx * dx);
}
} // namespace

extern IBase* heightmapCluster;                   // ds:0x8ABAF24 - Core/Globals.cpp (an IHeightmapCluster)

// The debug counter moveIntersectingSoldiers bumps (0x8987BE4, `dice::hfe::world::count`).
int count = 0;

// depth-first through children, then siblings - the first object whose template
// class is CID_EngineTemplate. `out` is left alone when there is none.
void getEngineFromChildsAndSiblings(IObject* object, IObject*& out)
{
    while (object)
    {
        if (object->getTemplate()->getClassID() == CID_EngineTemplate)
        {
            out = object;
            return;
        }
        getEngineFromChildsAndSiblings(object->getChild(), out);   // +0x2C.. the child link (slot 60)
        if (out)
            return;
        object = object->getSibling();                             // +0x2C via getSibling
    }
}

// Updates active server spawn slots, countdowns and held objects. Processes at most one empty slot per tick.
// A nearby live vehicle pauses respawn; failed spawning retries after one second. Clears the use-button latch afterward.
void ObjectSpawner::handleUpdate(float dt, unsigned int)
{
    if (!game->queryInterface(IID_IGameServer))
        return;
    if (!m_active)                                       // +0x180
        return;
    if (!m_nrOfObjectToSpawn)                            // +0x198
        return;

    bool firstEmptySlot = true;
    for (unsigned int i = 0; i < m_spawnedObjectIds.size(); ++i)
    {
        const ObjectSpawnerTemplate* t = m_template;
        if (t->m_byte1C2)                                // +0x1C2 the convert step's "never" latch
            continue;
        if (t->m_objectTemplates[m_team].name.empty() || !t->m_objectTemplates[m_team].resolved)
            continue;

        if (m_spawnedObjectIds[i] != -1)
        {
            // a filled slot
            if (t->m_maxSpawnDelay == -1.0f || t->m_minSpawnDelay == -1.0f)   // +0x18C +0x188
                return;                                  // the whole update, as shipped
            IObject* object = objectManager->getObjectFromGrid(m_spawnedObjectIds[i]);
            if (!object)
            {
                m_spawnedObjectIds[i] = -1;
            }
            else
            {
                IArmorGLComp* armor = armorOf(object);
                if (armor && armor->isDestroyed())
                    m_spawnedObjectIds[i] = -1;
            }
            continue;
        }

        // an empty slot - the first one only
        if (!firstEmptySlot)
            continue;
        if (m_spawnDelay <= 0.0f)                        // +0x170
        {
            if (m_lastSpawnedId != -1)
            {
                IObject* last = objectManager->getObjectFromGrid(m_lastSpawnedId);
                if (last)
                {
                    IArmorGLComp* armor = armorOf(last);
                    if (armor && armor->isDestroyed())
                    {
                        const float distance = fabsf(distanceBetween(last->getAbsolutePosition(),
                                                                     getAbsolutePosition()));
                        if (t->m_radius > distance)      // +0x1A4
                            game->destroyObject(last);   // IGame +0xA0 - clear the wreck
                    }
                }
            }
            if (!getHasUseButton() || (getHasUseButton() && m_useButtonPressed))
            {
                const int id = spawnObject();
                m_spawnedObjectIds[i] = id;
                m_lastSpawnedId = id;
                if (id == -1)
                    m_spawnDelay = 1.0f;
            }
        }
        else if (m_lastSpawnedId == -1)
        {
            m_spawnDelay -= dt;
        }
        else
        {
            IObject* last = objectManager->getObjectFromGrid(m_lastSpawnedId);
            if (!last)
            {
                m_lastSpawnedId = -1;
                m_spawnDelay -= dt;
            }
            else
            {
                IArmorGLComp* armor = armorOf(last);
                if (armor && armor->isDestroyed())
                {
                    m_spawnDelay -= dt;
                }
                else
                {
                    const float distance = fabsf(distanceBetween(last->getAbsolutePosition(),
                                                                 getAbsolutePosition()));
                    if (distance > m_template->m_radius)     // +0x1A4
                        m_spawnDelay -= dt;
                }
            }
        }
        firstEmptySlot = false;
    }

    // the hold arm
    if (m_holdObject)                                    // +0x1B0
    {
        IObject* held = objectManager->getObjectFromGrid(m_lastSpawnedId);
        if (held)
        {
            IArmorGLComp* armor = armorOf(held);
            bool release = false;
            if (armor && armor->isDestroyed())
                release = true;
            else
            {
                IObject* engine = 0;
                getEngineFromChildsAndSiblings(held, engine);
                // The CID_EngineTemplate object is an EngineBase; the PE reads m_input at retail
                // +0x1C4. The named member, because this build's layout is not retail's (m_input
                // sits at +0x1D0 on x86 here). flt_888CA90 = 0.1.
                if (!engine || fabsf(static_cast<EngineBase*>(engine)->m_input) >= 0.1f)
                    release = true;
            }
            if (release)
                m_holdObject = false;
            else
            {
                IObject* root = getRootParent(this);
                Vec3 speed = V3(0.0f, 0.0f, 0.0f);
                if (root)
                {
                    IPhysicsNode* rootNode = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(root));
                    if (rootNode)
                        speed = rootNode->getTangentSpeed(held->getAbsolutePosition());   // +0xA4
                }
                if (IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(held)))
                {
                    node->reset(true);                   // +0x118
                    node->setPositionalSpeed(speed);     // +0x68
                }
                Mat4 transform = getAbsoluteTransformation();          // slot 33
                transform.m[12] += m_spawnOffset.x;                    // +0x1A0
                transform.m[13] += m_spawnOffset.y;
                transform.m[14] += m_spawnOffset.z;
                held->setAbsoluteTransformation(transform);            // slot 35
                if (root)
                {
                    if (IPlayerControlObject* rootPco = pcoOf(root))
                        static_cast<SimpleObject*>(held)->m_replicationState =
                            static_cast<unsigned char>(rootPco->getTeam());   // +0x134 <- ipco +0x98
                }
            }
            if (!m_holdObject)
            {
                // released this tick: leave the object with the carrier's speed and awake
                IObject* root = getRootParent(this);
                Vec3 speed = V3(0.0f, 0.0f, 0.0f);
                if (root)
                {
                    IPhysicsNode* rootNode = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(root));
                    if (rootNode)
                        speed = rootNode->getTangentSpeed(held->getAbsolutePosition());
                }
                if (IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(held)))
                {
                    node->setSleepiness(0);              // +0x124
                    node->setPositionalSpeed(speed);     // +0x68
                }
                if (IPlayerControlObject* heldPco = pcoOf(held))
                {
                    heldPco->setObjectSpawnerHolding(false);   // ipco +0xC0
                    static_cast<SimpleObject*>(held)->m_replicationState = 0;   // +0x134
                }
            }
        }
    }
    m_useButtonPressed = false;                          // +0x19C
}

// Creates the team's configured object and returns its id, or -1 when blocked or unavailable.
// Repeated unoccupied blockers may be destroyed after the retry threshold.
// Successful spawning applies placement, effects, lifetime and vehicle-spawner bookkeeping.
int ObjectSpawner::spawnObject()
{
    const int team = m_team;                                     // +0x184
    const ObjectSpawnerTemplate* t = m_template;                 // +0x158
    if (t->m_objectTemplates[team].name.empty())
        return -1;
    const IObjectTemplate* spawnTemplate = t->m_objectTemplates[team].resolved;
    const int vehicleTeam = t->m_teamOnVehicle ? team : 0;       // +0x1C1
    int spawnedId = -1;

    std::vector<IObject*> blocking;
    ICollisionMesh* mesh = getCollisionMesh(team);
    if (mesh)
    {
        Mat4 transform = getAbsoluteTransformation();            // slot 33
        Vec3 position = V3(transform.m[12] + m_spawnOffset.x,
                      transform.m[13] + m_spawnOffset.y,
                      transform.m[14] + m_spawnOffset.z);
        Vec3 snapped = V3(0.0f, 0.0f, 0.0f);
        if (getSnappedPosition(snapped))
            position = snapped;

        ObjectInterfacePredicator pcoPredicator(0x100u, IID_IPlayerControlObject);
        std::vector<IObject*> found;
        objectManager->getObjectsFromGrid(position, 10.0f, found, &pcoPredicator);   // +0x40
        for (unsigned int i = 0; i < found.size(); ++i)
        {
            if (!checkIfObjectIntersects(mesh, transform, found[i]))
                continue;
            IPlayerControlObject* pco = static_cast<IPlayerControlObject*>(
                found[i]->queryInterface(IID_IPlayerControlObject));
            if (!pco)
                continue;
            if (pco->isOccupied())                               // ipco +0x18
                return -1;
            blocking.push_back(found[i]);
        }
    }

    if (!blocking.empty())
    {
        if (m_blockedSpawnCount <= 4)
        {
            ++m_blockedSpawnCount;                                         // the blocked-tick count
        }
        else
        {
            ObjectInterfacePredicator soldierPredicator(0x100u, IID_ISoldier);
            std::vector<IObject*> soldiers;
            objectManager->getObjectsFromGrid(getAbsolutePosition(), 7.0f, soldiers, &soldierPredicator);
            if (soldiers.empty())
            {
                for (unsigned int i = 0; i < blocking.size(); ++i)
                    game->destroyObject(blocking[i]);            // IGame +0xA0
            }
        }
        return -1;
    }

    m_blockedSpawnCount = 0;
    float rz = 0.0f, rx = 0.0f, ry = 0.0f;
    getRotationZXY(getAbsoluteTransformation(), rz, rx, ry);
    const Vec3 rotation = V3(-ry, -rx, -rz);
    const Vec3& here = getAbsolutePosition();
    const Vec3 position = V3(m_spawnOffset.x + here.x, m_spawnOffset.y + here.y, m_spawnOffset.z + here.z);
    IObject* object = gameLogic->spawnObject(spawnTemplate, position, rotation, vehicleTeam);   // +0x11C [S]
    if (object)
    {
        playEffectOnObjectSpawner();
        if (object->getFlags() & 8u)
            snapSpawnedObject(object);
        IArmorGLComp* armor = armorOf(object);
        if (!armor || armorTemplateCanBeDestroyed(armor))
            moveIntersectingSoldiers(object);
        if (m_nrOfObjectToSpawn > 0)                             // +0x198
        {
            if (--m_nrOfObjectToSpawn == 0)
                updateFlags(0, 0x800);                           // leave the update set
        }
        spawnedId = static_cast<int>(object->getObjectId());     // +0x10
        calcSpawnDelay();
        if (IPlayerControlObject* pco = pcoOf(object))
        {
            pco->setObjectSpawnerId(static_cast<int>(getObjectId()));   // +0xB8
            pco->setTimeToLiveUnused(t->m_timeToLive);           // +0xA0 <- template +0x190
            pco->setTimeSpawned(static_cast<float>(g_system->getExactTime())); // +0xB0
            if (t->m_holdObject)                                 // +0x1C0
            {
                pco->setObjectSpawnerHolding(true);              // +0xC0
                if (IObject* root = getRootParent(this))
                {
                    if (IPlayerControlObject* rootPco = pcoOf(root))
                        static_cast<SimpleObject*>(object)->m_replicationState =
                            static_cast<unsigned char>(rootPco->getTeam());   // +0x134
                }
            }
        }
    }
    m_holdObject = t->m_holdObject;                              // +0x1B0 <- +0x1C0, on every path
    return spawnedId;
}

// the spawn effect (template effectOnSpawn, resolved in the ctor), played at this
// position with the world up axis (0, 1, 0).
void ObjectSpawner::playEffectOnObjectSpawner()
{
    if (!m_effectOnSpawnTemplate)                                // +0x1AC
        return;
    const Vec3 up = V3(0.0f, 1.0f, 0.0f);
    gameLogic->playEffect(getAbsolutePosition(), m_effectOnSpawnTemplate, up);   // +0xE0 [S]
}

// getSnappedPosition - 0x8190010, 830 B. Where the object should land: from the spawn point (this
// transform + offset), a ray from the mesh's top (bounding-box max.y along this UP axis) down to its
// bottom (min.y), against flag-0x1000100 / not-flag-8 geometry (ObjectFlagPredicatorEx). A hit ->
// out = hit - bottom offset, true. No hit -> the terrain: if the object's bottom would end up
// below the heightmap at (x, z), out = (x, height - bottom.y, z), true; else false (no snap).
// Nothing without a collision mesh for the team.
bool ObjectSpawner::getSnappedPosition(Vec3& out)
{
    Mat4 transform = getAbsoluteTransformation();                // slot 33
    transform.m[12] += m_spawnOffset.x;
    transform.m[13] += m_spawnOffset.y;
    transform.m[14] += m_spawnOffset.z;
    ICollisionMesh* mesh = getCollisionMesh(0);
    if (!mesh)
        return false;

    const LocalBoundingBox* box = static_cast<const LocalBoundingBox*>(mesh->getBoundingBox(1));   // slot 22
    const Vec3 up = V3(transform.m[4], transform.m[5], transform.m[6]);
    const Vec3 top = V3(up.x * box->m_max.y, up.y * box->m_max.y, up.z * box->m_max.y);
    const Vec3 bottom = V3(up.x * box->m_min.y, up.y * box->m_min.y, up.z * box->m_min.y);

    ObjectFlagPredicatorEx predicator(0x1000100u, 8u);
    IObject* hitObject = 0;
    Vec3 hitPoint = V3(0.0f, 0.0f, 0.0f);
    Vec3 hitNormal = V3(0.0f, 0.0f, 0.0f);
    int hitPart = 0;
    const Vec3 start = V3(top.x + transform.m[12], top.y + transform.m[13], top.z + transform.m[14]);
    const Vec3 direction = V3(bottom.x - top.x, bottom.y - top.y, bottom.z - top.z);
    objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, start, direction,
                                 &predicator, true, false, false, true, 0);   // +0x68; object-line path live
    if (hitObject)
    {
        out.x = hitPoint.x - bottom.x;
        out.y = hitPoint.y - bottom.y;
        out.z = hitPoint.z - bottom.z;
        return true;
    }
    const float height = reinterpret_cast<IHeightmapCluster*>(heightmapCluster)
                             ->getHeightInWorldCoords(transform.m[12], transform.m[14]);   // +0x6C
    if (-bottom.y <= transform.m[13] - height)
        return false;
    out.x = transform.m[12];
    out.y = height - bottom.y;
    out.z = transform.m[14];
    return true;
}

// snapSpawnedObject - 0x8190350, 788 B: getSnappedPosition's twin on the spawned object - its own
// transform, its own full bounding box (slot 40), the ray ignoring the object itself
// (ObjectIgnorePredicator, flags 0x100); a hit or a terrain correction moves the object
// (setAbsolutePosition, slot 28). Nothing for a null object or one without a collision mesh.
void ObjectSpawner::snapSpawnedObject(IObject* object)
{
    if (!object)
        return;
    if (!object_detail::LayoutAccess::collisionMesh(object))     // +0x40
        return;
    const Mat4& transform = object->getAbsoluteTransformation();
    const LocalBoundingBox* box = object->getFullBoundingBox();  // slot 40
    const Vec3 up = V3(transform.m[4], transform.m[5], transform.m[6]);
    const Vec3 top = V3(up.x * box->m_max.y, up.y * box->m_max.y, up.z * box->m_max.y);
    const Vec3 bottom = V3(up.x * box->m_min.y, up.y * box->m_min.y, up.z * box->m_min.y);

    ObjectIgnorePredicator predicator(0x100u, object);
    IObject* hitObject = 0;
    Vec3 hitPoint = V3(0.0f, 0.0f, 0.0f);
    Vec3 hitNormal = V3(0.0f, 0.0f, 0.0f);
    int hitPart = 0;
    const Vec3 start = V3(top.x + transform.m[12], top.y + transform.m[13], top.z + transform.m[14]);
    const Vec3 direction = V3(bottom.x - top.x, bottom.y - top.y, bottom.z - top.z);
    objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, start, direction,
                                 &predicator, true, false, false, true, 0);   // object-line path live
    if (hitObject)
    {
        object->setAbsolutePosition(V3(hitPoint.x - bottom.x, hitPoint.y - bottom.y, hitPoint.z - bottom.z));
        return;
    }
    const float height = reinterpret_cast<IHeightmapCluster*>(heightmapCluster)
                             ->getHeightInWorldCoords(transform.m[12], transform.m[14]);
    if (-bottom.y <= transform.m[13] - height)
        return;
    object->setAbsolutePosition(V3(transform.m[12], height - bottom.y, transform.m[14]));
}

// Pushes nearby intersecting soldiers away from the spawned object, retrying collision checks.
// Blocked displacement falls back to a vehicle exit position when available, then resets soldier physics.
void ObjectSpawner::moveIntersectingSoldiers(IObject* object)
{
    IPlayerControlObject* objectPco = pcoOf(object);
    ObjectInterfacePredicator soldierPredicator(0x100u, IID_ISoldier);
    std::vector<IObject*> soldiers;
    const float radius = object->getPhysicBoundingRadius();      // slot 38
    objectManager->getObjectsFromGrid(object->getAbsolutePosition(), radius, soldiers, &soldierPredicator);

    for (unsigned int i = 0; i < soldiers.size(); ++i)
    {
        IObject* soldier = soldiers[i];
        const float innerRadius = object->getPhysicBoundingRadius() * *g_objectSpawnerInnerCircle;
        Vec3 objectPos = object->getAbsolutePosition();
        Vec3 soldierPos = soldier->getAbsolutePosition();
        Vec3 delta = V3(soldierPos.x - objectPos.x, soldierPos.y - objectPos.y, soldierPos.z - objectPos.z);
        const float distance = sqrtf(delta.z * delta.z + delta.y * delta.y + delta.x * delta.x);
        if (innerRadius > distance)
        {
            delta.y = 0.0f;
            const float lengthSq = delta.z * delta.z + delta.x * delta.x;
            if (fabsf(lengthSq - 1.0f) >= 1.1920929e-7f)      // normalise unless already unit
            {
                if (fabsf(lengthSq) >= 1.1920929e-7f)
                {
                    const float inv = 1.0f / sqrtf(lengthSq);
                    delta.x *= inv; delta.y *= inv; delta.z *= inv;
                }
                else
                {
                    delta.x = 0.0f; delta.z = 0.0f;
                }
            }
            const float push = innerRadius - distance;
            const Vec3& p = soldier->getAbsolutePosition();
            soldier->setAbsolutePosition(V3(delta.x * push + p.x, delta.y * push + p.y, delta.z * push + p.z));
            if (*g_objectSpawnerDrawLines)
            {
                soldier->getAbsolutePosition();
                ++count;
            }
        }

        // the flat step direction and the total step budget
        soldierPos = soldier->getAbsolutePosition();
        objectPos = object->getAbsolutePosition();
        Vec3 direction = V3(soldierPos.x - objectPos.x, 0.0f, soldierPos.z - objectPos.z);
        const float remaining = (object->getPhysicBoundingRadius() + 0.5f)
                              - sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        {
            const float lengthSq = direction.z * direction.z + direction.y * direction.y + direction.x * direction.x;
            if (fabsf(lengthSq - 1.0f) >= 1.1920929e-7f)
            {
                if (fabsf(lengthSq) >= 1.1920929e-7f)
                {
                    const float inv = 1.0f / sqrtf(lengthSq);
                    direction.x *= inv; direction.y *= inv; direction.z *= inv;
                }
                else
                {
                    direction.x = direction.y = direction.z = 0.0f;
                }
            }
        }
        const Vec3 startPos = soldierPos;                        // where the stepping starts
        Vec3 stepped = startPos;
        bool intersects = checkIfObjectIntersectsSoldier(object, soldier, true);
        for (int attempt = 1; attempt <= *g_objectSpawnerNumTries && intersects; ++attempt)
        {
            const float along = remaining / (float)*g_objectSpawnerNumTries * (float)attempt;
            stepped = V3(direction.x * along + startPos.x, direction.y * along + startPos.y,
                           direction.z * along + startPos.z);
            if (*g_objectSpawnerDrawLines)
            {
                soldier->getAbsolutePosition();
                ++count;
            }
            soldier->setAbsolutePosition(stepped);
            intersects = checkIfObjectIntersectsSoldier(object, soldier, true);
        }

        const Vec3& now = soldier->getAbsolutePosition();
        const bool unmoved = fabsf(startPos.x - now.x) <= 1.1920929e-7f
                          && fabsf(startPos.y - now.y) <= 1.1920929e-7f
                          && fabsf(startPos.z - now.z) <= 1.1920929e-7f;
        bool rayHit = false;
        if (!unmoved)
            rayHit = checkIfRayHitAnyObject(object->getAbsolutePosition(), soldier->getAbsolutePosition(),
                                            object, true) != 0;
        if (objectPco && (intersects || rayHit))
        {
            IPlayerControlObject* soldierPco = pcoOf(soldier);
            IPlayer* occupant = soldierPco ? soldierPco->getOccupingPlayer() : 0;   // +0x10
            IObject* occupantVehicle = occupant ? occupant->getVehicle() : 0;         // IPlayer +0x48
            if (!occupant || occupantVehicle == occupant->getVehicle())    // the shipped self-compare
            {
                Vec3 exitPosition = V3(0.0f, 0.0f, 0.0f);
                if (objectPco->isAllowedToExit())                // +0xA4
                    objectPco->getAltExitPosition(exitPosition); // +0x50
                else
                    objectPco->getExitPosition(exitPosition);    // +0x4C
                soldier->setAbsolutePosition(exitPosition);
            }
        }
        if (IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(soldier)))
            node->reset(true);                                   // +0x118
    }
}

// . The live team gates the saved start-team value; the
// stream's short-write result is deliberately ignored after the base succeeds.
bool ObjectSpawner::makeScript(io::IStream* stream)
{
    if (!Object::makeScript(stream)) return false;
    std::ostringstream out;
    if (m_controlPointId != -1) out << "Object.setControlPointId " << m_controlPointId << std::endl;
    if (m_team) out << "Object.setTeam " << static_cast<unsigned int>(m_teamOnStart) << std::endl;
    const std::string text = out.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
    return true;
}

} // namespace world
} // namespace hfe
} // namespace dice
