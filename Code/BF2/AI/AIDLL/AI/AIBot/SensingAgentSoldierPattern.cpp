// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIBot\SensingAgentSoldierPattern.cpp"
// (asserts "No info" / "No control info" in 0x101368F0). Split out of the former
// SensingAgentPatterns.cpp; the functions are unchanged.
#include "SensingAgentPatterns.h"
#include <iostream>
// static init: ELF 0x1C2980, 0x1C29E0, 0x1C2A10 (Soldier)

#include "SensingAgent.h"
#include "BotMain.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPALookInDir.h"
#include "../AIBotActionPlan/BAPAMergeSoldierInputs.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

SensingAgentSoldierPattern::SensingAgentSoldierPattern()
{
}

SensingAgentSoldierPattern::~SensingAgentSoldierPattern()
{
}

int SensingAgentSoldierPattern::getId() const
{
    return m_id;
}

void SensingAgentSoldierPattern::reset(int id)
{
    m_id = id;
}

// ELF 0x1C1F60. Both createPlan overloads pass a null `plan`, so the reuse arm (the agent's plan
// kept when it is this pattern's: false; otherwise `plan` deleted and the agent's cleared) never
// runs from them. True with the current equipment's control info; otherwise `plan` = 0.
bool SensingAgentSoldierPattern::initPlan(SensingAgent* agent, Bot* bot, IPIControlInfo*& control,
                                          BotScript*& plan)
{
    if (plan)
    {
        if (static_cast<BAPStatement*>(agent->m_plan)->getPatternId() == getId())
            return false;
        delete plan;
        agent->m_plan = 0;
    }
    agent->m_bapContext->clear();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (equipment)
    {
        control = equipment->getControlInfo();
        if (control)
            return true;
    }
    plan = 0;
    return false;
}

// ELF 0x1C2050: parallel(look, BAPAMergeSoldierInputs), owned by the pattern.
BotScript* SensingAgentSoldierPattern::endPlan(BAPALook* look, Bot* /*bot*/)
{
    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(getId(), -1, false));
    BAPAMergeSoldierInputs* const merge = checkPointer(new BAPAMergeSoldierInputs(getId()));
    parallel->addStatement(look);
    parallel->addStatement(merge);
    parallel->mpOwner = this;
    return parallel;
}

// ELF 0x1C2310: look in the direction exactly (tolerance 0), never ending.
BotScript* SensingAgentSoldierPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (initPlan(agent, bot, control, plan))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookInDir* const look = checkPointer(
            new BAPALookInDir(direction, 0.0f, checkPointer(new BAPConFalse()), controls, id));
        plan = endPlan(look, bot);
    }
    return plan;
}

// ELF 0x1C2640: look at the object (tolerance 0).
BotScript* SensingAgentSoldierPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int target)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (initPlan(agent, bot, control, plan))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
            bot, target, 0.0f, checkPointer(new BAPConFalse()), controls, id, 0, 0.0f));
        plan = endPlan(look, bot);
    }
    return plan;
}

} // namespace ai
} // namespace hfe
} // namespace dice
