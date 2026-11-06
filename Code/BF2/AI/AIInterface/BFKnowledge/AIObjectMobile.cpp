#include "AIObjectMobile.h"

#include <cmath>

#include "AITemplateMobile.h"

#include "../SingletonServer.h"                              // BFSingletonServer::m_iInformationFactory
#include "../../AIDLL/AI/AIKnowledge/BFInformationCourier.h"      // IAIInformationCourier
#include "../../AIDLL/AI/AIKnowledge/InformationFactory.h"        // IInformationFactory
#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../AIDLL/AI/Main/CommonSingletonServer.h"

#include "../../../Core/Ids.h"
#include "../../../Core/Debug.h"
#include "../../../Scene/Object/WorldObjTemplPortal/PortalEvent.h"
#include "../../../Game/Objects/IObject.h"
#include "../../../Game/Objects/ISoldier.h"
#include "../../../Game/Objects/ObjectLayoutAccess.h"        // IObject +0x44, the physics node
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAIInformationCourier* informationCourier()
{
    return static_cast<IAIInformationCourier*>(CommonSingletonServer::m_iaiInformationCourier);
}

} // anonymous namespace

// The three unknown dwords and the template, then the owner's engine object
// (asked of the IAIObject argument through getObject, +0x20), and only then the callback - so the
// callback is stored in the body, after +0x20. +0x18 and +0x24 are left unwritten.
AIObjectMobile::AIObjectMobile(IAIObject* object, AITemplateMobile* aiTemplate)
    : IAIObjectMobile(object), m_unknown08(0), m_unknown0C(0), m_unknown10(0),
      m_template(aiTemplate), m_worldObject(object->getObject())
{
    m_portalCallback = checkPointer(new PortalEventCallback(this));
}

// A portal-passing object is unsubscribed from both event types with the
// callback first (portalManager +0x34), then the callback is freed - a plain delete, the callback
// interface having no virtual destructor.
AIObjectMobile::~AIObjectMobile()
{
    if (m_worldObject->queryComponent(world::IID_IPortalPassing, world::IID_IPortalPassing))   // +0x5C
    {
        world::IPortalManager* const portals = static_cast<world::IPortalManager*>(world::portalManager);
        portals->unSubscribePortalPassingObjectEvents(static_cast<world::PortalEventType>(0),
                                                      m_worldObject, m_portalCallback);   // +0x34
        portals->unSubscribePortalPassingObjectEvents(static_cast<world::PortalEventType>(1),
                                                      m_worldObject, m_portalCallback);
    }
    delete m_portalCallback;
}

int AIObjectMobile::getId() const
{
    return AIOFMobile;
}

void AIObjectMobile::createInformationPlugIn(unsigned int handle, bool real)
{
    static_cast<IInformationFactory*>(BFSingletonServer::m_iInformationFactory)
        ->requestInformationPlugin(handle, getId(), real);                         // +0x20
}

// the physics node, then the courier updates the owner's navigation polygon.
void AIObjectMobile::init()
{
    m_physicsNode = static_cast<world::IPhysicsNode*>(
        world::object_detail::LayoutAccess::physicsNode(m_worldObject));           // +0x44
    informationCourier()->updateCurrentPolygon(m_object);                           // +0x38
}

// empty.
void AIObjectMobile::update()
{
}

// the physics node's linear velocity (+0x16C) and rotational speed (+0x64).
const Vec3& AIObjectMobile::getSpeed()
{
    return m_physicsNode->getLinearIntegrationVelocity();
}

const Vec3& AIObjectMobile::getRotationalSpeed()
{
    return m_physicsNode->getRotationalSpeed();
}

// : |getSpeed|, through the slot. In double (the PE's 53-bit x87).
float AIObjectMobile::getSpeedMagnitude()
{
    const Vec3& speed = getSpeed();                                                 // +0x18
    return static_cast<float>(std::sqrt(static_cast<double>(speed.x) * speed.x +
                                        static_cast<double>(speed.y) * speed.y +
                                        static_cast<double>(speed.z) * speed.z));
}

// template reads.
int AIObjectMobile::getVehicleNumber() const
{
    return m_template->m_vehicleNumber;
}

