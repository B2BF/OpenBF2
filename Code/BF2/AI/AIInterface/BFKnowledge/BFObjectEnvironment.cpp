// Engine object queries and information-grid updates used by the AI.
// [RETIRE-WHEN: aidll-01-adapters] Remove when the corresponding implementation is complete and validated.

#include "BFObjectEnvironment.h"

#include "AIFrontier.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/AIUtils/IAimerHelper.h"
#include "../../../Core/ClassManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods
#include <cstring>
#include <set>
#include "IAIObject.h"
#include "IAIObjectPlugIn.h"                         // IAIObjectUnit::isOccupied
#include "../../../Scene/Object/PlayerManager.h"   // isValidPlayerId's one call
#include "../../../Core/Ids.h"                     // the four container CIDs, IID_IPlayerControlObject
#include "../../../Game/Objects/IObject.h"         // getTemplate, getAIComponent
#include "../../../Game/Objects/IObjectTemplate.h" // getClassID
#include "../../../Game/Objects/IPlayer.h"         // the per-player tests and getId
#include "../../../Game/Objects/Game/PlayerControlObject.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Game/IGameLogic.h"                                  // gameLogic: getPcoMap, giveDamage
#include "../../../Game/Objects/GameLogicComp/IArmorGLComp.h"          // getHitPoints / getMaxHitPoints
#include "../../../Game/Objects/GameLogicComp/ArmorGLCompTemplate.h"   // getCanBeDestroyed
#include "../../../Scene/Object/ObjectManager.h"   // getObjectFromName
#include "BFTerrain.h"                                                 // IAITerrain::intersectTerrain (+0x64)
#include "BFTimer.h"                                                   // IBFTimer::getGameTime
#include "../../../Game/GameLogic/CombatArea.h"                         // getAiCombatAreaLayer
#include "../../../Game/GameLogic/CombatAreaManager.h"
#include "../../../Game/Objects/DestroyableObjectTemplate.h"
#include "../../../Game/Objects/ObjectSpawnerTemplate.h"
#include "../../../Game/Objects/Game/TargetObject.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Game/IGame.h"
#include "../../../Core/Debug.h"
#include "../../../Math/LocalBoundingBox.h"
#include "AITemplate.h"
#include "AITemplateManager.h"
#include <cmath>
#include "IWeapon.h"                                                   // hasIndirectLineOfFire's weapon reads
#include "BFPhysics.h"                                                 // IAIPhysics::getGravity
#include "../../../Core/Random.h"                                      // Rand (ELF dice::hfe::Rand)
#include "../../../Game/Objects/Game/Soldier.h"                        // Soldier::getCameras
#include "../../../Game/Objects/ICameraObject.h"
#include "../../../Game/Objects/ObjectLayoutAccess.h"               // the cached collision mesh (+0x40)
#include "../../../Physics/ICollisionMesh.h"                           // the sense mesh's faces and vertices
#include "../../../Physics/MaterialManager.h"                          // RayCastStopPredicate::predicateSurface
#include "../../../Physics/IPhysicsNode.h"                             // isAsleep
#include "../../../Physics/Material.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include <cfloat>

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace world { extern IBase* materialManager; }   // ds:0x8AD2820 - Core/Globals.cpp

