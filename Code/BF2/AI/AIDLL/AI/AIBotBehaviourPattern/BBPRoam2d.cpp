// Ground roaming pattern, recovered from libaidll.so (Linux 1.5, ia-32).

// static init: ELF 0x29B9A0 (__tcf_0 0x29BA00, _GLOBAL__I 0x29BA30): <iostream>'s Init object only.
#include <iostream>

#include "BBPRoam2d.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x29B560 (C2), 0x29B5A0 (C1).
BBPRoam2d::BBPRoam2d(int patternId)
{
    m_patternId = patternId;
}

// ELF 0x29B5E0 (D2), 0x29B620 (D1), 0x29B660 (D0).
BBPRoam2d::~BBPRoam2d()
{
}

// ELF 0x29B700
BotScript* BBPRoam2d::createPlan(Bot* bot)
{
    const bool initiated = m_initiated[bot->getBotId()];
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    if (!initiated)
        return 0;

    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->getPlan()) {
        delete bot->getPlan();
        bot->setPlan(0);
    }
    bot->getSquadMember()->clearRequestAction();
    IPIMobile* const mobile = self->getMobile();
    const unsigned long long controls = self->getControlInfo()->getDriveControls();
    const float radius = self->getPhysical()->getRadius() * 4.0f;
    const float speed = mobile->getMaxSpeed();
    BAPConPosition* const arrived = new BAPConPosition(m_positions[bot->getBotId()], radius);
    BAPAMoveToFinding* const plan = new BAPAMoveToFinding(bot, m_positions[bot->getBotId()], speed, speed,
                                                       arrived, controls, m_patternId, 0.0f, 0.0f);
    plan->mpOwner = this;
    return plan;
}

// ELF 0x29BA50
int BBPRoam2d::getRoamType()
{
    return 1;
}

// ELF 0x29B6A0
void BBPRoam2d::init(Bot* bot, const Vec3& position)
{
    m_initiated[bot->getBotId()] = true;
    m_positions[bot->getBotId()] = position;
}

} // namespace ai
} // namespace hfe
} // namespace dice
