// Triggerable pattern (man-down bots and world triggerables).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32).

// static init: ELF 0x2A7F80 (__tcf_0 0x2A7FE0, _GLOBAL__I 0x2A8010): <iostream>'s Init object only.
#include <iostream>

#include "BBPTriggerable.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConTriggerableActive.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWIdle.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIPlanInterpreter/VehicleControlInline.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Game/Objects/Game/Triggerable.h"
#include "../../../../Game/Objects/TemplateLeaves.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2A6970 (C2) / 0x2A6AF0 (C1): BBPPattern(), +0x08 = id, +0x0C = checkPointer(new[getMaxNBots]); +0x14 = 0 (0x2A69D3, stored before +0x0C).
BBPTriggerable::BBPTriggerable(int id)
    : BBPPattern()
{
    m_id = id;
    m_triggerable = 0;
    m_perBot = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x2A6C70 (D2) / 0x2A6CC0 (D1) / 0x2A6D10 (D0): delete[] +0x0C when set, then ~BBPPattern.
BBPTriggerable::~BBPTriggerable()
{
    if (m_perBot)
        delete[] m_perBot;
}

// ELF 0x2A6DA0: retain this pattern's existing plan, otherwise approach the triggerable and
// repeatedly trigger it while it is inactive. The inactive-pattern path destroys the old plan.
BotScript* BBPTriggerable::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    bot->getSquadMember()->clearRequestAction();
    bot->getCurrentEquipment();
    if (bot->hasPlan())
    {
        if (static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_id)
            return bot->getPlan();
    }
    else if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getSquadMember()->clearRequestAction();
    bot->getScriptContext()->clear();
    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_id, -1, false));
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const unsigned long long controls = equipment->getControlInfo()->getDriveControls();
    // Shipped defect: this condition is constructed and leaked; the movement receives null.
    new BAPConFalse();
    const float radius = 0.6f * m_triggerable->m_template->m_radius;
    const Mat4& transform = m_triggerable->getAbsoluteTransformation();
    Vec3 direction = { transform.m[8], transform.m[9], transform.m[10] };
    vehiclecontrol::normalizeVec3(direction);
    const Vec3& position = m_triggerable->getAbsolutePosition();
    const Vec3 goal = { direction.x * 2.0f + position.x, direction.y * 2.0f + position.y,
                        direction.z * 2.0f + position.z };
    parallel->addStatement(checkPointer(new BAPAMoveToFinding(bot, goal, 5.0f, 5.0f, 0,
                                                             controls, m_id, radius, 0.0f)));

    BAPConFalse* const stop = checkPointer(new BAPConFalse());
    BAPConTrue* const start = checkPointer(new BAPConTrue());
    BAPATrigger* const trigger = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(10),
                                                            start, stop, 0x400ULL, m_id, 0.0, false));
    BAPConTriggerableActive* const active = checkPointer(new BAPConTriggerableActive(m_triggerable));
    BAPConNot* const inactive = checkPointer(new BAPConNot(active));
    parallel->addStatement(checkPointer(new BAPFlowCIf(inactive, trigger, 0, m_id, true, false)));
    BAPConTrue* const repeat = checkPointer(new BAPConTrue());
    BAPFlowCWhile* const loop = checkPointer(new BAPFlowCWhile(repeat, parallel, true, m_id));
    BAPIWIdle* const plan = checkPointer(new BAPIWIdle(equipment->getPosition(), loop, m_id, bot));
    plan->m_owner = this;
    return plan;
}

// ELF 0x2A6D60: bot->invalidatePlan() (+0x5C), m_initiated[bot->getBotId()] = true, +0x14.
void BBPTriggerable::init(Bot* bot, world::Triggerable* triggerable)
{
    bot->invalidatePlan();
    m_initiated[bot->getBotId()] = true;
    m_triggerable = triggerable;
}

} // namespace ai
} // namespace hfe
} // namespace dice