namespace ai {

// ELF 0x8400A30 / 0x8400A50.
TargetData::TargetData(unsigned int idValue, const Vec3& positionValue, IAIObject* objectValue)
    : id(idValue), position(positionValue), object(objectValue)
{
}

// ELF 0x8400A70 / 0x8400A90.
TargetData::TargetData(const TargetData& other)
    : id(other.id), position(other.position), object(other.object)
{
}

// ELF 0x8400AB0. The error can be filtered; then only id and object are copied.
TargetData& TargetData::operator=(const TargetData& other)
{
    Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/BFObjectEnvironment.cpp", 75)
        << "This does not work!";
    id = other.id;
    object = other.object;
    return *this;
}

namespace {

// ELF 0x840B200, 0x840B260, 0x840B270.
class DestroyableObjectsPredicate : public world::ObjectPredicator
{
public:
    virtual bool predicate(world::IObject* object)
    {
        world::IObjectTemplate* const objectTemplate = static_cast<world::IObjectTemplate*>(
            object->getTemplate()->queryInterface(world::IID_IObjectTemplate));
        return objectTemplate && objectTemplate->getClassID() == world::CID_DestroyableObjectTemplate
            && static_cast<world::DestroyableObjectTemplate*>(objectTemplate)->usedByAi;
    }
    virtual unsigned int getFlags() const { return 0x1000000; }
    virtual bool performCulling(world::IObject*) { return true; }
};

// ELF 0x840B4F0, 0x840B540, 0x840B550 (the AI TU's predicate).
class NoMobilePredicate : public world::ObjectPredicator
{
public:
    virtual bool predicate(world::IObject* object)
    {
        if (!object->hasCollisionLod(0))
            return false;
        world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(
            world::object_detail::LayoutAccess::physicsNode(object));
        return !node || !node->getIsMobile();
    }
    virtual unsigned int getFlags() const { return 0x1000000; }
    virtual bool performCulling(world::IObject*) { return true; }
};

// ELF 0x840B560, 0x840B590, 0x840B5A0.
class ObjectSpawnerPredicate : public world::ObjectPredicator
{
public:
    virtual bool predicate(world::IObject* object)
    {
        return object->getTemplate()->getClassID() == world::CID_ObjectSpawnerTemplate;
    }
    virtual unsigned int getFlags() const { return 0x1000000; }
    virtual bool performCulling(world::IObject*) { return true; }
};

// ELF 0x840A1A0's inlined comparison orders only the signed endpoint pair.
struct Edge
{
    int first, second, index;
    bool operator<(const Edge& other) const
    {
        return first == other.first ? second < other.second : first < other.first;
    }
};

// bf2 0x8400BC0. The object's cached AI component; failing that, for the four container templates
// the occupying object's. The four tests are independent ifs, each overwriting the answer when its
// occupying object exists, as the image runs them in sequence. The parachute's
// IPlayerControlObject is used unchecked.
IAIObject* getIAIObject(world::IObject* object)
{
    IAIObject* aiObject = static_cast<IAIObject*>(object->getAIComponent());   // +0x34
    if (aiObject)
        return aiObject;

    if (object->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
    {
        world::IObject* const occupying =
            static_cast<world::PlayerControlObject*>(object)->getOccupingObject();   // +0x18C
        if (occupying)
            aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
    }
    if (object->getTemplate()->getClassID() == world::CID_ParachuteTemplate)
    {
        world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        world::IObject* const occupying = control->getOccupingObject();             // +0x14
        if (occupying)
            aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
    }
    if (object->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate)
    {
        world::IObject* const occupying =
            static_cast<world::PlayerControlObject*>(object)->getOccupingObject();
        if (occupying)
            aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
    }
    if (object->getTemplate()->getClassID() == world::CID_GrapplingHookRopeContainerTemplate)
    {
        world::IObject* const occupying =
            static_cast<world::PlayerControlObject*>(object)->getOccupingObject();
        if (occupying)
            aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
    }
    return aiObject;
}

// The TU's own one-slot object filter (typeinfo without a base, vtable of one entry).
class IObjectPredicate
{
public:
    virtual bool predicate(world::IObject* object) = 0;   // +0x00
};

// bf2 0x840B1D0 (vtable 0x88D75F8): objects with a collision LOD 0 (IObject slot 51).
class CollisionPredicate : public IObjectPredicate
{
public:
    virtual bool predicate(world::IObject* object) { return object->hasCollisionLod(0); }
};

// bf2 0x840B450. Pre-order: the object itself when the predicate (if any) takes it, then every
// object down its getChild chain (IObject +0xF0, read literally: the chain is followed through
// getChild, not the sibling link), each recursively.
void getAllChildren(world::IObject* object, std::vector<world::IObject*>& out, IObjectPredicate* filter)
{
    if (!filter || filter->predicate(object))
        out.push_back(object);
    for (world::IObject* child = object->getChild(); child; child = child->getChild())
        getAllChildren(child, out, filter);
}

// bf2 0x840B310 / 0x840B360 (vtable 0x88D75B0): the line test's stop filter. Objects whose AI
// object is in `ignore` do not stop the line; any other object is remembered for predicateSurface.
// With `seeThroughStops` a see-through surface (material +0x7C) does not stop the line either, but
// it records the squared distance from `from` to that object's position when nearer than the best
// so far; every other surface stops it.
class RayCastStopPredicate : public world::ObjectPredicator
{
public:
    RayCastStopPredicate(const std::vector<IAIObject*>& ignore, bool seeThroughStops, const Vec3& from)
        : m_ignore(&ignore)
        , m_seeThroughStops(seeThroughStops)
        , m_nearestSeeThroughSq(FLT_MAX)
        , m_from(from)
    {
    }

    virtual bool predicate(world::IObject* object)
    {
        IAIObject* const aiObject = getIAIObject(object);
        if (aiObject)
        {
            for (std::vector<IAIObject*>::const_iterator it = m_ignore->begin(); it != m_ignore->end(); ++it)
                if (aiObject == *it)
                    return false;
        }
        m_object = object;
        return true;
    }
    virtual unsigned int getFlags() const { return 0x1000000; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int surface)
    {
        world::Material* const material = static_cast<world::MaterialManager*>(world::materialManager)
            ->getMaterialPtr(static_cast<unsigned int>(surface));                       // +0x24
        if (!m_seeThroughStops || !material->getIsSeeThrough())                          // +0x7C
            return true;
        const Vec3& position = m_object->getAbsolutePosition();                          // +0x6C
        Vec3 delta;
        delta.x = m_from.x - position.x;
        delta.y = m_from.y - position.y;
        delta.z = m_from.z - position.z;
        const float distanceSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        if (m_nearestSeeThroughSq > distanceSq)
            m_nearestSeeThroughSq = distanceSq;
        return false;
    }

    /* +0x04 */ const std::vector<IAIObject*>* m_ignore;
    /* +0x08 */ bool m_seeThroughStops;
    /* +0x0C */ world::IObject* m_object;          // left unset until predicate takes an object
    /* +0x10 */ float m_nearestSeeThroughSq;
    /* +0x14 */ Vec3 m_from;
};

// bf2 0x840B280 (vtable 0x88D75C8): the sphere pass's filter. Objects without an AI object pass;
// an object whose AI object is the primary object (IAIObject +0xFC) of a non-null `ignore` entry
// is refused. The flag is stored and never read.
class SphereExcludeFilter : public world::ObjectPredicator
{
public:
    SphereExcludeFilter(const std::vector<IAIObject*>& ignore, bool flag)
        : m_ignore(&ignore)
        , m_flag(flag)
    {
    }

    virtual bool predicate(world::IObject* object)
    {
        IAIObject* const aiObject = getIAIObject(object);
        if (!aiObject)
            return true;
        for (std::vector<IAIObject*>::const_iterator it = m_ignore->begin(); it != m_ignore->end(); ++it)
        {
            if (*it && (*it)->getPrimaryObject() == aiObject)
                return false;
        }
        return true;
    }
    virtual unsigned int getFlags() const { return 0x1000000; }
    virtual bool performCulling(world::IObject*) { return true; }

    /* +0x04 */ const std::vector<IAIObject*>* m_ignore;
    /* +0x08 */ bool m_flag;
};

// bf2 0x840B0C0 (vtable 0x88D7620): an engine-side wrapper of an AI filter. Objects without an AI
// object answer `noAIObject`; with one, the filter (if any) decides.
class AIPredWrapper : public world::ObjectPredicator
{
public:
    AIPredWrapper(AIObjectPredicate* filter, bool noAIObject)
        : m_filter(filter)
        , m_noAIObject(noAIObject)
    {
    }

    virtual bool predicate(world::IObject* object)
    {
        IAIObject* const aiObject = getIAIObject(object);
        if (!aiObject)
            return m_noAIObject;
        if (!m_filter)
            return true;
        return m_filter->predicate(aiObject);
    }
    virtual unsigned int getFlags() const { return 0x1000000; }
    virtual bool performCulling(world::IObject*) { return true; }

    /* +0x04 */ AIObjectPredicate* m_filter;
    /* +0x08 */ bool m_noAIObject;
};

// bf2 0x840B130 / 0x840B180 (vtable 0x88D7608): AIPredWrapper's test, and see-through surfaces
// (material +0x7C) do not stop a line.
class AISeeingPredWrapper : public world::ObjectPredicator
{
public:
    AISeeingPredWrapper(AIObjectPredicate* filter, bool noAIObject)
        : m_filter(filter)
        , m_noAIObject(noAIObject)
    {
    }

    virtual bool predicate(world::IObject* object)
    {
        IAIObject* const aiObject = getIAIObject(object);
        if (!aiObject)
            return m_noAIObject;
        if (!m_filter)
            return true;
        return m_filter->predicate(aiObject);
    }
    virtual unsigned int getFlags() const { return 0x1000000; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int surface)
    {
        world::Material* const material = static_cast<world::MaterialManager*>(world::materialManager)
            ->getMaterialPtr(static_cast<unsigned int>(surface));                       // +0x24
        return !material->getIsSeeThrough();                                             // +0x7C
    }

    /* +0x04 */ AIObjectPredicate* m_filter;
    /* +0x08 */ bool m_noAIObject;
};

// The image's inlined uniform draw, 2^-24 * ((((raw >> 7) | 1) + 1)), and the rounded index pick
// built on it: lo + (hi - lo) * draw + 0.5, truncated. [CHOSEN] helper spellings for inlined code.
inline float randomUnit()
{
    return static_cast<float>(static_cast<int>(((Rand.getRaw() >> 7) | 1u) + 1u)) * 5.9604645e-08f;
}

inline unsigned int randomIndex(float hi)
{
    const float lo = 0.0f;
    return static_cast<unsigned int>(static_cast<long long>(lo + (hi - lo) * randomUnit() + 0.5f));
}

// Row-vector point transform, p * m (m[12..14] the translation). [CHOSEN] spelling of the inlined
// operator.
inline Vec3 transformPoint(const Vec3& p, const Mat4& m)
{
    Vec3 out;
    out.x = p.x * m.m[0] + p.y * m.m[4] + p.z * m.m[8] + m.m[12];
    out.y = p.x * m.m[1] + p.y * m.m[5] + p.z * m.m[9] + m.m[13];
    out.z = p.x * m.m[2] + p.y * m.m[6] + p.z * m.m[10] + m.m[14];
    return out;
}

// Common topology in ELF 0x8405F70 and 0x8407300. IDs are relative to zero even on append.
void appendBoxTopology(IAIObjectEnvironment::Mesh& mesh)
{
    static const std::pair<int, int> edges[] = {
        {0, 1}, {0, 2}, {0, 4}, {0, 6}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {2, 3},
        {2, 6}, {2, 7}, {3, 5}, {3, 7}, {4, 5}, {4, 6}, {4, 7}, {5, 7}, {6, 7}
    };
    static const triple<int, int, int> faces[] = {
        {12, 10, 8}, {10, 17, 9}, {9, 3, 1}, {3, 14, 2}, {2, 6, 0}, {13, 7, 6},
        {17, 15, 14}, {15, 16, 13}, {16, 12, 11}, {11, 5, 7}, {8, 4, 5}, {4, 1, 0}
    };
    mesh.edges.reserve(18);
    for (unsigned int i = 0; i < 18; ++i)
        mesh.edges.push_back(edges[i]);
    for (unsigned int i = 0; i < 12; ++i)
        mesh.faces.push_back(make_triple(faces[i].first, faces[i].second, faces[i].third));
}

} // namespace

// First wins in, exact match out.
IAIObjectEnvironment::IAIObjectEnvironment()
{
    if (!CommonSingletonServer::m_iaiObjectEnvironment)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IAIObjectEnvironment"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IAIObjectEnvironment::~IAIObjectEnvironment()
{
    if (CommonSingletonServer::m_iaiObjectEnvironment == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IAIObjectEnvironment"));
        BFSingletonServer::updateStaticMethods();
    }
}

// bf2 0x8400CA0: the three vectors default-constructed, the flag and severe-damage timestamp zeroed.
// Leaves padding at +0x29..+0x2B untouched.
BFObjectEnvironment::BFObjectEnvironment()
{
    m_byte28 = 0;                                // +0x28, a byte
    m_lastSevereDamageTime = 0.0;                // +0x2C, a double
}

// bf2 0x8400DA0: the members' destructors (the three vectors, last first).
BFObjectEnvironment::~BFObjectEnvironment()
{
}

// bf2 0x8400F50 (Hex-Rays; listing for the call). The frustum query (object manager +0x50, flag
// true, 0.0, 0.0) into the scratch list through AIPredWrapper(filter, false), then each AI object.
void BFObjectEnvironment::getWorldFrustumObjects(std::vector<IAIObject*>& out, const Frustum& frustum, AIObjectPredicate* filter) const
{
    std::vector<world::IObject*>& objects = const_cast<std::vector<world::IObject*>&>(m_senseObjects);
    objects.resize(0);
    AIPredWrapper wrapper(filter, false);
    world::objectManager->getObjectsFromGrid(frustum, objects, &wrapper, true, 0.0f, 0.0f);   // +0x50
    pushAIObjects(out, objects);
}

// bf2 0x84010C0 (Hex-Rays). As getWorldFrustumObjects with the sphere query (+0x40).
void BFObjectEnvironment::getWorldSphereObjects(std::vector<IAIObject*>& out, const Vec3& centre, float radius, AIObjectPredicate* filter) const
{
    std::vector<world::IObject*>& objects = const_cast<std::vector<world::IObject*>&>(m_senseObjects);
    objects.resize(0);
    AIPredWrapper wrapper(filter, false);
    world::objectManager->getObjectsFromGrid(centre, radius, objects, &wrapper);         // +0x40
    pushAIObjects(out, objects);
}

// bf2 0x8401230 (Hex-Rays). As getWorldFrustumObjects with the whole-grid query (+0x3C).
void BFObjectEnvironment::getAllWorldObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const
{
    std::vector<world::IObject*>& objects = const_cast<std::vector<world::IObject*>&>(m_senseObjects);
    objects.resize(0);
    AIPredWrapper wrapper(filter, false);
    world::objectManager->getObjectsFromGrid(objects, &wrapper);                         // +0x3C
    pushAIObjects(out, objects);
}

void BFObjectEnvironment::getAllDestroyableObjects(std::vector<DestroyableObject>& out) const
{
    // ELF 0x8401390. This query does not clear the scratch vector before dispatch.
    DestroyableObjectsPredicate predicate;
    std::vector<world::IObject*>& objects = const_cast<std::vector<world::IObject*>&>(m_senseObjects);
    world::objectManager->getObjectsFromGrid(objects, &predicate);
    for (std::vector<world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::DestroyableObjectTemplate* const objectTemplate =
            static_cast<world::DestroyableObjectTemplate*>(static_cast<world::IObjectTemplate*>(
                (*it)->getTemplate()->queryInterface(world::IID_IObjectTemplate)));
        DestroyableObject entry;
        entry.initiallyWalkable = objectTemplate->initiallyWalkable;
        entry.min = Vec3{objectTemplate->minCorner[0], objectTemplate->minCorner[1], objectTemplate->minCorner[2]};
        entry.max = Vec3{objectTemplate->maxCorner[0], objectTemplate->maxCorner[1], objectTemplate->maxCorner[2]};
        entry.transform = (*it)->getAbsoluteTransformation();
        out.push_back(entry);
    }
}

// bf2 0x8401670: every player in the player manager's list (+0x3C), its id (IPlayer +0x54).
void BFObjectEnvironment::getWorldPlayerIds(std::vector<int>& out) const
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        out.push_back((*it)->getId());
}

// bf2 0x8401700. Every alive, not man-down player with a vehicle contributes its vehicle's AI
// object (for ladder, zipline and grappling-hook containers the occupying object's, for a
// parachute the IPlayerControlObject's occupying object's, unchecked). A primary object (+0xF8)
// is taken directly (through the filter, if any); any other object contributes its primary object
// (+0xFC) to a set, and each set member whose unit is not occupied (IAIObjectUnit +0x48) is
// taken afterwards, also through the filter.
void BFObjectEnvironment::getWorldPlayerOccupiedObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    std::set<IAIObject*> primaries;
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;
        if (!player || !player->getIsAlive() || player->getIsManDown() || !player->getVehicle())
            continue;

