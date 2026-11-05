// so and
// AIDLL_w32ded.dll . Retail omissions and the material fall-through are kept.

#include "IPIMobile.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EEAF0, __tcf_0 0x2EEB50, _GLOBAL__I 0x2EEB80).
#include <iostream>

#include "Information.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

void clearValidPos(IPIMobile::ValidPos& position)
{
    position.polygon = static_cast<unsigned int>(-1);
    position.island = static_cast<unsigned int>(-1);
    position.position.x = 0.0f;
    position.position.y = 0.0f;
    position.position.z = 0.0f;
}

} // anonymous namespace

IPIMobile::IPIMobile(Information* information)
{
    m_information = information;
    clearValidPos(m_lastValidInfantryPos);
    clearValidPos(m_lastValidVehiclePosA);
    clearValidPos(m_lastValidVehiclePosB);
    // m_useFirstVehicleValidPos is intentionally untouched.
}

IPIMobile::~IPIMobile()
{
}

int IPIMobile::getType()
{
    return 2;
}

void IPIMobile::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

int IPIMobile::getVehicleNumber() const
{
    return m_information->getAIObject()->getMobile()->getVehicleNumber();
}

float IPIMobile::getMaxSpeed()
{
    return m_information->getAIObject()->getMobile()->getMaxSpeed();
}

float IPIMobile::getTurnRadius()
{
    return m_information->getAIObject()->getMobile()->getTurnRadius();
}

float IPIMobile::getDrag()
{
    return m_information->getAIObject()->getMobile()->getDrag();
}

float IPIMobile::getCoverSearchRadius()
{
    return m_information->getAIObject()->getMobile()->getCoverSearchRadius();
}

float IPIMobile::getSoundSphereRadiusMax() const
{
    return m_information->getAIObject()->getMobile()->getSoundSphereRadiusMax();
}

float IPIMobile::getSoundSphereRadiusMin() const
{
    return m_information->getAIObject()->getMobile()->getSoundSphereRadiusMin();
}

float IPIMobile::getHearingProbabilityMax() const
{
    return m_information->getAIObject()->getMobile()->getHearingProbabilityMax();
}

float IPIMobile::getHearingProbabilityMin() const
{
    return m_information->getAIObject()->getMobile()->getHearingProbabilityMin();
}

bool IPIMobile::isTurnable() const
{
    return m_information->getAIObject()->getMobile()->isTurnable();
}

float IPIMobile::getAvoidCollisionLookAhead() const
{
    return m_information->getAIObject()->getMobile()->getAvoidCollisionLookAhead();
}

float IPIMobile::getSprintStamina() const
{
    return m_information->getAIObject()->getMobile()->getSprintStamina();
}

const IPIMobile::ValidPos& IPIMobile::getLastValidPosition() const
{
    return getLastValidPosition(getVehicleNumber());
}

const IPIMobile::ValidPos& IPIMobile::getLastValidPosition(int vehicleNumber) const
{
    if (IAIPathfinding::instance->getTraceMapIndex() == vehicleNumber)
        return m_lastValidInfantryPos;
    if (m_useFirstVehicleValidPos)
        return m_lastValidVehiclePosA;
    return m_lastValidVehiclePosB;
}

void IPIMobile::setLastValidInfantryPosInternal(const ValidPos& position)
{
    m_lastValidInfantryPos = position;
}

void IPIMobile::setLastValidVehiclePosInternal(const ValidPos& position)
{
    unsigned char material; // Retail ignores getMaterial's result and may inspect this uninitialized.
    IAIPathfinding::instance->getMaterial(IAIPathfinding::instance->getTraceMapIndex() == 0,
                                          position.island, position.polygon, material);
    switch (material)
    {
    case 3:
        m_lastValidVehiclePosB = position;
        // Retail deliberately falls through: material 3 updates both records and selects A.
    default:
        m_lastValidVehiclePosA = position;
        m_useFirstVehicleValidPos = true;
        break;
    case 4:
        m_lastValidVehiclePosB = position;
        m_useFirstVehicleValidPos = false;
        break;
    }
}

void IPIMobile::setLastValidVehiclePosInternal(const ValidPos& first, const ValidPos& second,
                                                bool useFirst)
{
    m_lastValidVehiclePosB = second;
    m_lastValidVehiclePosA = first;
    m_useFirstVehicleValidPos = useFirst;
}

void IPIMobile::setLastValidPositions(const ValidPos& infantry, const ValidPos& firstVehicle,
                                      const ValidPos& secondVehicle)
{
    m_lastValidInfantryPos = infantry;
    m_lastValidVehiclePosA = firstVehicle;
    m_lastValidVehiclePosB = secondVehicle;
    // The selector is intentionally preserved.
}

} // namespace ai
} // namespace hfe
} // namespace dice
