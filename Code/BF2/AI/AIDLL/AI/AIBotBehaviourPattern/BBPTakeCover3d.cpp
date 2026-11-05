// Take-cover pattern of aircraft (AI/AIDLL/AI/AIBotBehaviourPattern/BBPTakeCover3d.cpp in the image).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// createPlan was checked against the listing for constructor arguments (Ghidra merges the trigger's
// 64-bit mask and drops the bool arguments).

// static init: ELF 0x2A0A40 (__tcf_0 0x2A0AA0, _GLOBAL__I 0x2A0AD0): <iostream>'s Init object only.
#include <iostream>

#include "BBPTakeCover3d.h"

#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x29FD20: BBPPattern(), the vtable store, then the argument at +0x08.
BBPTakeCover3d::BBPTakeCover3d(int patternId)
    : BBPPattern()
{
    m_patternId = patternId;
}

// ELF 0x29FDA0 (D0 0x29FDE0): the vtable store, then BBPPattern's destructor; nothing of its own.
BBPTakeCover3d::~BBPTakeCover3d()
{
}

// ELF 0x29FE20
void BBPTakeCover3d::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

// ELF 0x29FE50. Keeps a plan of this pattern; otherwise drops the plan and builds
// `parallel(reset controls, trigger input 47 for 2 s, move to 500 m along the own heading)`.
// The own equipment's information is not null-checked before its mobile and control-info
// plug-ins are read, as compiled.
// [PLATFORM] precision: the x87 build adds the extended product heading * 500 to the position;
// the float expression below rounds the product first.
BotScript* BBPTakeCover3d::createPlan(Bot* bot)
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

    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId)
        return bot->getPlan();

    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    bot->getSquadMember()->clearRequestAction();

    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = self->getMobile();
    IPIControlInfo* const controlInfo = self->getControlInfo();

    BAPCombinerParallel* const plan = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
    plan->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_patternId)));
    plan->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(0x2F),
                                                    checkPointer(new BAPConTime(2.0f, false, true, true)),
                                                    checkPointer(new BAPConFalse()), 0x800000000000ULL,
                                                    m_patternId, 0.0f, false)));

    const Vec3& position = self->getPosition();             // +0x30
    const Mat4& transformation = self->getTransformation(); // +0x3C
    Vec3 offset;
    offset.x = transformation.m[8] * 500.0f;
    offset.y = transformation.m[9] * 500.0f;
    offset.z = 500.0f * transformation.m[10];
    Vec3 destination;
    destination.x = offset.x + position.x;
    destination.y = offset.y + position.y;
    destination.z = offset.z + position.z;
    plan->addStatement(checkPointer(new BAPAMoveTo3d(bot, destination, mobile->getMaxSpeed(), mobile->getMaxSpeed(),
                                                     checkPointer(new BAPConFalse()),
                                                     controlInfo->getDriveControls(), 30.0f, m_patternId)));
    plan->setParent(this);   // +0x04 (BotScript::mpOwner)
    return plan;
}

} // namespace ai
} // namespace hfe
} // namespace dice