        IAIObject* aiObject = static_cast<IAIObject*>(player->getVehicle()->getAIComponent());
        if (!aiObject)
        {
            const unsigned int classId = player->getVehicle()->getTemplate()->getClassID();
            if (classId == world::CID_LadderContainerTemplate)
            {
                world::IObject* const occupying =
                    static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (occupying)
                    aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }
            else if (player->getVehicle()->getTemplate()->getClassID() == world::CID_ParachuteTemplate)
            {
                world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
                    player->getVehicle()->queryInterface(world::IID_IPlayerControlObject));
                aiObject = static_cast<IAIObject*>(control->getOccupingObject()->getAIComponent());
            }
            else if (player->getVehicle()->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate
                     || player->getVehicle()->getTemplate()->getClassID()
                            == world::CID_GrapplingHookRopeContainerTemplate)
            {
                world::IObject* const occupying =
                    static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (occupying)
                    aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }
            if (!aiObject)
                continue;
        }

        if (aiObject->isPrimary())
        {
            if (!filter || filter->predicate(aiObject))
                out.push_back(aiObject);
        }
        else
        {
            primaries.insert(aiObject->getPrimaryObject());
        }
    }

    for (std::set<IAIObject*>::const_iterator it = primaries.begin(); it != primaries.end(); ++it)
    {
        IAIObject* const primary = *it;
        if (primary->getUnit()->isOccupied())
            continue;
        if (!filter || filter->predicate(primary))
            out.push_back(primary);
    }
}

// Collects AI objects matching the supplied predicate within the requested region.
void BFObjectEnvironment::getWorldPlayerOccupiedObjectsWithIds(std::vector<std::pair<int, IAIObject*> >& out, AIObjectPredicate* filter) const
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();

    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;

        if (!player || !player->getIsAlive() || player->getIsManDown() || !player->getVehicle())
            continue;

        IAIObject* aiObject = static_cast<IAIObject*>(player->getVehicle()->getAIComponent());

        if (!aiObject)
        {
            if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
            {
                world::IObject* occupying =
                    static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (occupying)
                    aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }
            else if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_ParachuteTemplate)
            {
                world::IPlayerControlObject* control = static_cast<world::IPlayerControlObject*>(
                    player->getVehicle()->queryInterface(world::IID_IPlayerControlObject));
                aiObject = static_cast<IAIObject*>(control->getOccupingObject()->getAIComponent());
            }
            else if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate ||
                     (*it)->getVehicle()->getTemplate()->getClassID() == world::CID_GrapplingHookRopeContainerTemplate)
            {
                world::IObject* occupying =
                    static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (occupying)
                    aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }

            if (!aiObject)
                continue;
        }

        if (filter)
        {
            if (filter->predicate(aiObject))
                out.push_back(std::make_pair((*it)->getId(), aiObject));
        }
        else
        {
            out.push_back(std::make_pair((*it)->getId(), aiObject));
        }
    }
}

// bf2 0x84032E0 (Hex-Rays): the local human player's vehicle's cached AI component.
IAIObject* BFObjectEnvironment::getLocalPlayerObject() const
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();          // +0x30
    if (!player)
        return 0;
    world::IObject* const vehicle = player->getVehicle();                                // +0x48
    return vehicle ? static_cast<IAIObject*>(vehicle->getAIComponent()) : 0;
}

// bf2 0x8403320 (Hex-Rays): the translation of the local human player's camera (IPlayer +0x180,
// as an IObject, unchecked).
bool BFObjectEnvironment::getLocalPlayerPosition(Vec3& position) const
{
    if (!world::playerManager)
        return false;
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    world::ICameraObject* const camera = player->getCamera();                            // +0x180
    if (!camera)
        return false;
    world::IObject* const object = static_cast<world::IObject*>(camera->queryInterface(world::IID_IObject));
    const Mat4& transform = object->getAbsoluteTransformation();
    position.x = transform.m[12];
    position.y = transform.m[13];
    position.z = transform.m[14];
    return true;
}

