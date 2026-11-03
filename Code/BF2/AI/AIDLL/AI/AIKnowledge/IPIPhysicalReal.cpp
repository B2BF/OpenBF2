
#include "IPIPhysicalReal.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2F0D60, __tcf_0 0x2F0DC0, _GLOBAL__I 0x2F0DF0).
#include <iostream>

#include "Information.h"
#include "../AIBotBehaviour/CustomCurve.h"                          // SCurve::calculate (static)
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"                // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"        // IAIObject, IAIObjectPhysical

namespace dice {
namespace hfe {
namespace ai {

// IPIPhysical::getType - : 0 - the physical plug-in's type id (Information::
// removePlugIn case 0). [CHOSEN, placement only]: the ELF puts it between the IPIPhysicalDummy and
// IPIPhysicalKnown TUs, in neither's code, and this tree has no IPIPhysical.cpp; it is the class's
// key function, so IPIPhysical's vtable is emitted here with it. (The PE folds it with every other
// `return 0` - its IPIPhysicalReal slot 1 is a shared constant-return body.)
int IPIPhysical::getType()
{
    return 0;
}

// the Information, then the vptr - m_mod is left as allocated.
IPIPhysicalReal::IPIPhysicalReal(Information* information)
{
    m_information = information;
}

// nothing of its own.
IPIPhysicalReal::~IPIPhysicalReal()
{
}

// setTime(double) with the game time.
void IPIPhysicalReal::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

// Ignores time and sets the modifier to 0.5 + 0.5 * SCurve(absolute health) when maximum
// health is positive, otherwise uses zero health. [PLATFORM] PE treats a NaN maximum as
// usable health; ELF selects zero. This implementation preserves the PE comparison.
void IPIPhysicalReal::setTime(double /*time*/)
{
    IAIObjectPhysical* const physical = m_information->getAIObject()->getPhysical();   // +0x94
    float health;
    if (physical->getMaxHealth() <= 0.0f)                                             // +0x1C
        health = 0.0f;
    else
        health = physical->getHealth();                                               // +0x18
    m_mod = 0.5f;
    m_mod = static_cast<float>(static_cast<double>(SCurve::calculate(health)) * 0.5 + m_mod);
}

// health over maximum health - the health is read (and rounded) first.
float IPIPhysicalReal::getHealth() const
{
    IAIObjectPhysical* const physical = m_information->getAIObject()->getPhysical();
    const float health = physical->getHealth();                                       // +0x18
    return health / physical->getMaxHealth();                                        // +0x1C
}

// a real record's health is the engine's; nothing to store.
void IPIPhysicalReal::setHealth(float /*health*/)
{
}

float IPIPhysicalReal::getMaxHealth()
{
    return m_information->getAIObject()->getPhysical()->getMaxHealth();               // +0x1C
}

// refreshed (setTime, through the vtable), then read.
float IPIPhysicalReal::getMod()
{
    setTime();
    return m_mod;
}

BoundingBox* IPIPhysicalReal::getBoundingBox()
{
    return m_information->getAIObject()->getPhysical()->getCurrentBoundingBox();      // +0x34
}

const LocalBoundingBox* IPIPhysicalReal::getLocalBoundingBox()
{
    return m_information->getAIObject()->getPhysical()->getLocalBoundingBox();        // +0x30
}

float IPIPhysicalReal::getRadius()
{
    return m_information->getAIObject()->getPhysical()->getRadius();                  // +0x3C
}

// by value through a hidden result pointer in both images.
Vec3 IPIPhysicalReal::getSphereLocalOffset() const
{
    return m_information->getAIObject()->getPhysical()->getSphereLocalOffset();       // +0x40
}

float IPIPhysicalReal::getSmallestRadius() const
{
    return m_information->getAIObject()->getPhysical()->getSmallestRadius();          // +0x44
}

// ELF 0x2F0CA0: forwards, returning the engine plug-in's answer.
float IPIPhysicalReal::getSubSpheres2d(std::vector<Vec2>& out) const
{
    return m_information->getAIObject()->getPhysical()->getSubSpheres2d(out);                // +0x48
}

float IPIPhysicalReal::getArmour(Vec3& direction)
{
    return m_information->getAIObject()->getPhysical()->getArmour(direction);         // +0x28
}

float IPIPhysicalReal::getBestArmour()
{
    return m_information->getAIObject()->getPhysical()->getBestArmour();              // +0x2C
}

int IPIPhysicalReal::getStrengthType()
{
    return m_information->getAIObject()->getPhysical()->getStrType();                 // +0x4C
}

float IPIPhysicalReal::getVolume()
{
    return m_information->getAIObject()->getPhysical()->getVolume();                  // +0x24
}

float IPIPhysicalReal::getMass()
{
    return m_information->getAIObject()->getPhysical()->getMass();                    // +0x20
}

int IPIPhysicalReal::getMaterial() const
{
    return m_information->getAIObject()->getPhysical()->getMaterial();                // +0x5C
}

float IPIPhysicalReal::getManDownTimeLeft() const
{
    return m_information->getAIObject()->getPhysical()->getManDownTimeLeft();         // +0x60
}

float IPIPhysicalReal::getMaxManDownTime() const
{
    return m_information->getAIObject()->getPhysical()->getMaxManDownTime();          // +0x64
}

bool IPIPhysicalReal::getManDown() const
{
    return m_information->getAIObject()->getPhysical()->getManDown();                 // +0x68
}

bool IPIPhysicalReal::isTouchingTerrain(Vec3& position) const
{
    return m_information->getAIObject()->getPhysical()->isTouchingTerrain(position);  // +0x50
}

bool IPIPhysicalReal::isInAir() const
{
    return m_information->getAIObject()->getPhysical()->isInAir();                    // +0x54
}

bool IPIPhysicalReal::isInWater() const
{
    return m_information->getAIObject()->getPhysical()->isInWater();                  // +0x58
}

} // namespace ai
} // namespace hfe
} // namespace dice
