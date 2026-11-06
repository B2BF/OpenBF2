// so and AIDLL_w32ded.dll .

#include "IPIMobileReal.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EFB80, __tcf_0 0x2EFBE0, _GLOBAL__I 0x2EFC10).
#include <iostream>

#include <cmath>

#include "Information.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"

namespace dice {
namespace hfe {
namespace ai {

IPIMobileReal::IPIMobileReal(Information* information)
    : IPIMobile(information)
{
}

IPIMobileReal::~IPIMobileReal()
{
}

void IPIMobileReal::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

void IPIMobileReal::setTime(double /*time*/)
{
}

void IPIMobileReal::setHasContact()
{
}

void IPIMobileReal::setLostContact()
{
}

const Vec3& IPIMobileReal::getSpeed()
{
    return m_information->getAIObject()->getMobile()->getSpeed();
}

const Vec3& IPIMobileReal::getRotationalSpeed()
{
    return m_information->getAIObject()->getMobile()->getRotationalSpeed();
}

float IPIMobileReal::getSpeedMagnitude()
{
    const Vec3& speed = getSpeed();
    return std::sqrt(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);
}

float IPIMobileReal::getSoundSphereRadius() const
{
    return m_information->getAIObject()->getMobile()->getSoundSphereRadius();
}

void IPIMobileReal::getPositionEstimate(Vec3& position)
{
    position = m_information->getPosition();
}

// ELF 0x2EF900. The information's position moved by the speed over (time - now).
void IPIMobileReal::getPositionEstimate(double time, Vec3& position)
{
    position = m_information->getPosition();
#if defined(_MSC_VER)
    // [PLATFORM] PE captures the timer receiver, calls speed, then calls that retained timer.
    const IBFTimer* const timer =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const Vec3& speed = getSpeed();
    const double difference = time - timer->getGameTime();
    const double scaledX = static_cast<double>(speed.x) * difference;
    const float scaledY = static_cast<float>(static_cast<double>(speed.y) * difference);
    const float scaledZ = static_cast<float>(static_cast<double>(speed.z) * difference);
    position.x = static_cast<float>(scaledX + static_cast<double>(position.x));
    position.y = static_cast<float>(static_cast<double>(scaledY) + position.y);
    position.z = static_cast<float>(static_cast<double>(scaledZ) + position.z);
#else
    // [PLATFORM] ELF calls the timer first and rounds the delta before calling speed.
    const float difference = static_cast<float>(
        time - static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
    const Vec3& speed = getSpeed();
    const long double scaledX = static_cast<long double>(speed.x) * difference;
    const float scaledY =
        static_cast<float>(static_cast<long double>(speed.y) * difference);
    const float scaledZ =
        static_cast<float>(static_cast<long double>(speed.z) * difference);
    position.x = static_cast<float>(scaledX + static_cast<long double>(position.x));
    position.y = static_cast<float>(static_cast<long double>(scaledY) + position.y);
    position.z = static_cast<float>(static_cast<long double>(scaledZ) + position.z);
#endif
}

unsigned int IPIMobileReal::getSector() const
{
    IAIObject* const sector = m_information->getAIObject()->getMobile()->getSector();
    if (!sector)
        return static_cast<unsigned int>(-1);
    return sector->getInformation(m_information->getSide());
}

int IPIMobileReal::getLastPassedPortal() const
{
    return m_information->getAIObject()->getMobile()->getLastPassedPortal();
}

void IPIMobileReal::setLastValidInfantryPos(const ValidPos& position)
{
    setLastValidInfantryPosInternal(position);
}

void IPIMobileReal::setLastValidVehiclePos(const ValidPos& position)
{
    setLastValidVehiclePosInternal(position);
}

} // namespace ai
} // namespace hfe
} // namespace dice