// bf2 0x84033A0 (Hex-Rays): the local human player's camera transformation (no player-manager
// null test here).
bool BFObjectEnvironment::getLocalPlayerCameraTransformation(Mat4& transform) const
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    world::ICameraObject* const camera = player->getCamera();
    if (!camera)
        return false;
    world::IObject* const object = static_cast<world::IObject*>(camera->queryInterface(world::IID_IObject));
    transform = object->getAbsoluteTransformation();
    return true;
}

// bf2 0x8403490 (Hex-Rays): the local human player's id, 0 without one.
int BFObjectEnvironment::getLocalPlayerId() const
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    return player ? player->getId() : 0;
}

// bf2 0x84034C0 (Hex-Rays): the ids of connected human players, rebuilt in the member list.
const std::vector<int>& BFObjectEnvironment::getActivePlayerIds() const
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();      // +0x3C
    std::vector<int>& ids = const_cast<std::vector<int>&>(m_activePlayerIds);
    ids.resize(0);
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        if (!(*it)->getIsAIPlayer() && (*it)->getIsConnected())                          // +0x64, +0x78
            ids.push_back((*it)->getId());
    }
    return m_activePlayerIds;
}

// Returns whether PlayerManager resolves the id to a player.
bool BFObjectEnvironment::isValidPlayerId(int id) const
{
    return world::playerManager->getPlayerFromId(id) != 0;
}

// bf2 0x8403600. The player after `id` (+0x24 getNextPlayerFromId) is answered when it is a
// connected human; AI players and disconnected ones are skipped by asking again with the same
// id, which returns the same player, so the loop ends at -1. Read literally.
int BFObjectEnvironment::getNextPlayerId(int id) const
{
    world::IPlayer* const first = world::playerManager->getNextPlayerFromId(id);
    if (!first)
        return -1;
    world::IPlayer* player = first;
    do
    {
        if (!player->getIsAIPlayer() && player->getIsConnected())
            return player->getId();
        player = world::playerManager->getNextPlayerFromId(id);
    }
    while (player != first);
    return -1;
}

// bf2 0x8401DD0 (ELF listing, Ghidra cross-check). The terrain first (IAITerrain +0x64
// intersectTerrain into zeroed scratch); a terrain hit is a hit. Otherwise the answer is the
// object test (this +0x90, collideLineWithObjects) with every argument forwarded. The image also
// computes `to - from` into a local that nothing reads; it is left out.
bool BFObjectEnvironment::collideLineWithWorld(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool a, float b, bool c, bool& hit)
{
    Vec3 terrainHit;
    terrainHit.x = 0.0f;
    terrainHit.y = 0.0f;
    terrainHit.z = 0.0f;
    Vec3 terrainNormal;
    terrainNormal.x = 0.0f;
    terrainNormal.y = 0.0f;
    terrainNormal.z = 0.0f;
    float distance;
    if (static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
            ->intersectTerrain(from, to, terrainHit, distance, terrainNormal))           // +0x64
        return true;
    return collideLineWithObjects(from, to, ignore, a, b, c, hit) != 0;                  // +0x90
}

// bf2 0x8401FF0 (listing). Objects first: intersectLine (+0x68, flags 1, 0, 0, 1, mask 0) along
// `to - from` through AISeeingPredWrapper(filter, true), or no predicate at all without a filter;
// then the terrain (IAITerrain +0x64). The hit point goes to `hit`, or a local when it is null.
bool BFObjectEnvironment::collideLineWithWorld(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, Vec3* hit)
{
    Vec3 localHit;
    localHit.x = 0.0f;
    localHit.y = 0.0f;
    localHit.z = 0.0f;
    Vec3& hitPoint = hit ? *hit : localHit;
    Vec3 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    normal.z = 0.0f;
    Vec3 direction;
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = to.z - from.z;
    AISeeingPredWrapper wrapper(filter, true);
    world::IObject* hitObject;
    int hitPart;
    if (world::objectManager->intersectLine(hitObject, hitPoint, normal, hitPart, from, direction,
                                            filter ? &wrapper : 0, true, false, false, true, 0))   // +0x68
        return true;
    float distance;
    return static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
               ->intersectTerrain(from, to, hitPoint, distance, normal) != 0;            // +0x64
}

// bf2 0x8401EC0 (Hex-Rays; listing for the calls). The first object hit along `to - from`
// (AIPredWrapper(filter, true), or none without a filter), else the terrain hit; `height` is that
// point's y. False when neither is hit (height untouched).
bool BFObjectEnvironment::getMaxHeight(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, float& height)
{
    AIPredWrapper wrapper(filter, true);
    Vec3 hitPoint;
    hitPoint.x = 0.0f;
    hitPoint.y = 0.0f;
    hitPoint.z = 0.0f;
    Vec3 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    normal.z = 0.0f;
    Vec3 direction;
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = to.z - from.z;
    world::IObject* hitObject;
    int hitPart;
    float distance;
    if (world::objectManager->intersectLine(hitObject, hitPoint, normal, hitPart, from, direction,
                                            filter ? &wrapper : 0, true, false, false, true, 0)
        || static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
               ->intersectTerrain(from, to, hitPoint, distance, normal))
    {
        height = hitPoint.y;
        return true;
    }
    return false;
}

// Engine ELF 0x84022F0 (amd-64 0x7715E0). Tests both segments through the helper's apex.
// shipped defect: this calls pure IAimerHelper slot 2, not AimerHelper's by-value-float slot 3.
// No AimerHelper is constructed in either AI image; no null guard is added to the retail call.
bool BFObjectEnvironment::hasIndirectLineOfFire(IWeapon* weapon, const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore)
{
    static Vec3 zero = { 0.0f, 0.0f, 0.0f };
    (void)zero;
    const float gravity = static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity();   // +0x08
    Vec3 predictedTarget = {0.0f, 0.0f, 0.0f};
    Vec3 velocity = {0.0f, 0.0f, 0.0f};
    Vec3 apex = {0.0f, 0.0f, 0.0f};
    const Vec3& weaponPosition = weapon->getPosition();                                  // +0x5C
    Vec3 delta;
    delta.x = to.x - from.x;
    delta.y = to.y - from.y;
    delta.z = to.z - from.z;
    const float scaledGravity = weapon->getGravityModifier() * gravity;                  // +0x80
    const float drag = weapon->getDrag();                                                // +0x7C
    const float exitVelocity = weapon->getExitVelocity();                                // +0x84
    if (!static_cast<IAimerHelper*>(CommonSingletonServer::m_iAimerHelper)->unimplementedAimerSlot2(
            &exitVelocity, &drag, &scaledGravity, &delta, &weaponPosition, &predictedTarget, &velocity, &apex))
        return false;
    bool hit;
    return !collideLineWithWorld(from, apex, ignore, false, 0.0f, false, hit)
        && !collideLineWithWorld(apex, to, ignore, false, 0.0f, false, hit);
}

