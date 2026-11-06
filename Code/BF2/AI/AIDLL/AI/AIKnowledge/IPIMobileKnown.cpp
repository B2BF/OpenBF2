// so and AIDLL_w32ded.dll .

#include "IPIMobileKnown.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EF6C0, __tcf_0 0x2EF720, _GLOBAL__I 0x2EF750).
#include <iostream>

#include <cmath>

#include "Information.h"
#include "../Main/InformationManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

void clearVector(Vec3& vector)
{
    vector.x = 0.0f;
    vector.y = 0.0f;
    vector.z = 0.0f;
}

} // anonymous namespace

IPIMobileKnown::IPIMobileKnown(Information* information)
    : IPIMobile(information)
{
    clearVector(m_speed);
    clearVector(m_rotationalSpeed);
    clearVector(m_unresolvedVector);
    m_hasContact = true;
    m_speed = m_information->getAIObject()->getMobile()->getSpeed();
    m_hasContact = false;
    stealLastValid();
    m_time = information->getTimeStamp();
    // m_sector and m_lastPassedPortal are intentionally untouched.
}

IPIMobileKnown::~IPIMobileKnown()
{
}

void IPIMobileKnown::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

void IPIMobileKnown::setTime(double time)
{
    IAIObjectMobile* const mobile = m_information->getAIObject()->getMobile();
    m_time = time;
    IAIObject* const sector = mobile->getSector();
    if (sector)
        m_sector = sector->getInformation(m_information->getSide());
    else
        m_sector = static_cast<unsigned int>(-1);
    if (!m_hasContact)
        stealLastValid();
}

void IPIMobileKnown::setHasContact()
{
    if (!m_hasContact)
        stealLastValid();
    m_hasContact = true;
}

void IPIMobileKnown::setLostContact()
{
    m_hasContact = false;
    m_speed = m_information->getAIObject()->getMobile()->getSpeed();
    setTime();
}

const Vec3& IPIMobileKnown::getSpeed()
{
    if (m_hasContact)
        return m_information->getAIObject()->getMobile()->getSpeed();
    return m_speed;
}

const Vec3& IPIMobileKnown::getRotationalSpeed()
{
    if (m_hasContact)
        return m_information->getAIObject()->getMobile()->getRotationalSpeed();
    return m_rotationalSpeed;
}

float IPIMobileKnown::getSpeedMagnitude()
{
    const Vec3& speed = getSpeed();
    return std::sqrt(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);
}

float IPIMobileKnown::getSoundSphereRadius() const
{
    return m_information->getAIObject()->getMobile()->getSoundSphereRadius();
}

void IPIMobileKnown::getPositionEstimate(Vec3& position)
{
    if (m_hasContact)
        position = m_information->getPosition();
    else
        getPositionEstimate(
            static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime(),
            position);
}

// ELF 0x2EF4B0.
void IPIMobileKnown::getPositionEstimate(double time, Vec3& position)
{
#if defined(_MSC_VER)
    // [PLATFORM] PE evaluates security first. X uses the retained scale; Y/Z use its float copy.
    const float security = m_information->getSecurity(time);
    const double wideScale = static_cast<double>(security) * (m_time - time);
    const float roundedScale = static_cast<float>(wideScale);
    const float scaledX = static_cast<float>(static_cast<double>(m_speed.x) * wideScale);
    const float scaledY = m_speed.y * roundedScale;
    const float scaledZ = m_speed.z * roundedScale;
    position.x = scaledX;
    position.y = scaledY;
    position.z = scaledZ;
#else
    // [PLATFORM] ELF rounds age before security and retains one x87-wide scale for X/Y/Z.
    const float age = static_cast<float>(m_time - time);
    const float security = m_information->getSecurity(time);
    const long double wideScale =
        static_cast<long double>(security) * static_cast<long double>(age);
    const float scaledX =
        static_cast<float>(static_cast<long double>(m_speed.x) * wideScale);
    const float scaledY =
        static_cast<float>(static_cast<long double>(m_speed.y) * wideScale);
    const float scaledZ =
        static_cast<float>(static_cast<long double>(m_speed.z) * wideScale);
    // ELF materializes its temporary Vec3 in Z, X, Y store order.
    position.z = scaledZ;
    position.x = scaledX;
    position.y = scaledY;
#endif
    const Vec3& knownPosition = m_information->getPosition();
    position.x += knownPosition.x;
    position.y += knownPosition.y;
    position.z += knownPosition.z;
}

unsigned int IPIMobileKnown::getSector() const
{
    return m_sector;
}

int IPIMobileKnown::getLastPassedPortal() const
{
    return m_lastPassedPortal;
}

void IPIMobileKnown::setLastValidInfantryPos(const ValidPos& position)
{
    if (m_hasContact)
        setLastValidInfantryPosInternal(position);
}

void IPIMobileKnown::setLastValidVehiclePos(const ValidPos& position)
{
    if (m_hasContact)
        setLastValidVehiclePosInternal(position);
}

void IPIMobileKnown::stealLastValid()
{
    IAIObject* const object = m_information->getAIObject();
    Information* const realInformation =
        informationManager->getInformation(object->getInformation(object->getSide()));
    IPIMobile* const mobile = realInformation->getMobile();
    setLastValidInfantryPosInternal(mobile->m_lastValidInfantryPos);
    setLastValidVehiclePosInternal(mobile->m_lastValidVehiclePosA,
                                   mobile->m_lastValidVehiclePosB,
                                   mobile->m_useFirstVehicleValidPos);
}

} // namespace ai
} // namespace hfe
} // namespace dice
