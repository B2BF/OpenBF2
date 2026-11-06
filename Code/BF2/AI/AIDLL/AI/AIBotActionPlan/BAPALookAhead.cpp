// Look statement that keeps the view along the vehicle's facing. Recovered from libaidll.so
// 1.5.3153 (ia-32); ELF addresses on every body.

#include "BAPALookAhead.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1D3070, __tcf_0 0x1D30D0, _GLOBAL__I 0x1D3100).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D2E60 (C1; C2 0x1D2DE0): the base, the bot's current equipment at +0x30, then the end
// condition's owner (not null-checked).
BAPALookAhead::BAPALookAhead(Bot* bot, float lookSpeed, BAPCondition* endCondition, unsigned long long controls,
                             int tag)
    : BAPALook(lookSpeed, endCondition, controls, tag)
{
    m_equipment = bot->getCurrentEquipment();
    m_endCondition->setOwner(this);
}

// ELF 0x1D2F30 (D1; D2 0x1D2EE0, D0 0x1D2F80): deletes the end condition.
BAPALookAhead::~BAPALookAhead()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1D2FE0: the equipment's facing; ok reports whether its information still exists, and a
// stale handle yields (0, 0, 0).
Vec3 BAPALookAhead::getLookDirection(bool& ok)
{
    Information* const equipment = bapinline::info(m_equipment);
    ok = equipment != 0;
    if (!ok)
        return bapinline::vec3(0.0f, 0.0f, 0.0f);
    return equipment->getFaceing();
}

} // namespace ai
} // namespace hfe
} // namespace dice