// bf2 0x84024B0 (ELF listing; Hex-Rays cross-check). A random point on the object's collision
// surface, in world space (`position`) and in the root object's frame (`localPosition`). The
// scratch list is the root and its getChild chain with a collision LOD 0. While no mesh is found
// and there is more than one candidate, a random candidate's collision mesh (IObject +0x40) is
// taken when it has LOD 0; failing that the first candidate's, unchecked. With faces, a random
// face and a uniform point in it (the u + v > 1 half folded back); with vertices only, a random
// vertex; with neither (or no candidates) the object's position and a zero local point. `hint` is
// not read. The object's bounding radius is fetched and discarded on the fallback path.
void BFObjectEnvironment::pickRandomSensePosition(Vec3& position, Vec3& localPosition, Vec3 hint, IAIObject* object)
{
    world::IObject* const root = object->getObject();                                    // +0x20
    m_senseObjects.resize(0);

    CollisionPredicate collision;
    getAllChildren(root, m_senseObjects, &collision);

    if (!m_senseObjects.empty())
    {
        Mat4 rootInverse = root->getAbsoluteTransformation();                            // slot 33
        const Mat4 rootTransform = rootInverse;
        rootInverse.setInverse(rootTransform);

        world::ICollisionMesh* mesh = 0;
        world::IObject* chosen = 0;
        while (!mesh && m_senseObjects.size() > 1)
        {
            chosen = m_senseObjects[randomIndex(static_cast<float>(m_senseObjects.size()) - 1.0f)];
            if (chosen->hasCollisionLod(0))
                mesh = static_cast<world::ICollisionMesh*>(world::object_detail::LayoutAccess::collisionMesh(chosen));
        }
        if (!mesh)
        {
            chosen = m_senseObjects[0];
            if (chosen->hasCollisionLod(0))
                mesh = static_cast<world::ICollisionMesh*>(world::object_detail::LayoutAccess::collisionMesh(chosen));
        }

        const unsigned int faceCount = static_cast<unsigned int>(mesh->getFaceCount(0));   // slot 19
        const unsigned int vertexCount = mesh->getVertexCount(0);                         // slot 16
        Vec3 world;
        if (faceCount)
        {
            const unsigned int face = randomIndex(static_cast<float>(faceCount) - 1.0f);
            float u = randomUnit();
            float v = randomUnit();
            if (u + v > 1.0f)
            {
                u = 1.0f - u;
                v = 1.0f - v;
            }
            const unsigned short* const faces = static_cast<const unsigned short*>(mesh->getFaceVector(0));   // slot 20
            const Vec3* const vertices = static_cast<const Vec3*>(mesh->getVertexVector(0));                    // slot 17
            const unsigned short* const f = faces + face * 4;                              // 8-byte faces
            const Vec3& p0 = vertices[f[0]];
            Vec3 edge1;
            edge1.x = vertices[f[1]].x - p0.x;
            edge1.y = vertices[f[1]].y - p0.y;
            edge1.z = vertices[f[1]].z - p0.z;
            Vec3 edge2;
            edge2.x = vertices[f[2]].x - p0.x;
            edge2.y = vertices[f[2]].y - p0.y;
            edge2.z = vertices[f[2]].z - p0.z;
            localPosition.x = p0.x + edge1.x * u + edge2.x * v;
            localPosition.y = p0.y + edge1.y * u + edge2.y * v;
            localPosition.z = p0.z + edge1.z * u + edge2.z * v;
            world = transformPoint(localPosition, chosen->getAbsoluteTransformation());
        }
        else if (vertexCount)
        {
            const unsigned int vertex = randomIndex(static_cast<float>(vertexCount - 1));
            localPosition = static_cast<const Vec3*>(mesh->getVertexVector(0))[vertex];
            world = transformPoint(localPosition, chosen->getAbsoluteTransformation());
        }
        else
        {
            // The same fallback as an empty list (the image shares the block).
            object->getBoundingRadius();                                                         // +0xE8, unused
            localPosition.x = 0.0f;
            localPosition.y = 0.0f;
            localPosition.z = 0.0f;
            position = object->getPosition();                                                    // +0xD4
            return;
        }
        position = world;
        localPosition = transformPoint(world, rootInverse);
        return;
    }

    object->getBoundingRadius();                                                         // +0xE8, unused
    localPosition.x = 0.0f;
    localPosition.y = 0.0f;
    localPosition.z = 0.0f;
    position = object->getPosition();                                                    // +0xD4
}

// bf2 0x8403010 (listing; Hex-Rays cross-check), byte-identical to getSoldierSensePosition: for a
// soldier (object with an IObject, IAIObject +0xA4 isOfType(ITSoldier), template implementing
// CID_SoldierTemplate) the first camera of Soldier::getCameras (the map's leftmost entry, unchecked)
// as an IObject: its absolute position and its relative position. `part` is neither read nor written.
bool BFObjectEnvironment::pickSoldierRandomSensePosition(Vec3& position, Vec3& localPosition, IAIObject* object, unsigned int& part)
{
    if (!object->getObject() || !object || !object->isOfType(ITSoldier))                 // +0x20, +0xA4
        return false;
    world::IObject* const soldier = object->getObject();
    if (!const_cast<world::IObjectTemplate*>(soldier->getTemplate())
             ->queryImplementation(world::CID_SoldierTemplate))                          // +0x50
        return false;

    world::ICameraObject* const camera =
        static_cast<world::Soldier*>(soldier)->getCameras()->begin()->second;            // +0x1A4
    world::IObject* cameraObject = 0;
    if (camera)
        cameraObject = static_cast<world::IObject*>(camera->queryInterface(world::IID_IObject));
    position = cameraObject->getAbsolutePosition();                                      // +0x6C
    localPosition = cameraObject->getRelativePosition();                                 // +0xE0
    return true;
}

// bf2 0x8403100 (listing; Hex-Rays cross-check). See pickSoldierRandomSensePosition; `part` unused.
bool BFObjectEnvironment::getSoldierSensePosition(Vec3& position, Vec3& localPosition, IAIObject* object, unsigned int part)
{
    if (!object->getObject() || !object || !object->isOfType(ITSoldier))                 // +0x20, +0xA4
        return false;
    world::IObject* const soldier = object->getObject();
    if (!const_cast<world::IObjectTemplate*>(soldier->getTemplate())
             ->queryImplementation(world::CID_SoldierTemplate))                          // +0x50
        return false;

    world::ICameraObject* const camera =
        static_cast<world::Soldier*>(soldier)->getCameras()->begin()->second;            // +0x1A4
    world::IObject* cameraObject = 0;
    if (camera)
        cameraObject = static_cast<world::IObject*>(camera->queryInterface(world::IID_IObject));
    position = cameraObject->getAbsolutePosition();                                      // +0x6C
    localPosition = cameraObject->getRelativePosition();                                 // +0xE0
    return true;
}

// bf2 0x84031F0 (listing). `slope` is the physical plug-in's smallest radius (+0x94, +0x44);
// height and normal are the terrain's at the position's x/z (IAITerrain +0x28). The object's
// position and an isOfType(ITSoldier) answer are fetched, and a probe segment from radius above
// the object's height down by 2 * radius is built, but none of them is used. Returns 0.
int BFObjectEnvironment::getHeightAndNormalForObject(IAIObject* object, const Vec3& position, float& height, Vec3& normal, float& slope)
{
    const float radius = object->getPhysical()->getSmallestRadius();
    slope = radius;
    const Vec3 objectPosition = object->getPosition();                                   // +0xD4
    object->isOfType(ITSoldier);                                                         // +0xA4, unused
    Vec3 probeFrom;
    probeFrom.x = position.x;
    probeFrom.y = radius + objectPosition.y;
    probeFrom.z = position.z;
    Vec3 probeDelta;
    probeDelta.x = 0.0f;
    probeDelta.y = radius * -2.0f;
    probeDelta.z = 0.0f;
    (void)probeFrom;
    (void)probeDelta;
    Vec3 terrainNormal;
    terrainNormal.x = 0.0f;
    terrainNormal.y = 0.0f;
    terrainNormal.z = 0.0f;
    unsigned char material;
    height = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                 ->getHeightAndNormal(position.x, position.z, terrainNormal, material);   // +0x28
    normal = terrainNormal;
    return 0;
}

// ELF 0x8403770. The team parameter is unused; the object map is copied before iteration.
void BFObjectEnvironment::getVehicleSpawnerPositions(std::vector<Vec3>& out, unsigned char /*team*/) const
{
    const std::map<unsigned int, world::IObject*> objects = world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::IObject* const object = it->second;
        if (object->getNotInAI() || object->getTemplate()->getClassID() != world::CID_ObjectSpawnerTemplate)
            continue;
        const world::ObjectSpawnerTemplate* const spawner =
            static_cast<const world::ObjectSpawnerTemplate*>(object->getTemplate());
        for (unsigned int slot = 0; slot <= 2; ++slot)
        {
            const std::string name = spawner->getObjectTemplate(slot);
            world::IObjectTemplate* const objectTemplate = world::objectTemplateManager->getTemplate(name);
            if (!objectTemplate)
                break;
            world::SimpleObjectTemplate* const simple = static_cast<world::SimpleObjectTemplate*>(
                objectTemplate->queryImplementation(world::CID_SimpleObjectTemplate));
            if (simple)
            {
                const std::string aiName = simple->m_aiTemplate;
                if (!aiName.empty())
                {
                    AITemplate* const aiTemplate = AITemplateManager::m_instance->getTemplate(aiName);
                    if (aiTemplate && !(aiTemplate->m_types & 4) && aiTemplate->isMobile())
                        out.push_back(object->getAbsolutePosition());
                }
            }
        }
    }
}

