// [INFERRED] Source placement.

#include "AIObjectPhysical.h"

#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "AITemplatePhysical.h"
#include "BFGame.h"                                          // IAIGame::getIsPrecaching (+0x68)

#include "../IAIEventCourier.h"                              // event_retrievePathfindingMapId & co.
#include "../SingletonServer.h"                              // BFSingletonServer
#include "../../AIDLL/AI/AIKnowledge/InformationFactory.h"        // IInformationFactory
#include "../../AIDLL/AI/Main/CommonSingletonServer.h"

#include "../../../Core/Ids.h"
#include "../../../Game/Objects/DestroyableObjectTemplate.h" // usedByAi / initiallyWalkable / corners
#include "../../../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../../../Game/Objects/GameLogicComp/ArmorGLCompTemplate.h"   // getTimeToStayAsWreck
#include "../../../Game/Objects/IObject.h"
#include "../../../Game/Objects/IObjectTemplate.h"
#include "../../../Game/Objects/IPlayer.h"
#include "../../../Game/Objects/ObjectLayoutAccess.h"
#include "../../../Math/LocalBoundingBox.h"
#include "../../../Math/BoundingBox.h"
#include "../../../Physics/ICollisionMesh.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/IResponsePhysics.h"
#include "../../../Physics/PhysicsUtil.h"                    // hfe::resetPhysics
#include "../../../Physics/SoldierResponse.h"                // m_byteFA (+0xFA), a friend
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAIGame* game() { return static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame); }

IInformationFactory* informationFactory()
{
    return static_cast<IInformationFactory*>(BFSingletonServer::m_iInformationFactory);
}

world::ICollisionMesh* collisionMesh(world::IObject* object)
{
    return static_cast<world::ICollisionMesh*>(world::object_detail::LayoutAccess::collisionMesh(object));
}

world::IResponsePhysics* responsePhysics(world::IObject* object)
{
    return static_cast<world::IResponsePhysics*>(
        world::object_detail::LayoutAccess::responsePhysics(object));
}

} // anonymous namespace

// . +0x0C is left unwritten.
// ELF 0x8419940 / 0x8419980.
AIObjectPhysical::AIObjectPhysical(IAIObject* object, AITemplatePhysical* aiTemplate)
    : IAIObjectPhysical(object), m_template(aiTemplate), m_physicsNode(0), m_mapPosId(-1)
{
}

// A pathfinding-relevant object gives its map entry back.
// ELF 0x8419AD0 / 0x8419B30 / 0x8419B90.
AIObjectPhysical::~AIObjectPhysical()
{
    if (m_template->m_affectsPathfinding)
        BFSingletonServer::m_iaiEventCourier->event_pathfindingMapIdRestoreOriginalMaterials(
            m_mapPosId, m_object);                                                  // +0xC4
}

// ELF 0x8419D00.
int AIObjectPhysical::getId() const
{
    return AIOFPhysical;
}

// - the factory's three-argument request under this plug-in's own id.
// ELF 0x8419BF0.
void AIObjectPhysical::createInformationPlugIn(unsigned int handle, bool real)
{
    informationFactory()->requestInformationPlugin(handle, getId(), real);         // +0x20
}

// The template's +0x04 is copied before the template initialises (see the
// header), the physics node is fetched, the template learns from its first instance, and a
// `usedByAi` destroyable object - outside precaching - asks the AI for a pathfinding map id from its
// template's walkability and corners (DestroyableObjectTemplate +0x189, +0x18C, +0x198) and
// announces itself as a bridge.
// The template is cast to DestroyableObjectTemplate unchecked - only such templates can set
// m_affectsPathfinding (AITemplatePhysical::initFromObject).
// ELF 0x84199C0.
void AIObjectPhysical::init()
{
    m_maxHealth = m_template->m_maxHealth;
    m_physicsNode = static_cast<world::IPhysicsNode*>(
        world::object_detail::LayoutAccess::physicsNode(m_object->getObject()));   // +0x44
    if (!m_template->m_initialized)
        m_template->initFromObject(m_object);

    if (m_template->m_affectsPathfinding && !game()->getIsPrecaching())             // +0x68
    {
        const world::DestroyableObjectTemplate* const objectTemplate =
            static_cast<const world::DestroyableObjectTemplate*>(m_object->getObject()->getTemplate());
        const Mat4& transformation = m_object->getTransformation();                 // +0xDC
        BFSingletonServer::m_iaiEventCourier->event_retrievePathfindingMapId(
            m_mapPosId, objectTemplate->initiallyWalkable,
            *reinterpret_cast<const Vec3*>(objectTemplate->minCorner),
            *reinterpret_cast<const Vec3*>(objectTemplate->maxCorner), transformation);   // +0xB8
        BFSingletonServer::m_iaiEventCourier->event_bridgeCreated(m_object);        // +0xD0
    }
}

// empty.
// ELF 0x841A740.
void AIObjectPhysical::update()
{
}

