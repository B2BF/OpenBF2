// Turn statement towards an object. Recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on
// every body.

#include "BAPATurnTowardsObject.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1E2B30, __tcf_0 0x1E2B90, _GLOBAL__I 0x1E2BC0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E27C0 (C1; C2 0x1E2680): the base, the target at +0x38, the bot at +0x34. For the bot's
// equipment and then the target, a non-primary information asks for its primary object of the
// bot's side; both answers are discarded as compiled (neither information is null-checked).
// Finally the end condition's owner (not null-checked).
BAPATurnTowardsObject::BAPATurnTowardsObject(Bot* bot, unsigned int target, float turnSpeed,
                                             BAPCondition* endCondition, unsigned long long controls, int tag)
    : BAPATurn(turnSpeed, endCondition, controls, tag)
{
    m_target = target;
    m_bot = bot;
    Information* const self = bapinline::info(bot->getCurrentEquipment());
    if (!self->isPrimary())
        self->getPrimaryObject(bot->getSide());   // [UNRESOLVED] result unused in the image (0x1E28EC)
    Information* const object = bapinline::info(target);
    if (!object->isPrimary())
        object->getPrimaryObject(bot->getSide());   // [UNRESOLVED] result unused in the image (0x1E28CB)
    m_endCondition->setOwner(this);
}

// ELF 0x1E2950 (D1; D2 0x1E2900, D0 0x1E29A0): deletes the end condition.
BAPATurnTowardsObject::~BAPATurnTowardsObject()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1E2A00: target position minus the bot's equipment position (not normalised; the
// equipment information is not null-checked). A stale target: ok = false and (1, 0, 0).
Vec3 BAPATurnTowardsObject::getDirection(bool& ok)
{
    Information* const target = bapinline::info(m_target);
    ok = target != 0;
    if (!ok)
        return bapinline::vec3(1.0f, 0.0f, 0.0f);
    const Vec3& selfPosition = bapinline::info(m_bot->getCurrentEquipment())->getPosition();
    const Vec3& targetPosition = target->getPosition();
    return bapinline::vec3(targetPosition.x - selfPosition.x, targetPosition.y - selfPosition.y,
                           targetPosition.z - selfPosition.z);
}

} // namespace ai
} // namespace hfe
} // namespace dice