// bf2 0x8403680: the object manager's name lookup (+0x24), its cached AI component (+0x34).
IAIObject* BFObjectEnvironment::getObjectFromName(const std::string& name)
{
    world::IObject* const object = world::objectManager->getObjectFromNameHash(name);
    return object ? static_cast<IAIObject*>(object->getAIComponent()) : 0;
}

// bf2 0x84036B0 (Hex-Rays; listing). The boundary query (+0x6C) over the primary terrain's
// extent (IAITerrain +0x54 .. +0x60, as floats).
bool BFObjectEnvironment::getAllStaticCollisionMeshes(std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const
{
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    Vec2 min;
    min.x = static_cast<float>(terrain->getPrimaryWorldMinX());
    min.y = static_cast<float>(terrain->getPrimaryWorldMinZ());
    Vec2 max;
    max.x = static_cast<float>(terrain->getPrimaryWorldMaxX());
    max.y = static_cast<float>(terrain->getPrimaryWorldMaxZ());
    return getAllStaticCollisionMeshesWithinBoundary(min, max, meshes, water, addOns, more, bounds, team);
}

bool BFObjectEnvironment::getAllStaticCollisionMeshesWithinBoundary(const Vec2& min, const Vec2& max, std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const
{
    // ELF 0x8403B20. The team parameter does not participate in this query.
    (void)team;
    std::vector<world::IObject*> objects;
    Vec3 boxMin = {0.0f, 0.0f, 0.0f}, boxMax = {0.0f, 0.0f, 0.0f};
    NoMobilePredicate noMobile;
    ObjectSpawnerPredicate spawners;
    vec2ToVec3XZ(min, boxMin);
    vec2ToVec3XZ(max, boxMax);
    world::objectManager->getObjectsInBoxFromGrid(boxMin, boxMax, objects, &noMobile);
    world::objectManager->getObjectsInBoxFromGrid(boxMin, boxMax, objects, &spawners);
    std::pair<Vec2, Vec2> combatBounds;
    combatBounds.first = Vec2{0.0f, 0.0f};
    combatBounds.second = Vec2{0.0f, 0.0f};
    createBoundingBoxFromCombatAreas(combatBounds);
    bounds = combatBounds;

    for (std::vector<world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::IObject* const object = *it;
        world::IObjectTemplate* const objectTemplate = static_cast<world::IObjectTemplate*>(
            object->getTemplate()->queryInterface(world::IID_IObjectTemplate));
        if (!objectTemplate)
            continue;
        const std::string& name = objectTemplate->getName();
        objectTemplate->getClassID(); // The shipped body discards this query.
        if (object->getNotInAI())
            continue;
        if (object->getTemplate()->getClassID() != world::CID_ObjectSpawnerTemplate)
        {
            if (hasOnlyInfantryCollMesh(object) || !objectOverlapsCombatAreas(object, bounds))
                continue;
            const Mat4& transform = object->getAbsoluteTransformation();
            if (object->hasCollisionLod(3))
            {
                world::ICollisionMesh* const collision = static_cast<world::ICollisionMesh*>(
                    world::object_detail::LayoutAccess::collisionMesh(object));
                if (objectTemplate->getClassID() == world::CID_DestroyableObjectTemplate)
                {
                    world::DestroyableObjectTemplate* const destroyable =
                        static_cast<world::DestroyableObjectTemplate*>(objectTemplate);
                    if (destroyable->usedByAi)
                    {
                        DestroyableAddOnMesh entry;
                        entry.name = object->getTemplate()->getName();
                        entry.initiallyWalkable = destroyable->initiallyWalkable;
                        const Vec3 low = {destroyable->minCorner[0], destroyable->minCorner[1], destroyable->minCorner[2]};
                        const Vec3 high = {destroyable->maxCorner[0], destroyable->maxCorner[1], destroyable->maxCorner[2]};
                        createBoundingBoxFromLocalCoordinates(low, high, transform, entry.mesh);
                        const IAIObject* const aiObject = static_cast<IAIObject*>(object->getAIComponent());
                        addOns.insert(std::make_pair(aiObject, entry));
                    }
                    if (!destroyable->initiallyWalkable)
                        continue;
                }
                const bool ladder = objectTemplate->getClassID() == world::CID_LadderTemplate;
                std::list<MeshAndBoundingBox>& output = ladder ? more : meshes;
                {
                    MeshAndBoundingBox entry;
                    entry.name = name;
                    // shipped defect: the owner is copied before it is assigned below.
                    output.push_back(entry);
                }
                MeshAndBoundingBox& entry = output.back();
                entry.object = static_cast<IAIObject*>(object->getAIComponent());
                createMeshFromObject(collision, 3, transform, entry.mesh);
                const LocalBoundingBox* const box = collision->getBoundingBox(3);
                createBoundingBoxFromLocalCoordinates(box->m_min, box->m_max, transform, entry.boundingBox);
            }
            else if (object->hasCollisionLod(0))
            {
                world::ICollisionMesh* const collision = static_cast<world::ICollisionMesh*>(
                    world::object_detail::LayoutAccess::collisionMesh(object));
                std::vector<std::pair<std::string, unsigned int>>* const materials = objectTemplate->getMaterialVector();
                const int count = static_cast<int>(materials->size());
                bool hasWater = false;
                for (int i = 0; i < count; ++i)
                {
                    world::Material* const material = static_cast<world::MaterialManager*>(world::materialManager)
                        ->getMaterialPtr((*materials)[i].second);
                    if (material->getHasWaterPhysics())
                    {
                        hasWater = true;
                        break;
                    }
                }
                if (hasWater)
                {
                    WaterMesh entry; // The retail name stays empty.
                    createMeshFromObject(collision, 0, transform, entry.mesh);
                    water.push_back(entry);
                }
            }
            continue;
        }

        const world::ObjectSpawnerTemplate* const spawner =
            static_cast<const world::ObjectSpawnerTemplate*>(object->getTemplate());
        bool valid[3] = {false, false, false};
        Mesh slotMeshes[3];
        bool rejected = false;
        for (unsigned int slot = 0; slot <= 2; ++slot)
        {
            const std::string templateName = spawner->getObjectTemplate(slot);
            world::IObjectTemplate* const spawnedTemplate = world::objectTemplateManager->getTemplate(templateName);
            if (!spawnedTemplate)
                continue;
            const Vec3 rotation = Vec3::getZero();
            const Vec3 position = Vec3::getZero();
            world::IObject* const spawned = gameLogic->spawnObject(spawnedTemplate, position, rotation, 0);
            if (spawned->queryInterface(world::IID_IPlayerControlObject))
            {
                world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(
                    world::object_detail::LayoutAccess::physicsNode(spawned));
                if (node && node->getIsMobile())
                {
                    game->destroyObject(spawned);
                    rejected = true;
                    break;
                }
            }
            world::IObjectTemplate* const spawnedObjectTemplate = static_cast<world::IObjectTemplate*>(
                spawned->getTemplate()->queryInterface(world::IID_IObjectTemplate));
            if (spawnedObjectTemplate->getClassID() == world::CID_DestroyableObjectTemplate
                && static_cast<world::DestroyableObjectTemplate*>(spawnedObjectTemplate)->usedByAi)
            {
                game->destroyObject(spawned);
                rejected = true;
                break;
            }
            const Mat4& transform = object->getAbsoluteTransformation();
            if (spawned->hasCollisionLod(3))
            {
                world::ICollisionMesh* const collision = static_cast<world::ICollisionMesh*>(
                    world::object_detail::LayoutAccess::collisionMesh(spawned));
                const LocalBoundingBox* const box = collision->getBoundingBox(3);
                createBoundingBoxFromLocalCoordinates(box->m_min, box->m_max, transform, slotMeshes[slot]);
                game->destroyObject(spawned);
                valid[slot] = true;
            }
            else
                game->destroyObject(spawned);
        }
        if (rejected || (!valid[0] && !valid[1] && !valid[2]))
            continue;
        {
            MeshAndBoundingBox entry;
            entry.name = name;
            meshes.push_back(entry);
        }
        MeshAndBoundingBox& entry = meshes.back();
        entry.object = 0;
        Vec3 low = {100000.0f, 100000.0f, 100000.0f};
        Vec3 high = {-100000.0f, -100000.0f, -100000.0f};
        for (unsigned int slot = 0; slot <= 2; ++slot)
        {
            if (!valid[slot])
                continue;
            for (unsigned int i = 0; i < slotMeshes[slot].vertices.size(); ++i)
            {
                const Vec3& p = slotMeshes[slot].vertices[i];
                if (low.x > p.x) low.x = p.x;
                if (low.y > p.y) low.y = p.y;
                if (low.z > p.z) low.z = p.z;
                if (p.x > high.x) high.x = p.x;
                if (p.y > high.y) high.y = p.y;
                if (p.z > high.z) high.z = p.z;
            }
        }
        createBoundingBoxFromWorldCoordinates(low, high, entry.boundingBox);
        createBoundingBoxFromWorldCoordinates(low, high, entry.mesh);
    }
    bounds.first.x -= 3.0f;
    bounds.first.y -= 3.0f;
    bounds.second.x += 3.0f;
    bounds.second.y += 3.0f;
    return true;
}

// bf2 0x8405840 (Hex-Rays): an object in the grid (object manager +0x30) without a physics node,
// or whose node sleeps (+0x11C). Unknown ids are not asleep.
bool BFObjectEnvironment::isAsleep(unsigned int id) const
{
    world::IObject* const object = world::objectManager->getObjectFromGrid(id);
    if (!object)
        return false;
    world::IPhysicsNode* const node =
        static_cast<world::IPhysicsNode*>(world::object_detail::LayoutAccess::physicsNode(object));   // +0x44
    return !node || node->getIsSleeping();
}

// ELF 0x8405740. The target map's owning player determines the team filter.
void BFObjectEnvironment::getTargetObjects(std::vector<TargetData>& out, int team) const
{
    const std::map<unsigned int, world::IObject*>* const objects =
        static_cast<const std::map<unsigned int, world::IObject*>*>(gameLogic->getTargetObjectMap());
    for (std::map<unsigned int, world::IObject*>::const_iterator it = objects->begin(); it != objects->end(); ++it)
    {
        world::IObject* const object = it->second;
        world::IPlayer* const player = world::playerManager->getPlayerFromId(
            static_cast<world::TargetObject*>(object)->getTargetPlayerId());
        if (player && player->getTeam() == team)
        {
            IAIObject* const aiObject = static_cast<IAIObject*>(world::getRootParent(object)->getAIComponent());
            const Vec3& position = object->getAbsolutePosition();
            out.push_back(TargetData(it->first, position, aiObject));
        }
    }
}

// bf2 0x8405890, 10 bytes: `mov eax, 1` and return; the materialised 1 is why the declaration
// returns bool (the mangled name carries no return type).
bool BFObjectEnvironment::loadOvergrowthCollision(const std::string& path) const
{
    return true;
}

bool BFObjectEnvironment::initAiCombatAreas() const
{
    // ELF 0x84058A0. Capture the singleton before resizing the vector.
    CombatAreaManager* const manager = CombatAreaManager::getInstance();
    std::vector<CombatArea*>& areas = const_cast<std::vector<CombatArea*>&>(m_combatAreas);
    areas.resize(0);
    return manager->getAiCombatAreas(areas);
}

bool BFObjectEnvironment::isAiPositionInsideCombatArea(const Vec3& position) const
{
    // ELF 0x8405930.
    return CombatAreaManager::getInstance()->isAiPositionInside(position, m_combatAreas);
}

// bf2 0x8405960 (Hex-Rays; listing): the first AI combat area's layer, read as a byte (movzbl
// of +0x30), or 255 without one.
int BFObjectEnvironment::getAiCombatAreaLayer() const
{
    int layer = 255;
    if (m_combatAreas.size())
        layer = *reinterpret_cast<const unsigned char*>(&m_combatAreas[0]->m_layer);
    return layer;
}

// ELF 0x8405990. Always clear the output, then orient the first area's polygon clockwise.
void BFObjectEnvironment::getAICombatArea(std::list<Vec2>& out) const
{
    out.clear();
    if (m_combatAreas.empty())
        return;
    const CombatArea* const area = m_combatAreas[0];
    if (area->isClockwise())
        for (std::vector<Vec2>::const_iterator it = area->m_points.begin(); it != area->m_points.end(); ++it)
            out.push_back(*it);
    else
        for (std::vector<Vec2>::const_reverse_iterator it = area->m_points.rbegin(); it != area->m_points.rend(); ++it)
            out.push_back(*it);
}

// Updates the AI object environment during the simulation tick.
void BFObjectEnvironment::damageSeverelyDamagedVehicles()
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    if (m_lastSevereDamageTime > now)
        m_lastSevereDamageTime = 0.0;

    if (!(now - m_lastSevereDamageTime > 4.0))
        return;

    m_lastSevereDamageTime = now;

    std::map<unsigned int, world::IObject*>* const pcos = gameLogic->getPcoMap();   // +0x198

    for (std::map<unsigned int, world::IObject*>::iterator it = pcos->begin(); it != pcos->end(); ++it)
    {
        world::IObject* const object = it->second;

        if (object->getTemplate()->getClassID() == world::CID_SoldierTemplate)
            continue;

        world::IPlayerControlObject* pco = 0;
        for (world::IObject* o = object; o; o = o->getParent())
        {
            pco = static_cast<world::IPlayerControlObject*>(
                o->queryInterface(world::IID_IPlayerControlObject));
            if (pco)
                break;
        }

        if (pco->isOccupied())                                                        // +0x18
            continue;

        world::IArmorGLComp* const armor =
            static_cast<world::IArmorGLComp*>(object->getArmorComponent());          // +0x3C
        world::ArmorGLCompTemplate* const armorTemplate = static_cast<world::ArmorGLCompTemplate*>(
            object->getTemplate()->getComponentFromClassID(world::CID_ArmorGLCompTemplate));

        if (!armor || !armorTemplate)
            continue;

        const float maxHitPoints = armor->getMaxHitPoints();                         // +0x84

        if (!armorTemplate->getCanBeDestroyed())                                      // +0x154
            continue;

        if (armor->getHitPoints() / maxHitPoints < 0.5f)                              // +0x3C
        {
            Vec3 position;  position.x = 0.0f;  position.y = 0.0f; position.z = 0.0f;
            Vec3 direction; direction.x = 0.0f; direction.y = 1.0f; direction.z = 0.0f;

            gameLogic->giveDamage(object, maxHitPoints * 0.1f, 0, 0, 0, -1, 0, 0,     // +0xE4
                                  position, direction, -1, false, true, false);
        }
    }
}

// bf2 0x8402130 (listing; Hex-Rays cross-check). The object line test from `from` along
// `to - from`: without spheres the object manager's intersectLine (+0x68, flags 1, 0, 0, 1, mask 0)
// through a RayCastStopPredicate; with spheres intersectLineWithSpheres (+0x70) with a
// SphereExcludeFilter first and the stop predicate second, `sphereRadius` last. `hitBeyondSeeThrough`
// is whether the hit point lies farther from `from` than the nearest see-through surface the stop
// predicate skipped (always false when none was). Returns the intersection's answer.
// bf2 0x8408350 (Hex-Rays). Reserves for the new objects, then appends each object's AI object
// (getIAIObject), skipping objects without one. `objects` is taken by value.
void BFObjectEnvironment::pushAIObjects(std::vector<IAIObject*>& out, std::vector<world::IObject*> objects) const
{
    out.reserve(objects.size() + out.size());
    for (std::vector<world::IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        IAIObject* const aiObject = getIAIObject(*it);
        if (aiObject)
            out.push_back(aiObject);
    }
}

int BFObjectEnvironment::collideLineWithObjects(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool useSpheres, float sphereRadius, bool seeThroughStops, bool& hitBeyondSeeThrough)
{
    Vec3 hitPoint;
    hitPoint.x = 0.0f;
    hitPoint.y = 0.0f;
    hitPoint.z = 0.0f;
    Vec3 hitNormal;
    hitNormal.x = 0.0f;
    hitNormal.y = 0.0f;
    hitNormal.z = 0.0f;
    Vec3 unused;                                   // a third zeroed Vec3 (-0x38), never passed
    unused.x = 0.0f;
    unused.y = 0.0f;
    unused.z = 0.0f;
    (void)unused;
    RayCastStopPredicate stop(ignore, seeThroughStops, from);
    world::IObject* hitObject;
    int hitPart;
    bool result;
    if (useSpheres)
    {
        SphereExcludeFilter exclude(ignore, seeThroughStops);
        Vec3 direction;
        direction.x = to.x - from.x;
        direction.y = to.y - from.y;
        direction.z = to.z - from.z;
        result = world::objectManager->intersectLineWithSpheres(hitObject, hitPoint, hitNormal, hitPart,
                                                                from, direction, &exclude, &stop,
                                                                sphereRadius);              // +0x70
    }
    else
    {
        Vec3 direction;
        direction.x = to.x - from.x;
        direction.y = to.y - from.y;
        direction.z = to.z - from.z;
        result = world::objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, from,
                                                     direction, &stop, true, false, false, true, 0);   // +0x68
    }
    const float dx = hitPoint.x - from.x;
    const float dy = hitPoint.y - from.y;
    const float dz = hitPoint.z - from.z;
    hitBeyondSeeThrough = dx * dx + dy * dy + dz * dz > stop.m_nearestSeeThroughSq;
    return result;
}

