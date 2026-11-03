// Defuse-mines pattern: per-bot plan parameters that BBDefuseMines::calculateUrgency stores through
// init. Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x244060 (__tcf_0 0x2440C0, _GLOBAL__I 0x2440F0): <iostream>'s Init object only.
#include <iostream>

#include "BBPDefuseMines.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPALookAtPoint.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAOverrideSensingAgent.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPICChangeVehicle.h"
#include "../AIBotActionPlan/BAPIWDefuseMines.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x240F80 (C2) / 0x2414A0 (C1): BBPPattern(), the argument to +0x08, four getMaxNBots-sized
// arrays (each checkPointer'd; the Vec3 ones run the zeroing Vec3 constructor loop, rodata
// 0x49CB34 0.0f, reproduced here by value-initialisation), then for each bot (getMaxNBots re-read
// per pass) mine and weapon index -1.
BBPDefuseMines::BBPDefuseMines(int value)
    : BBPPattern()
{
    m_unknown8 = value;
    m_mine = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_weaponIndex = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_minePosition = checkPointer(new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
    m_targetPosition = checkPointer(new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
    for (int i = 0; i < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots(); ++i)
    {
        m_mine[i] = 0xFFFFFFFFu;
        m_weaponIndex[i] = -1;
    }
}

// ELF 0x2419C0 (D2) / 0x241A50 (D1) / 0x241AE0 (D0): delete[] +0x0C..+0x18 in order when set, then
// the base.
BBPDefuseMines::~BBPDefuseMines()
{
    if (m_mine)
        delete[] m_mine;
    if (m_weaponIndex)
        delete[] m_weaponIndex;
    if (m_minePosition)
        delete[] m_minePosition;
    if (m_targetPosition)
        delete[] m_targetPosition;
}

// ELF 0x241C00
BotScript* BBPDefuseMines::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            BotScript* previous = bot->getPlan();
            if (previous)
                delete previous;
            bot->setPlan(0);
        }
        return 0;
    }
    bot->getPlayer()->getSpawnKit();
    bot->getCommunicationAgent();
    if (bot->hasPlan()
        && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_unknown8
        && static_cast<BAPStatement*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 1)
    {
        const unsigned int previousMine = static_cast<BAPIWDefuseMines*>(bot->getPlan())->m_mine;
        if (previousMine == m_mine[bot->getBotId()])
            return bot->getPlan();
    }
    const unsigned int mine = m_mine[bot->getBotId()];
    bot->getSquadMember()->clearRequestAction();
    bot->getSensingAgent()->clearPlanTargetInformation();
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    if (bot->getPlan())
    {
        BotScript* previous = bot->getPlan();
        if (previous)
            delete previous;
        bot->setPlan(0);
    }
    bot->getScriptContext()->clear();
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    if (!self || !self->getUnit() || mine == 0)
        return 0;
    IPIMobile* mobile = self->getMobile();
    IPIControlInfo* controls = self->getControlInfo();
    const int weaponIndex = m_weaponIndex[bot->getBotId()];
    if (weaponIndex == -1)
    {
        self->getAIObject()->update();
        return 0;
    }
    const int side = bot->getSide();
    const unsigned int soldierHandle = bot->getPlayer()->getSoldier()->getInformation(side);
    Information* soldier = informationManager->getInformation(soldierHandle);
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(soldier->getArmament()->getWeapon(weaponIndex));
    BAPCombinerSerial* root = checkPointer(new BAPCombinerSerial(m_unknown8, -1));
    BAPCombinerSerial* sequence = checkPointer(new BAPCombinerSerial(m_unknown8, -1));
    sequence->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_unknown8)));
    if (!(self->getObjectTypeFlags() & 0x200000))
    {
        BAPCombinerParallel* leave = checkPointer(new BAPCombinerParallel(m_unknown8, -1, false));
        leave->addStatement(checkPointer(new BAPICChangeVehicle(soldierHandle)));
        BAPConFalse* stop = checkPointer(new BAPConFalse());
        BAPConTrue* start = checkPointer(new BAPConTrue());
        leave->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(10), start, stop,
                                                        0x400ULL, m_unknown8, 0.0, false)));
        sequence->addStatement(leave);
    }
    else
    {
        const unsigned long long driveControls = controls->getDriveControls();
        const float arriveSpeed = mobile->getMaxSpeed();
        const float speed = mobile->getMaxSpeed();
        const Vec3& destination = m_targetPosition[bot->getBotId()];
        sequence->addStatement(checkPointer(new BAPAMoveToFinding(bot, destination, speed, arriveSpeed,
                                                                  0, driveControls, m_unknown8, 0.0f, 0.0f)));
        // shipped defect: a signed 32-bit input mask is sign-extended to 64 bits.
        const unsigned int activateBit = 1u << (weapon->getWeaponActivate() & 31);
        const unsigned long long activateMask = static_cast<unsigned long long>(static_cast<long long>(static_cast<int>(activateBit)));
        BAPConFalse* activateStop = checkPointer(new BAPConFalse());
        BAPConTrue* activateStart = checkPointer(new BAPConTrue());
        const io::PlayerInputMap activate = static_cast<io::PlayerInputMap>(weapon->getWeaponActivate());
        sequence->addStatement(checkPointer(new BAPATrigger(activate, activateStart, activateStop,
                                                           activateMask, m_unknown8, 0.0, false)));
        BAPCombinerParallel* defuse = checkPointer(new BAPCombinerParallel(m_unknown8, -1, false));
        defuse->addStatement(checkPointer(new BAPASoldierPose(SPCrouching, checkPointer(new BAPConTrue()),
                                                             m_unknown8, true)));
        defuse->addStatement(checkPointer(new BAPAOverrideSensingAgent()));
        const unsigned long long lookControls = self->getControlInfo()->getLookControls();
        BAPConFalse* lookCondition = checkPointer(new BAPConFalse());
        const Vec3& minePosition = m_minePosition[bot->getBotId()];
        defuse->addStatement(checkPointer(new BAPALookAtPoint(minePosition, bot, 0.0f, lookCondition,
                                                             lookControls, m_unknown8)));
        const unsigned int fireBit = 1u << (weapon->getWeaponFire() & 31);
        const unsigned long long fireMask = static_cast<unsigned long long>(static_cast<long long>(static_cast<int>(fireBit)));
        BAPConFalse* abort = checkPointer(new BAPConFalse());
        BAPConFalse* fireStop = checkPointer(new BAPConFalse());
        BAPConTrue* fireStart = checkPointer(new BAPConTrue());
        const io::PlayerInputMap fire = static_cast<io::PlayerInputMap>(weapon->getWeaponFire());
        defuse->addStatement(checkPointer(new BAPATriggerContinously(fire, fireStart, fireStop, abort,
                                                                   fireMask, m_unknown8, 0.0)));
        sequence->addStatement(checkPointer(new BAPFlowCWhile(checkPointer(new BAPConTrue()), defuse, true, m_unknown8)));
    }
    root->addStatement(sequence);
    const unsigned int target = m_mine[bot->getBotId()];
    BAPIWDefuseMines* wrapper = checkPointer(new BAPIWDefuseMines(bot, target, root, m_unknown8));
    wrapper->m_owner = this;
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return wrapper;
}

// ELF 0x241B80: one getBotId; marks the bot initiated (BBPPattern +0x04) and stores the parameters.
void BBPDefuseMines::init(unsigned int mine, int weaponIndex, Bot* bot, Vec3 minePosition, Vec3 targetPosition)
{
    const unsigned int botId = bot->getBotId();
    m_initiated[botId] = true;
    m_mine[botId] = mine;
    m_weaponIndex[botId] = weaponIndex;
    m_minePosition[botId] = minePosition;
    m_targetPosition[botId] = targetPosition;
}

} // namespace ai
} // namespace hfe
} // namespace dice
