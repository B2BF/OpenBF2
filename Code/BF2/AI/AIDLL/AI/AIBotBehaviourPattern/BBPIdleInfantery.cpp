// Idle pattern of infantry (AI/AIDLL/AI/AIBotBehaviourPattern/BBPIdleInfantery.cpp in the image).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x290960 (__tcf_0 0x2909C0, _GLOBAL__I 0x2909F0): <iostream>'s Init object only.
#include <iostream>

#include "BBPIdleInfantery.h"

#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPConWeaponIsZoomed.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWIdle.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x28F520
BBPIdleInfantery::BBPIdleInfantery(int id)
    : BBPIdle(id)
{
}

// ELF 0x28F5A0
BBPIdleInfantery::~BBPIdleInfantery()
{
}

// ELF 0x28F650. Stand still with reset controls; near a strategic object, also unzoom a zoomed
// weapon (BAPATrigger on the zoom input while not zoomed) and, for 0x200000-flagged equipment,
// take soldier pose 2. The plan root is a BAPIWIdle around `while (true) parallel`.
BotScript* BBPIdleInfantery::createPlan(Bot* bot)
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

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId)
        return bot->getPlan();
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }

    bot->getSquadMember()->clearRequestAction();   // SquadMember::clearRequestAction

    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_patternId, false, 99, false));
    BAPAResetControls* const resetControls = checkPointer(new BAPAResetControls(true, true, true, m_patternId));
    parallel->addStatement(resetControls);

    const Vec3& position = equipment->getPosition();
    AIStrategicObject* const closest =
        AIStrategicObjectManager::getInstance()->getClosestObject(position, bot->getSide(), false);
    if (closest)
    {
        float radius = closest->m_size[bot->getSide()];
        Vec3 centre;
        centre.x = 0.0f;
        centre.y = 0.0f;
        centre.z = 0.0f;
        vec2ToVec3XZ(closest->getPosition(bot->getSide()), centre);
        centre.y = position.y;
        Vec3 delta;
        delta.x = centre.x - position.x;
        delta.y = centre.y - position.y;
        delta.z = centre.z - position.z;
        const float distanceSqr = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        radius = radius + 2.0f;
        if (radius * radius > distanceSqr)
        {
            IPIArmament* const armament = equipment->getArmament();
            if (armament && armament->getActiveWeapon() != -1)
            {
                WeaponInfo* const weapon =
                    WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
                if (weapon && weapon->getWeaponZoom() != 0x40)
                {
                    // Right-to-left argument evaluation in the image: the controls mask and both
                    // conditions are built before the input is read again.
                    const int patternId = m_patternId;
                    const unsigned long long controls = static_cast<long long>(1 << weapon->getWeaponZoom());
                    BAPConFalse* const stop = checkPointer(new BAPConFalse());
                    BAPConTrue* const start = checkPointer(new BAPConTrue());
                    BAPATrigger* const unzoom = checkPointer(new BAPATrigger(
                        static_cast<io::PlayerInputMap>(weapon->getWeaponZoom()), start, stop, controls, patternId,
                        0.0, false));
                    BAPConNot* const notZoomed = checkPointer(new BAPConNot(checkPointer(new BAPConWeaponIsZoomed())));
                    BAPFlowCIf* const ifNotZoomed =
                        checkPointer(new BAPFlowCIf(notZoomed, unzoom, 0, m_patternId, true, false));
                    parallel->addStatement(ifNotZoomed);
                }
            }
            if (equipment->isOfType(ITSoldier))
            {
                BAPASoldierPose* const pose = checkPointer(
                    new BAPASoldierPose(static_cast<SoldierPose>(2), checkPointer(new BAPConTrue()), m_patternId, true));
                parallel->addStatement(pose);
            }
        }
    }

    BAPFlowCWhile* const loop = checkPointer(new BAPFlowCWhile(checkPointer(new BAPConTrue()), parallel, m_patternId));
    BAPIWIdle* const idle = checkPointer(new BAPIWIdle(equipment->getPosition(), loop, m_patternId, bot));
    idle->setParent(this);   // +0x04 (BotScript::mpOwner)
    return idle;
}

// ELF 0x290A10
int BBPIdleInfantery::getType()
{
    return 3;
}

// ELF 0x28F620
void BBPIdleInfantery::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