// ELF 0x8405C30. Each face contributes undirected edges, stored once in insertion order.
void BFObjectEnvironment::createMeshFromObject(world::ICollisionMesh* collision, int lod,
                                               const Mat4& transform, Mesh& mesh) const
{
    const Vec3* const vertices = static_cast<const Vec3*>(collision->getVertexVector(lod));
    const int vertexCount = static_cast<int>(collision->getVertexCount(lod));
    mesh.vertices.reserve(static_cast<unsigned int>(vertexCount));
    for (int i = 0; i < vertexCount; ++i)
        mesh.vertices.push_back(transformPoint(vertices[i], transform));

    std::set<Edge> uniqueEdges;
    const int faceCount = collision->getFaceCount(lod);
    // Three unsigned 16-bit indices in an eight-byte record; the last word is not read here.
    struct ExportFace { unsigned short indices[3]; unsigned char unread[2]; };
    const ExportFace* const faces = static_cast<const ExportFace*>(collision->getFaceVector(lod));
    for (int i = 0; i < faceCount; ++i)
    {
        int previous = faces[i].indices[2];
        triple<int, int, int> triangle;
        for (int j = 0; j <= 2; ++j)
        {
            const int current = faces[i].indices[j];
            const Edge edge = {previous < current ? previous : current,
                               current < previous ? previous : current,
                               static_cast<int>(mesh.edges.size())};
            const std::pair<std::set<Edge>::iterator, bool> inserted = uniqueEdges.insert(edge);
            if (inserted.second)
                mesh.edges.push_back(std::make_pair(inserted.first->first, inserted.first->second));
            // ELF 0x840A100: insert_in_triple(triangle, 2 - j, index).
            if (j == 0) triangle.third = inserted.first->index;
            else if (j == 1) triangle.second = inserted.first->index;
            else triangle.first = inserted.first->index;
            previous = current;
        }
        mesh.faces.push_back(triangle);
    }
}

