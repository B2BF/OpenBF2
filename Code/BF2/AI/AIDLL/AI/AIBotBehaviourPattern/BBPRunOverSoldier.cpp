// Recovered from libaidll.so 1.5.3153 (ia-32); objdump listings with Ghidra cross-checks, ELF
// addresses on every body. Static init: ELF 0x29CAE0 (the <iostream> guard object only).

#include "BBPRunOverSoldier.h"

#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPARunOverObject.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
// static init: ELF 0x29CAE0 (__tcf_0 0x29CB40, _GLOBAL__I 0x29CB70): <iostream>'s Init object only.

#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x29C2D0: BBPPattern(), the vtable store (GOT -0x7CC + 8), the argument at +0x08, then
// `new unsigned int[getMaxNBots()]` (not initialised) through the inlined checkPointer into +0x0C
// (operator new 0x10).
BBPRunOverSoldier::BBPRunOverSoldier(int id)
    : m_patternId(id)
{
    m_target = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x29C450 (D2) / 0x29C4A0 (D1) / 0x29C4F0 (D0): delete[] +0x0C when set, then ~BBPPattern.
BBPRunOverSoldier::~BBPRunOverSoldier()
{
    if (m_target)
        delete[] m_target;
}

namespace {

// The image's "drop the plan" sequence (0x29C9F6): the plan's deleting destructor, then
// Bot +0x64 setPlan(0).
void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

} // namespace

// ELF 0x29C580 (listing; Ghidra cross-check). Without init for this bot: clear the debugger and
// the script context, drop the plan, no plan. Otherwise the current plan is kept while it is this
// pattern's fire wrapper (getId 15, info type 0) on the same target; else it is dropped and, when
// the bot's equipment has a unit and a mobile part, the plan is BAPIWFire(bot, target, true,
// false, 4, -1, BAPARunOverObject(target, drive controls, id), id) owned by this pattern, with
// every control reset. The equipment's control part is not null-tested (as shipped).
BotScript* BBPRunOverSoldier::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    bot->getCurrentEquipment();   // 0x29C60D, result unused
    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0
        && static_cast<BAPIWFire*>(bot->getPlan())->getTarget() == m_target[bot->getBotId()])
        return bot->getPlan();
    dropPlan(bot);

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment || !equipment->getUnit() || !equipment->getMobile())
        return 0;
    IPIControlInfo* const controlInfo = equipment->getControlInfo();
    BAPARunOverObject* const runOver = checkPointer(
        new BAPARunOverObject(m_target[bot->getBotId()], controlInfo->getDriveControls(), m_patternId));
    BAPIWFire* const root = checkPointer(
        new BAPIWFire(bot, m_target[bot->getBotId()], true, false, 4, -1, runOver, m_patternId));
    root->setParent(this);   // +0x04 (BotScript::mpOwner)
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return root;
}

// ELF 0x29C540: marks the bot initiated and stores its target.
void BBPRunOverSoldier::init(Bot* bot, unsigned int target)
{
    const unsigned int id = bot->getBotId();
    m_initiated[id] = true;
    m_target[id] = target;
}

} // namespace ai
} // namespace hfe
} // namespace dice