// Returns 1,000,000 when there is no armour component.
// ELF 0x841A170.
float AIObjectPhysical::getHealth()
{
    world::IArmorGLComp* const armor =
        static_cast<world::IArmorGLComp*>(m_object->getObject()->getArmorComponent());   // +0x3C
    if (armor)
        return armor->getHitPoints();                                               // +0x3C
    return 1000000.0f;
}

// / - template reads.
// ELF 0x8419C40.
float AIObjectPhysical::getMaxHealth() const
{
    return m_template->m_maxHealth;
}

// ELF 0x8419C50.
float AIObjectPhysical::getMass() const
{
    return m_template->m_mass;
}

// ELF 0x8419CC0.
float AIObjectPhysical::getVolume() const
{
    return m_template->m_volume;
}

// ELF 0x8419CE0.
float AIObjectPhysical::getBestArmour() const
{
    return m_template->m_bestArmour;
}

// ELF 0x8419CF0.
int AIObjectPhysical::getStrType() const
{
    return m_template->m_strType;
}

// a constant, whatever the direction.
// ELF 0x8419CD0.
float AIObjectPhysical::getArmour(const Vec3& /*direction*/) const
{
    return 1.0f;
}

// an object without collision LOD 1 calls abort - retail's own, not a
// frontier. Otherwise the collision mesh's LOD-1 box.
// ELF 0x841A1A0.
const LocalBoundingBox* AIObjectPhysical::getLocalBoundingBox() const
{
    if (!m_object->getObject()->hasCollisionLod(1))                                 // +0xCC
        std::abort();
    return collisionMesh(m_object->getObject())->getBoundingBox(1);                 // +0x58
}

// ELF 0x841A280. Allocation precedes the virtual queries; initialization follows them.
BoundingBox* AIObjectPhysical::getCurrentBoundingBox()
{
    BoundingBox* const box = new BoundingBox;
    const LocalBoundingBox* const local = getLocalBoundingBox();
    const Mat4& transformation = m_object->getObject()->getAbsoluteTransformation();
    for (int i = 0; i < 8; ++i)
        box->m_corners[i] = Vec3{ 0.0f, 0.0f, 0.0f };
    box->initWithLocalBoundingBox(*local);
    box->transform(transformation);
    return checkPointer(box);
}

// ELF 0x841A400.
BoundingBox* AIObjectPhysical::getBoundingBox(const Mat4& transformation)
{
    BoundingBox* const box = new BoundingBox;
    const LocalBoundingBox* const local = getLocalBoundingBox();
    for (int i = 0; i < 8; ++i)
        box->m_corners[i] = Vec3{ 0.0f, 0.0f, 0.0f };
    box->initWithLocalBoundingBox(*local);
    box->transform(transformation);
    return checkPointer(box);
}

// The collision mesh's LOD-1 radius, 0 without that LOD.
// ELF 0x8419C60.
float AIObjectPhysical::getRadius() const
{
    if (!m_object->getObject()->hasCollisionLod(1))                                 // +0xCC
        return 0.0f;
    return collisionMesh(m_object->getObject())->getBoundingRadius(1);              // +0x54
}

// The box centre, recomputed first when stale - through a const pointer, as
// retail does (the derived data is cached state).
// ELF 0x841A200.
Vec3 AIObjectPhysical::getSphereLocalOffset() const
{
    LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(getLocalBoundingBox());   // +0x30
    if (box->m_derivedDataDirty)
        box->calcRadius();
    return box->m_centre;
}

// the radius about the centre.
// ELF 0x841A250.
float AIObjectPhysical::getSmallestRadius() const
{
    LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(getLocalBoundingBox());
    if (box->m_derivedDataDirty)
        box->calcRadius();
    return box->m_radiusFromCentre;
}

// ELF 0x8419E50. Appends circles along the longer horizontal side; returns their spacing.
float AIObjectPhysical::getSubSpheres2d(std::vector<Vec2>& out) const
{
    const LocalBoundingBox* const minimum = getLocalBoundingBox();
    const LocalBoundingBox* const maximum = getLocalBoundingBox();
    const float minX = minimum->m_min.x;
    const float minZ = minimum->m_min.z;
    const float width = maximum->m_max.x - minX;
    const float depth = maximum->m_max.z - minZ;
    if (depth > width)
    {
        if (width != 0.0f)
        {
            const float count = depth / width;
            if (count > 1.0f)
            {
                Vec2 position = { minX + width * 0.5f, minZ };
                const float step = depth / (count + 1.0f);
                for (int i = 0; static_cast<float>(i) < count; ++i)
                {
                    position.y += step;
                    out.push_back(position);
                }
                return step;
            }
        }
    }
    else if (width > depth && depth != 0.0f)
    {
        const float count = width / depth;
        if (count > 1.0f)
        {
            Vec2 position = { minX, minZ + depth * 0.5f };
            const float step = width / (count + 1.0f);
            for (int i = 0; static_cast<float>(i) < count; ++i)
            {
                position.x += step;
                out.push_back(position);
            }
            return step;
        }
    }
    // shipped defect: both coordinates use x, with two independent virtual queries.
    const float x = getSphereLocalOffset().x;
    const float z = getSphereLocalOffset().x;
    out.push_back(Vec2{ x, z });
    return getSmallestRadius();
}