// ELF 0x8405F70. Corner ordering uses x/y/z bits 4/2/1, then the shared box topology.
void BFObjectEnvironment::createBoundingBoxFromLocalCoordinates(const Vec3& min, const Vec3& max,
                                                               const Mat4& transform, Mesh& mesh) const
{
    mesh.vertices.reserve(8);
    for (unsigned int i = 0; i < 8; ++i)
    {
        const Vec3 corner = {(i & 4) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 1) ? max.z : min.z};
        mesh.vertices.push_back(transformPoint(corner, transform));
    }
    appendBoxTopology(mesh);
}

// ELF 0x8407300.
void BFObjectEnvironment::createBoundingBoxFromWorldCoordinates(const Vec3& min, const Vec3& max, Mesh& mesh) const
{
    mesh.vertices.reserve(8);
    for (unsigned int i = 0; i < 8; ++i)
    {
        const Vec3 corner = {(i & 4) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 1) ? max.z : min.z};
        mesh.vertices.push_back(corner);
    }
    appendBoxTopology(mesh);
}

// ELF 0x84083F0. Every area's vertices participate, including exclusions.
void BFObjectEnvironment::createBoundingBoxFromCombatAreas(std::pair<Vec2, Vec2>& bounds) const
{
    Vec2 low = {100000.0f, 100000.0f};
    Vec2 high = {-100000.0f, -100000.0f};
    for (unsigned int i = 0; i < m_combatAreas.size(); ++i)
    {
        const std::vector<Vec2>& points = m_combatAreas[i]->m_points;
        for (unsigned int j = 0; j < points.size(); ++j)
        {
            if (low.x > points[j].x) low.x = points[j].x;
            if (low.y > points[j].y) low.y = points[j].y;
            if (points[j].x > high.x) high.x = points[j].x;
            if (points[j].y > high.y) high.y = points[j].y;
        }
    }
    bounds.first = low;
    bounds.second = high;
}

// ELF 0x8408500. Walk each transformed bottom edge in unit XZ steps until an AI area accepts it.
bool BFObjectEnvironment::objectOverlapsCombatAreas(const world::IObject* object,
                                                   std::pair<Vec2, Vec2>& bounds) const
{
    if (!object->hasCollisionLod(3))
        return false;
    world::ICollisionMesh* const collision = static_cast<world::ICollisionMesh*>(
        world::object_detail::LayoutAccess::collisionMesh(const_cast<world::IObject*>(object)));
    const LocalBoundingBox* const box = collision->getBoundingBox(3);
    CombatAreaManager* const manager = CombatAreaManager::getInstance();
    const Mat4& transform = object->getAbsoluteTransformation();
    std::vector<Vec3> points;
    const Vec3 corners[4] = {
        transformPoint(Vec3{box->m_min.x, box->m_min.y, box->m_min.z}, transform),
        transformPoint(Vec3{box->m_max.x, box->m_min.y, box->m_min.z}, transform),
        transformPoint(Vec3{box->m_max.x, box->m_min.y, box->m_max.z}, transform),
        transformPoint(Vec3{box->m_min.x, box->m_min.y, box->m_max.z}, transform)
    };
    for (unsigned int i = 0; i < 4; ++i)
        points.push_back(corners[i]);
    for (unsigned int i = 0; i < 4; ++i)
    {
        const Vec3& a = points[i];
        const Vec3& b = points[(i + 1) & 3];
        Vec3 direction = {b.x - a.x, 0.0f, b.z - a.z};
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        const float lengthSq = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
        if (!(FLT_EPSILON > std::fabs(lengthSq - 1.0f)))
        {
            if (!(FLT_EPSILON > std::fabs(lengthSq)))
            {
                const float scale = 1.0f / std::sqrt(lengthSq);
                direction.x *= scale;
                direction.y *= scale;
                direction.z *= scale;
            }
            else
                direction.x = direction.y = direction.z = 0.0f;
        }
        const int count = static_cast<int>(length) + 1;
        Vec3 position = points[i];
        for (int sample = 0; sample < count; ++sample)
        {
            if (manager->isAiPositionInside(position, m_combatAreas))
            {
                for (unsigned int j = 0; j < 4; ++j)
                {
                    if (bounds.first.x > points[j].x) bounds.first.x = points[j].x;
                    if (bounds.first.y > points[j].z) bounds.first.y = points[j].z;
                    if (points[j].x > bounds.second.x) bounds.second.x = points[j].x;
                    if (points[j].z > bounds.second.y) bounds.second.y = points[j].z;
                }
                return true;
            }
            position.x = direction.x + position.x;
            position.y = direction.y + position.y;
            position.z = direction.z + position.z;
        }
    }
    return false;
}

// ELF 0x8408C00.
bool BFObjectEnvironment::hasOnlyInfantryCollMesh(world::IObject* object) const
{
    world::ICollisionMesh* const collision = static_cast<world::ICollisionMesh*>(
        world::object_detail::LayoutAccess::collisionMesh(object));
    if (!collision || object->hasCollisionLod(1) || !object->hasCollisionLod(2))
        return false;
    return !collision->sameInstance(0, 2, 0, 3);
}

} // namespace ai
} // namespace hfe
} // namespace dice
