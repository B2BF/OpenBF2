// IPIPhysicalDummy. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2EFC50..0x2F018B; the inline
// slots are in IPIPhysicalDummy.h.

#include "IPIPhysicalDummy.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EFF40, __tcf_0 0x2EFFA0, _GLOBAL__I 0x2EFFD0).
#include <iostream>
#include <new>

#include "Information.h"
#include "../AIBotBehaviour/CustomCurve.h"                          // SCurve::calculate (static)
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"                // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"        // IAIObject, IAIObjectPhysical

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2EFC50 / 0x2EFCC0: the arguments in order; m_mod, m_inAir and m_inWater are not written.
IPIPhysicalDummy::IPIPhysicalDummy(Information* information, float health,
                                   BoundingBox* boundingBox, LocalBoundingBox* localBoundingBox,
                                   float radius, float armour, float bestArmour,
                                   int strengthType, float volume, float mass,
                                   bool touchingTerrain, int material)
{
    m_information = information;
    m_health = health;
    m_boundingBox = boundingBox;
    m_localBoundingBox = localBoundingBox;
    m_radius = radius;
    m_armour = armour;
    m_bestArmour = bestArmour;
    m_strengthType = strengthType;
    m_volume = volume;
    m_mass = mass;
    m_touchingTerrain = touchingTerrain;
    m_material = material;
}

// ELF 0x2EFD30 / 0x2EFD80 / 0x2EFDD0: deletes the bounding box, then the local bounding box
// (scalar deallocation, no visible destructor call; BoundingBox is incomplete here, as in
// IPICover).
IPIPhysicalDummy::~IPIPhysicalDummy()
{
    ::operator delete(m_boundingBox);
    delete m_localBoundingBox;
}

// ELF 0x2EFE30: setTime(double) through the vtable with the game time.
void IPIPhysicalDummy::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

// ELF 0x2EFE70. Ignores the time. Takes the health from the information's ENGINE object
// (IAIObject +0x94 physical; the dummy record's object is not tested) when its maximum health is
// positive, else zero, and sets the modifier to 0.5 + 0.5 * SCurve(health), as
// IPIPhysicalReal::setTime does. ELF comparison: a NaN maximum selects zero (PE not compared).
void IPIPhysicalDummy::setTime(double /*time*/)
{
    IAIObjectPhysical* const physical = m_information->getAIObject()->getPhysical();   // +0x94
    if (physical->getMaxHealth() > 0.0f)                                              // +0x1C
        m_health = physical->getHealth();                                             // +0x18
    else
        m_health = 0.0f;
    const float half = 0.5f;
    m_mod = half;
    m_mod = SCurve::calculate(m_health) * half + m_mod;
}

// ELF 0x2EFEF0: the engine object's maximum health (IAIObjectPhysical +0x1C).
float IPIPhysicalDummy::getMaxHealth()
{
    return m_information->getAIObject()->getPhysical()->getMaxHealth();
}

// ELF 0x2EFF20.
int IPIPhysicalDummy::getMaterial() const
{
    return m_material;
}

// ELF 0x2EFF30. Empty; the image keeps a 0x148-byte frame and no code (the body's locals were
// compiled out).
void IPIPhysicalDummy::setHealth(float /*health*/)
{
}

} // namespace ai
} // namespace hfe
} // namespace dice