// ELF 0x841A090.
bool AIObjectPhysical::isTouchingTerrain(Vec3& out) const
{
    return responsePhysics(m_object->getObject())->getIsIntersectingTerrain(out);   // +0x7C
}

// bf2 0x841A0C0 (ELF listing). The ISoldier query comes first; then false while the physics node
// sleeps (+0x11C); a soldier answers the negation of its response physics' ground-contact byte
// (SoldierResponsePhysics +0xFA, the object asked for again); anything else asks the node's
// getIsInAir (+0x1B0).
bool AIObjectPhysical::isInAir() const
{
    void* const soldier = m_object->getObject()->queryInterface(world::IID_ISoldier);      // +0x0C
    if (m_physicsNode->getIsSleeping())                                             // +0x11C
        return false;
    if (soldier)
    {
        const world::SoldierResponsePhysics* const response =
            static_cast<world::SoldierResponsePhysics*>(
                world::object_detail::LayoutAccess::responsePhysics(m_object->getObject()));
        return !response->m_byteFA;
    }
    return m_physicsNode->getIsInAir();                                             // +0x1B0
}

// strictly above zero (`setnbe` after fucomip - unordered is false).
// ELF 0x841A140.
bool AIObjectPhysical::isInWater() const
{
    return m_physicsNode->getUnderWater() > 0.0f;                                   // +0x10C
}

// The engine object is asked for and discarded; the answer is always 0.
// ELF 0x841A560.
int AIObjectPhysical::getMaterial() const
{
    m_object->getObject();
    return 0;
}

// The occupying player's man-down clock, when it has a
// control object, an occupant, and that occupant is down. The occupant is asked for twice.
// ELF 0x841A580.
float AIObjectPhysical::getManDownTimeLeft() const
{
    world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
        m_object->getObject()->queryInterface(world::IID_IPlayerControlObject));
    if (!pco || !pco->getOccupingPlayer())                                          // +0x10
        return 0.0f;
    world::IPlayer* const player = pco->getOccupingPlayer();
    if (!player->getIsManDown())                                                    // +0x100
        return 0.0f;
    return player->getManDownTimeLeft();                                            // +0xF8
}

// bf2 0x841A5F0 (ELF listing). The object's template component of class ArmorGLCompTemplate
// (IObjectTemplate +0x60, getComponentFromClassID) answers its +0x110 slot, which the ELF primary
// vtable (_ZTVN4dice3hfe5world19ArmorGLCompTemplateE) holds as getTimeToStayAsWreck; 1.0 without
// the component.
float AIObjectPhysical::getMaxManDownTime() const
{
    world::ArmorGLCompTemplate* const armorTemplate = static_cast<world::ArmorGLCompTemplate*>(
        m_object->getObject()->getTemplate()->getComponentFromClassID(world::CID_ArmorGLCompTemplate));
    if (!armorTemplate)
        return 1.0f;
    return armorTemplate->getTimeToStayAsWreck();                                   // +0x110
}

// status 1 on the armour component.
// ELF 0x841A640.
bool AIObjectPhysical::getManDown() const
{
    world::IArmorGLComp* const armor =
        static_cast<world::IArmorGLComp*>(m_object->getObject()->getArmorComponent());
    if (armor)
        return armor->getStatus() == 1;                                             // +0x5C
    return false;
}

// ELF 0x841A680.
bool AIObjectPhysical::doesAffectPathfinding() const
{
    return m_template->m_affectsPathfinding;
}

// ELF 0x841A730.
int AIObjectPhysical::getMapPosId() const
{
    return m_mapPosId;
}

// Only the owning side may switch; the
// physics node and the response physics of the node's object are switched together, and an enable
// resets the object's physics (hfe::resetPhysics, player control objects included). Both answer
// true for the owning side whether or not anything changed.
// ELF 0x8419D90.
bool AIObjectPhysical::enablePhysics(int side)
{
    if (side != m_object->getSide())                                                // +0x12C
        return false;
    if (!hasPhysicsEnabled())                                                       // +0x7C
    {
        m_physicsNode->setEnabled(true);                                            // +0x18
        responsePhysics(m_physicsNode->getObject())->setEnabled(true);              // +0x2C, +0x18
        resetPhysics(m_object->getObject(), true);
    }
    return true;
}

// ELF 0x8419D10.
bool AIObjectPhysical::disablePhysics(int side)
{
    if (side != m_object->getSide())
        return false;
    if (hasPhysicsEnabled())
    {
        m_physicsNode->setEnabled(false);
        responsePhysics(m_physicsNode->getObject())->setEnabled(false);
    }
    return true;
}

// ELF 0x8419E30.
bool AIObjectPhysical::hasPhysicsEnabled()
{
    return m_physicsNode->getEnabled();                                             // +0x1C
}

} // namespace ai
} // namespace hfe
} // namespace dice
