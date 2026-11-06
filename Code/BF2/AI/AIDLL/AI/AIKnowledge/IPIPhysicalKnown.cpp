// IPIPhysicalKnown. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2F0190..0x2F07CB; vtable
// 0x4BEFC0 (same 27 slots as IPIPhysicalReal's). The forwarders use the same IAIObjectPhysical
// slots as IPIPhysicalReal's.

#include "IPIPhysicalKnown.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2F0690, __tcf_0 0x2F06F0, _GLOBAL__I 0x2F0720).
#include <iostream>

#include "Information.h"
#include "../AIUtils/CustomCurve.h"                          // SCurve::calculate (static)
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"        // IAIObject, IAIObjectPhysical

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAIObjectPhysical* physicalOf(Information* information)
{
    return information->getAIObject()->getPhysical();                       // +0x94
}

} // anonymous namespace

// 0x2F0190: flags false, terrain position zero; health and modifier stay as allocated.
IPIPhysicalKnown::IPIPhysicalKnown(Information* information)
{
    m_information = information;
    m_touchingTerrain = false;
    m_terrainPosition.x = 0.0f;
    m_terrainPosition.y = 0.0f;
    m_terrainPosition.z = 0.0f;
    m_inAir = false;
    m_inWater = false;
    m_manDown = false;
}

IPIPhysicalKnown::~IPIPhysicalKnown()
{
}

// 0x2F02B0.
void IPIPhysicalKnown::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

// 0x2F02F0. Caches man-down, the absolute health (zero unless the maximum is positive; a NaN
// maximum gives zero, the ELF comparison), the modifier 0.5 + 0.5 * SCurve(health) and the
// terrain/air/water state.
void IPIPhysicalKnown::setTime(double /*time*/)
{
    IAIObjectPhysical* const physical = physicalOf(m_information);
    m_manDown = physical->getManDown();                                     // +0x68
    if (physical->getMaxHealth() > 0.0f)                                    // +0x1C
        m_health = physical->getHealth();                                   // +0x18
    else
        m_health = 0.0f;
    const float half = 0.5f;
    m_mod = half;
    m_mod = SCurve::calculate(m_health) * half + m_mod;
    m_touchingTerrain = physical->isTouchingTerrain(m_terrainPosition);     // +0x50
    m_inAir = physical->isInAir();                                          // +0x54
    m_inWater = physical->isInWater();                                      // +0x58
}

float IPIPhysicalKnown::getHealth() const
{
    return m_health;
}

void IPIPhysicalKnown::setHealth(float health)
{
    m_health = health;
}

float IPIPhysicalKnown::getMaxHealth()
{
    return physicalOf(m_information)->getMaxHealth();                       // +0x1C
}

float IPIPhysicalKnown::getMod()
{
    return m_mod;
}

BoundingBox* IPIPhysicalKnown::getBoundingBox()
{
    return physicalOf(m_information)->getCurrentBoundingBox();              // +0x34
}

const LocalBoundingBox* IPIPhysicalKnown::getLocalBoundingBox()
{
    return physicalOf(m_information)->getLocalBoundingBox();                // +0x30
}

float IPIPhysicalKnown::getRadius()
{
    return physicalOf(m_information)->getRadius();                          // +0x3C
}

Vec3 IPIPhysicalKnown::getSphereLocalOffset() const
{
    return physicalOf(m_information)->getSphereLocalOffset();               // +0x40
}

float IPIPhysicalKnown::getSmallestRadius() const
{
    return physicalOf(m_information)->getSmallestRadius();                  // +0x44
}

// ELF 0x2F05F0: forwards, returning the engine plug-in's answer.
float IPIPhysicalKnown::getSubSpheres2d(std::vector<Vec2>& out) const
{
    return physicalOf(m_information)->getSubSpheres2d(out);                        // +0x48
}

float IPIPhysicalKnown::getArmour(Vec3& direction)
{
    return physicalOf(m_information)->getArmour(direction);                 // +0x28
}

float IPIPhysicalKnown::getBestArmour()
{
    return physicalOf(m_information)->getBestArmour();                      // +0x2C
}

int IPIPhysicalKnown::getStrengthType()
{
    return physicalOf(m_information)->getStrType();                         // +0x4C
}

float IPIPhysicalKnown::getVolume()
{
    return physicalOf(m_information)->getVolume();                          // +0x24
}

float IPIPhysicalKnown::getMass()
{
    return physicalOf(m_information)->getMass();                            // +0x20
}

int IPIPhysicalKnown::getMaterial() const
{
    return physicalOf(m_information)->getMaterial();                        // +0x5C
}

float IPIPhysicalKnown::getManDownTimeLeft() const
{
    return physicalOf(m_information)->getManDownTimeLeft();                 // +0x60
}

float IPIPhysicalKnown::getMaxManDownTime() const
{
    return physicalOf(m_information)->getMaxManDownTime();                  // +0x64
}

bool IPIPhysicalKnown::getManDown() const
{
    return m_manDown;
}

// 0x2F0770: the cached terrain position and flag.
bool IPIPhysicalKnown::isTouchingTerrain(Vec3& position) const
{
    position = m_terrainPosition;
    return m_touchingTerrain;
}

bool IPIPhysicalKnown::isInAir() const
{
    return m_inAir;
}

bool IPIPhysicalKnown::isInWater() const
{
    return m_inWater;
}

} // namespace ai
} // namespace hfe
} // namespace dice