float AIObjectMobile::getMaxSpeed() const
{
    return m_template->m_maxSpeed;
}

float AIObjectMobile::getTurnRadius() const
{
    return m_template->m_turnRadius;
}

// The template's drag is never initialised by its default constructor and no console property
// writes it (AITemplateMobile.h) - this reads whatever the allocation held. Reproduced.
float AIObjectMobile::getDrag() const
{
    return m_template->m_drag;
}

float AIObjectMobile::getCoverSearchRadius() const
{
    return m_template->m_coverSearchRadius;
}

float AIObjectMobile::getLodHeight() const
{
    return m_template->m_lodHeight;
}

float AIObjectMobile::getAvoidCollisionLookAhead() const
{
    return m_template->m_avoidCollisionLookAhead;
}

bool AIObjectMobile::isTurnable() const
{
    return m_template->m_isTurnable;
}

// The "max" reads are the setters' second arguments.
float AIObjectMobile::getSoundSphereRadiusMax() const
{
    return m_template->m_soundSphereRadius2;
}

float AIObjectMobile::getSoundSphereRadiusMin() const
{
    return m_template->m_soundSphereRadius1;
}

float AIObjectMobile::getHearingProbabilityMax() const
{
    return m_template->m_hearingProbability2;
}

float AIObjectMobile::getHearingProbabilityMin() const
{
    return m_template->m_hearingProbability1;
}

// Interpolates the sound radius from speed and clamps at maximum speed.
// The unordered comparison follows the interpolation path; arithmetic uses double precision.
float AIObjectMobile::getSoundSphereRadius() const
{
    const Vec3& velocity = m_physicsNode->getPositionalSpeed();                     // +0x60
    const double speed = std::sqrt((static_cast<double>(velocity.x) * velocity.x +
                                    static_cast<double>(velocity.y) * velocity.y) +
                                   static_cast<double>(velocity.z) * velocity.z);
    const double maxSpeed = m_template->m_maxSpeed;
    if (!(speed >= maxSpeed))
    {
        const double ratio = speed / maxSpeed;
        return static_cast<float>(ratio * m_template->m_soundSphereRadius2 +
                                  (1.0 - ratio) * m_template->m_soundSphereRadius1);
    }
    return m_template->m_soundSphereRadius2;
}

// The sector the object's portal-passing component reports, as that sector
// object's AI component (IObject +0x34); null without either.
IAIObject* AIObjectMobile::getSector() const
{
    world::IPortalPassing* const passing = static_cast<world::IPortalPassing*>(
        m_worldObject->queryComponent(world::IID_IPortalPassing, world::IID_IPortalPassing));
    if (passing)
    {
        world::IObject* const sector = passing->getSector();                        // +0x10
        if (sector)
            return static_cast<IAIObject*>(sector->getAIComponent());
    }
    return 0;
}

int AIObjectMobile::getLastPassedPortal() const
{
    return m_lastPassedPortal;
}

// - the same courier call as init.
void AIObjectMobile::positionChanged()
{
    informationCourier()->updateCurrentPolygon(m_object);                           // +0x38
}

// A soldier's sprint stamina, through its control object (asked of the ISoldier
// face, +0x0C); 0 otherwise.
float AIObjectMobile::getSprintStamina() const
{
    IBase* const soldier = static_cast<IBase*>(
        m_object->getObject()->queryInterface(world::IID_ISoldier));
    if (soldier)
    {
        world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
            soldier->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            return pco->getSprintStamina();                                         // +0x100
    }
    return 0.0f;
}

// ELF 0x8417450. Preserve the repeated type query before the leave-sector branch.
void AIObjectMobile::PortalEventCallback::handleEvent(world::PortalEvent* event)
{
    if (event->getType() == world::PORTAL_EVENT_ENTER_SECTOR)
        m_owner->m_lastPassedPortal = static_cast<int>(event->getPortalId());
    else if (event->getType() == world::PORTAL_EVENT_LEAVE_SECTOR)
        m_owner->m_lastPassedPortal = -1;
    else
        Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/AIObjectMobile.cpp", 57)
            << "Unwanted event sent to callback";
}

} // namespace ai
} // namespace hfe
} // namespace dice
