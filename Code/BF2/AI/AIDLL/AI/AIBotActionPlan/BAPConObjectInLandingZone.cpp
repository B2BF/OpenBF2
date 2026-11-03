// Tests whether the object is inside the bot's assigned landing zone.

// static init: ELF 0x1EEC20 (__tcf_0 0x1EEC80, _GLOBAL__I 0x1EECB0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectInLandingZone.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIStrategicEntity/AILandingZone.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EEAD0 (C1; C2 0x1EEA90)
BAPConObjectInLandingZone::BAPConObjectInLandingZone(unsigned int object)
{
    m_object = object;
}

// ELF 0x1EEB30 (D1; D2 0x1EEB10, D0 0x1EEB50)
BAPConObjectInLandingZone::~BAPConObjectInLandingZone()
{
}

// ELF 0x1EEB90
bool BAPConObjectInLandingZone::evaluate(Bot* bot)
{
    Information* object = bapinline::info(m_object);
    if (!object || !static_cast<BotMain*>(bot)->m_ptr232)
        return false;
    const Vec3& position = object->getPosition();
    return static_cast<AILandingZone*>(static_cast<BotMain*>(bot)->m_ptr232)->isInZone(position);
}

// ELF 0x1EECD0
BAPConditionType BAPConObjectInLandingZone::getConditionType() const
{
    return BAPCTObjectInLandingZone;
}

} // namespace ai
} // namespace hfe
} // namespace dice
