// Enables or disables physics on the bot's primary equipment when its current state differs.

// static init: ELF 0x1DDF20 (__tcf_0 0x1DDF80, _GLOBAL__I 0x1DDFB0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAPhysicsEnable.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DDD00 (C1; C2 0x1DDCB0)
BAPAPhysicsEnable::BAPAPhysicsEnable(bool enable)
    : BAPAction(UninitializedPatternId()), m_enable(enable)
{
}

// ELF 0x1DDD90 (D1; D2 0x1DDD50, D0 0x1DDDD0)
BAPAPhysicsEnable::~BAPAPhysicsEnable()
{
}

// ELF 0x1DDE20
bool BAPAPhysicsEnable::execute(Bot* bot, bool& success, bool& finished)
{
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (self && self->isPrimary()) {
        IAIObjectPhysical* physical = self->getAIObject()->getPhysical();
        if (m_enable) {
            if (!physical->hasPhysicsEnabled())
                physical->enablePhysics(bot->getSide());
        } else {
            if (physical->hasPhysicsEnabled())
                physical->disablePhysics(bot->getSide());
        }
    }
    finished = true;
    success = true;
    return true;
}

// ELF 0x1DDFD0
BAPStatementType BAPAPhysicsEnable::getId() const
{
    return BAPSTPhysicsEnable;
}

// ELF 0x1DDFE0
bool BAPAPhysicsEnable::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
