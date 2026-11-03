// Tests the sensed object's visible flag while excluding indirect sensing.

// static init: ELF 0x1F1B40 (__tcf_0 0x1F1BA0, _GLOBAL__I 0x1F1BD0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectVisible.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F19B0 (C1; C2 0x1F1980)
BAPConObjectVisible::BAPConObjectVisible(unsigned int object)
{
    m_object = object;
}

// ELF 0x1F1A00 (D1; D2 0x1F19E0, D0 0x1F1A20)
BAPConObjectVisible::~BAPConObjectVisible()
{
}

// ELF 0x1F1A60
bool BAPConObjectVisible::evaluate(Bot* bot)
{
    SensedObject* object = bot->getSensedObjectFromHandle(m_object);
    return (object->m_flags & 2u) != 0 && (object->m_flags & 8u) == 0;
}

// ELF 0x1F1AA0
BAPConditionType BAPConObjectVisible::getConditionType() const
{
    return BAPCTObjectVisible;
}

// ELF 0x1F1AB0
void BAPConObjectVisible::operator<<(std::ostream& out)
{
    out << static_cast<int>(getConditionType()) << "(" << m_object << ")";
}

} // namespace ai
} // namespace hfe
} // namespace dice
