// Bridge-repair pattern: per-bot plan parameters that BBBridgeRepair::calculateUrgency stores through
// init. Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x232140 (__tcf_0 0x2321A0, _GLOBAL__I 0x2321D0): <iostream>'s Init object only.
#include <iostream>

#include "BBPBridgeRepair.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPAMoveToDirect.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAOverrideSensingAgent.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPICChangeVehicle.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
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

// ELF 0x22E4D0 (C2) / 0x22EC30 (C1): BBPPattern(), the argument to +0x08, six getMaxNBots-sized
// arrays (each checkPointer'd; the Vec3 ones run the zeroing Vec3 constructor loop, rodata
// 0x49CB34 0.0f, reproduced here by value-initialisation), then for each bot (getMaxNBots re-read
// per pass) segment and weapon index -1 and the near-request flag false.
BBPBridgeRepair::BBPBridgeRepair(int value)
    : BBPPattern()
{
    m_unknown8 = value;
    m_segment = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_weaponIndex = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_direction = checkPointer(new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
    m_nearRequest = checkPointer(new bool[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_standPosition = checkPointer(new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
    m_segmentEnd = checkPointer(new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
    for (int i = 0; i < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots(); ++i)
    {
        m_segment[i] = 0xFFFFFFFFu;
        m_weaponIndex[i] = -1;
        m_nearRequest[i] = false;
    }
}

// ELF 0x22F390 (D2) / 0x22F450 (D1) / 0x22F510 (D0): delete[] +0x0C..+0x20 in order when set, then
// the base.
BBPBridgeRepair::~BBPBridgeRepair()
{
    if (m_segment)
        delete[] m_segment;
    if (m_weaponIndex)
        delete[] m_weaponIndex;
    if (m_direction)
        delete[] m_direction;
    if (m_nearRequest)
        delete[] m_nearRequest;
    if (m_standPosition)
        delete[] m_standPosition;
    if (m_segmentEnd)
        delete[] m_segmentEnd;
}

// ELF 0x22F6E0
BotScript* BBPBridgeRepair::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()]) {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan()) {
            delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    bot->getPlayer()->getSpawnKit();
    bot->getCommunicationAgent();
    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_unknown8
        && static_cast<BAPStatement*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0
        && static_cast<BAPIWFire*>(bot->getPlan())->getTarget() == m_segment[bot->getBotId()])
        return bot->getPlan();

    const unsigned int segment = m_segment[bot->getBotId()];
    bot->getSquadMember()->clearRequestAction();
    bot->getSensingAgent()->clearPlanTargetInformation();
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    if (bot->getPlan()) {
        delete bot->getPlan();
        bot->setPlan(0);
    }
    bot->getScriptContext()->clear();

    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_segment[bot->getBotId()]);
    if (!self || !self->getUnit() || !target)
        return 0;
    bot->setBridgeRepairTarget(segment);
    IPIMobile* const mobile = self->getMobile();
    IPIControlInfo* const controls = self->getControlInfo();
    const int weaponIndex = m_weaponIndex[bot->getBotId()];
    if (weaponIndex == -1) {
        self->getAIObject()->update();
        return 0;
    }

    const int side = bot->getSide();
    const unsigned int soldierHandle = bot->getPlayer()->getSoldier()->getInformation(side);
    Information* const soldier = informationManager->getInformation(soldierHandle);
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(soldier->getArmament()->getWeapon(weaponIndex));
    BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_unknown8, -1));
    BAPCombinerSerial* const sequence = checkPointer(new BAPCombinerSerial(m_unknown8, -1));
    sequence->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_unknown8)));
    BAPStatement* last;
    if (!(self->getObjectTypeFlags() & 0x200000)) {
        BAPCombinerSerial* const leave = checkPointer(new BAPCombinerSerial(m_unknown8, -1));
        BAPConFalse* const stop = checkPointer(new BAPConFalse());
        BAPConTrue* const start = checkPointer(new BAPConTrue());
        leave->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(10), start, stop,
                                                       0x400ULL, m_unknown8, 0.0, false)));
        leave->addStatement(checkPointer(new BAPICChangeVehicle(soldierHandle)));
        last = leave;
    } else {
        if (m_nearRequest[bot->getBotId()] && !bot->getSquadMember()->getSquad()->haveConfirmedRepairRequest()) {
            bot->getSquadMember()->getSquad()->confirmRepairRequested();
            bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(6),
                static_cast<AIRadioChannelType>(3), 3, 2.0f, 0);
        }
        const Vec3 standPosition = m_standPosition[bot->getBotId()];
        const Vec3 segmentEnd = m_segmentEnd[bot->getBotId()];
        const unsigned long long drive = controls->getDriveControls();
        const float arriveSpeed = mobile->getMaxSpeed();
        const float speed = mobile->getMaxSpeed();
        sequence->addStatement(checkPointer(new BAPAMoveToFinding(bot, standPosition, speed, arriveSpeed,
                                                                 0, drive, m_unknown8, 0.0f, 0.0f)));

        // shipped defect: input masks shift a signed 32-bit one, then sign-extend to 64 bits.
        const unsigned int activateBit = 1u << (weapon->getWeaponActivate() & 31);
        const unsigned long long activateMask = static_cast<unsigned long long>(static_cast<long long>(static_cast<int>(activateBit)));
        BAPConFalse* const stop = checkPointer(new BAPConFalse());
        BAPConTrue* const start = checkPointer(new BAPConTrue());
        sequence->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(weapon->getWeaponActivate()),
                                                            start, stop, activateMask, m_unknown8, 0.0, false)));
        const unsigned long long directControls = self->getControlInfo()->getDriveControls();
        const Vec2 endXZ = {segmentEnd.x, segmentEnd.z};
        BAPConPosition* const atEnd = checkPointer(new BAPConPosition(endXZ, 1.0f));
        const float directArriveSpeed = mobile->getMaxSpeed();
        const float directSpeed = mobile->getMaxSpeed();
        sequence->addStatement(checkPointer(new BAPAMoveToDirect(bot, segmentEnd, directSpeed, directArriveSpeed,
                                                                atEnd, directControls, m_unknown8)));
        sequence->addStatement(checkPointer(new BAPAResetControls(true, false, false, m_unknown8)));
        BAPCombinerParallel* const repair = checkPointer(new BAPCombinerParallel(m_unknown8, -1, false));
        repair->addStatement(checkPointer(new BAPASoldierPose(static_cast<SoldierPose>(2),
                                                            checkPointer(new BAPConTrue()), m_unknown8, true)));
        repair->addStatement(checkPointer(new BAPAOverrideSensingAgent()));
        const unsigned long long lookControls = self->getControlInfo()->getLookControls();
        repair->addStatement(checkPointer(new BAPALookAtObject(bot, segment, 0.0f, checkPointer(new BAPConFalse()),
                                                              lookControls, m_unknown8, 0, 0.0f)));
        const unsigned int fireBit = 1u << (weapon->getWeaponFire() & 31);
        const unsigned long long fireMask = static_cast<unsigned long long>(static_cast<long long>(static_cast<int>(fireBit)));
        BAPConFalse* const abort = checkPointer(new BAPConFalse());
        BAPConFalse* const fireStop = checkPointer(new BAPConFalse());
        BAPConTrue* const fireStart = checkPointer(new BAPConTrue());
        repair->addStatement(checkPointer(new BAPATriggerContinously(static_cast<io::PlayerInputMap>(weapon->getWeaponFire()),
                                                                    fireStart, fireStop, abort, fireMask, m_unknown8, 0.0)));
        last = checkPointer(new BAPFlowCWhile(checkPointer(new BAPConTrue()), repair, true, m_unknown8));
    }
    sequence->addStatement(last);
    root->addStatement(sequence);
    const unsigned int requestHandle = target->getHandle();
    bot->getSquadMember()->setCurrentRequestAction(2, requestHandle);
    BAPIWFire* const wrapper = checkPointer(new BAPIWFire(bot, m_segment[bot->getBotId()], true, false,
                                                       weaponIndex, -1, root, m_unknown8));
    wrapper->m_owner = this;
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return wrapper;
}

// ELF 0x22F5E0: marks the bot initiated (BBPPattern +0x04) and stores the parameters; getBotId is
// called once per store.
void BBPBridgeRepair::init(unsigned int segment, int weaponIndex, Vec3 direction, Bot* bot, bool nearRequest,
                           Vec3 standPosition, Vec3 segmentEnd)
{
    m_initiated[bot->getBotId()] = true;
    m_segment[bot->getBotId()] = segment;
    m_weaponIndex[bot->getBotId()] = weaponIndex;
    m_direction[bot->getBotId()] = direction;
    m_nearRequest[bot->getBotId()] = nearRequest;
    m_standPosition[bot->getBotId()] = standPosition;
    m_segmentEnd[bot->getBotId()] = segmentEnd;
}

} // namespace ai
} // namespace hfe
} // namespace dice
