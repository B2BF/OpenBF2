// Tests the object's position against the selected vehicle and equipment navigation constraints.

// static init: ELF 0x1F1520 (__tcf_0 0x1F1580, _GLOBAL__I 0x1F15B0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectValidPosition.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IAIPathfinding.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F12F0 (C1; C2 0x1F1260)
BAPConObjectValidPosition::BAPConObjectValidPosition(unsigned int object)
{
    m_object = object;
    Information* information = bapinline::info(object);
    m_vehicle = information->getMobile()->getVehicleNumber();
    m_equipment = information->getUnit()->getEquipmentType();
}

// ELF 0x1F13C0 (C1; C2 0x1F1380)
BAPConObjectValidPosition::BAPConObjectValidPosition(unsigned int object, int vehicle, int equipment)
    : m_vehicle(vehicle), m_equipment(equipment)
{
    m_object = object;
}

// ELF 0x1F1420 (D1; D2 0x1F1400, D0 0x1F1440)
BAPConObjectValidPosition::~BAPConObjectValidPosition()
{
}

// ELF 0x1F1480
bool BAPConObjectValidPosition::evaluate(Bot*)
{
    Information* object = bapinline::info(m_object);
    if (!object)
        return true;
    const Vec3& position = object->getPosition();
    return IAIPathfinding::instance->isValidPosition(m_vehicle, m_equipment, position, true);
}

// ELF 0x1F15D0
BAPConditionType BAPConObjectValidPosition::getConditionType() const
{
    return BAPCTObjectValidPosition;
}

} // namespace ai
} // namespace hfe
} // namespace dice
