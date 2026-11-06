// [CHOSEN, not recovered] Private helpers shared by the sensing-agent pattern TUs
// (SensingAgentMouseFreeLookPattern.cpp, SensingAgentNullPattern.cpp, SensingAgentTurretPattern.cpp).
// The retail images expand them inline in each createPlan; this header only keeps one copy of the
// source after the split of the former SensingAgentPatterns.cpp. Include it from those .cpp files only.

#ifndef AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNPROLOGUE_H
#define AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNPROLOGUE_H

#include "SensingAgent.h"
#include "BotMain.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIUtils/checkPointer.h"

// The patterns' shared prologue (inline in every createPlan but the soldier's, e.g. 0x1C2B10): the
// agent's plan is kept when this pattern built it (BAPStatement +0x10 == getId()); otherwise it is
// deleted and the agent's BAP context cleared. The function returns the kept plan or null.
#define SENSING_PATTERN_KEEP_OR_DROP(agent)                                                       \
    if (BotScript* const previous = (agent)->m_plan)                                               \
    {                                                                                              \
        if (static_cast<BAPStatement*>(previous)->getPatternId() == getId())                        \
            return previous;                                                                       \
        delete previous;                                                                           \
        (agent)->m_plan = 0;                                                                       \
    }                                                                                              \
    (agent)->m_bapContext->clear()

namespace dice {
namespace hfe {
namespace ai {
namespace {

// The bot's current equipment's control info, or null (Bot +0xD8, Information +0x30).
inline IPIControlInfo* currentControlInfo(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    return equipment ? equipment->getControlInfo() : 0;
}

// BAPATriggerContinously(PIMouseLook, true, false, false, mask 0x800, id, 0.0): the mouse-look
// button held for the plan's life (0x1C06F9..0x1C0727; 0x30 bytes).
inline BAPATriggerContinously* holdMouseLook(int id)
{
    BAPCondition* const third = checkPointer(new BAPConFalse());
    BAPCondition* const second = checkPointer(new BAPConFalse());
    BAPCondition* const first = checkPointer(new BAPConTrue());
    return checkPointer(new BAPATriggerContinously(io::PIMouseLook, first, second, third, 0x800ULL, id, 0.0));
}

} // namespace
} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNPROLOGUE_H
