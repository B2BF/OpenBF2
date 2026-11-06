// Medic/engineer/support assist pattern: heal, repair or resupply a unit. Recovered from
// libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
//
// static init: ELF 0x296E20 (__tcf_0 0x296E80, _GLOBAL__I 0x296EB0): the <iostream> guard object only.

#include "BBPMedicAssist.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPAMergeSoldierInputs.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPAOverrideSensingAgent.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASendRadioMessage.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConObjectDistance.h"
#include "../AIBotActionPlan/BAPConObjectExists.h"
#include "../AIBotActionPlan/BAPConObjectHealth.h"
#include "../AIBotActionPlan/BAPConObjectInFront.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIBotActionPlan/BAPConTotalAmmo.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPConWeaponIsActive.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIBotActionPlan/BAPVariable.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../Main/IBotManager.h"
#include "WeaponChoice.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

#include <cmath>
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x290A20 (C2) / 0x290DE0 (C1): BBPPattern(), +0x08 = argument, then +0x0C/+0x10/+0x14 =
// new[getMaxNBots] (IAISettings slot 0, checkPointer'd) and every slot -1; the loop re-reads
// getMaxNBots on each iteration.
BBPMedicAssist::BBPMedicAssist(int value)
    : BBPPattern()
{
    m_id = value;
    IAISettings* settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    m_target = checkPointer(new unsigned int[settings->getMaxNBots()]);
    m_weaponIndex = checkPointer(new int[settings->getMaxNBots()]);
    m_targetPlayerId = checkPointer(new int[settings->getMaxNBots()]);
    for (int i = 0; i < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots(); ++i)
    {
        m_target[i] = 0xFFFFFFFFu;
        m_weaponIndex[i] = -1;
        m_targetPlayerId[i] = -1;
    }
}

// ELF 0x2911A0 (D2) / 0x291220 (D1) / 0x2912A0 (D0): delete[] +0x0C, +0x10, +0x14 when set,
// then the base.
BBPMedicAssist::~BBPMedicAssist()
{
    if (m_target)
        delete[] m_target;
    if (m_weaponIndex)
        delete[] m_weaponIndex;
    if (m_targetPlayerId)
        delete[] m_targetPlayerId;
}

namespace {

void dropPlan(Bot* bot)
{
    delete bot->getPlan();
    bot->setPlan(0);
}

// One weapon key as the image builds it inline: a trigger of the key that starts at once
// (BAPConTrue) and never stops by condition (BAPConFalse), with the key's control bit
// (1 << key, sign-extended from int) and no repeat time.
BAPATrigger* newKeyTrigger(const WeaponInfo* weapon, int (WeaponInfo::*key)() const, int patternId, bool flag)
{
    return checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>((weapon->*key)()),
                                        checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
                                        1 << (weapon->*key)(), patternId, 0.0, flag));
}

// The image's inlined plan-variable construction: `checkPointer(new BAPVariable<T>(context, value))`
// followed by the list insert at the context's begin.
template <class T>
BAPVariable<T>* newVariable(BAPContext* context, const T& value)
{
    BAPVariable<T>* const variable = checkPointer(new BAPVariable<T>(context, value));
    context->addVariable(variable);
    return variable;
}

// The radio message type a kit confirms for a treatment request: medic 3, engineer 4, support 10;
// 0 for any other kit (no confirmation).
int requestMessageType(int kit)
{
    if (kit == 5)
        return 3;
    if (kit == 4)
        return 4;
    if (kit == 3)
        return 10;
    return 0;
}

// The inlined Vec3 subtraction followed by the x87 length (fsqrt, sqrtf on NaN).
float distance(const Vec3& a, const Vec3& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

// ELF 0x291550 (listing; Ghidra drops the BAPATrigger/BAPASoldierPose bool arguments, merges the
// 64-bit control masks and loses the unused weapon-2 trigger). The medic (kit 5), engineer (kit 4)
// and support (kit 3) treatment plan: heal, repair or resupply the unit m_target[bot].
//
// The current plan is kept when it is this pattern's BAPIWFire on the same target; the request is
// then confirmed on the radio (enqueue 6/6/3) when the requester accepts. Otherwise the request
// action and the plan target are cleared, the plan dropped and a new one built:
//   self (equipment == target): serial { parallel { reset controls, pose (stand/lie/crouch),
//     trigger(activate key, true) } }
//   other: serial { [radio 6/6/2 when the requester confirms], reset controls,
//     while (target exists && health <= 0.99 / 0.75 (ammo <= 0.95 / 0.75 for support; the higher
//     value when a human occupies the target)) parallel { reset controls, override sensing,
//       move (to the own unit's entry position when the target cannot be reached by pathfinding,
//       else to the target), look at target, pose, if (within half the replenish radius plus both
//       radii) treat, merge soldier inputs } }
//   treat: parallel { trigger(activate key), while (true) { override sensing, look at target,
//     if (in front) hold fire while the weapon is active } (vehicles only) }, preceded by 0.5 s
//     of closing in with pathfinding when the target is a reachable vehicle.
// Shipped quirks kept: the primary object of a non-primary target is not null-checked; the
// override-sensing action of the treat block, an int plan variable and a trigger of the slot 2
// weapon's activate key are built and never used (leaked) when not added; the plan context is
// cleared after the plan is dropped; the radio statement is placed before the reset.
BotScript* BBPMedicAssist::createPlan(Bot* bot)
{
    // .bss 0x4CE4D0: no writer in the image, so the look offset is always 0.
    static float yOffset;

    if (!m_initiated[bot->getBotId()])
    {
        // 0x296DA0
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
            dropPlan(bot);
        return 0;
    }

    const int kit = bot->getPlayer()->getSpawnKit();
    CommunicationAgent* const communication = bot->getCommunicationAgent();
    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_id
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0
        && static_cast<BAPIWFire*>(bot->getPlan())->getTarget() == m_target[bot->getBotId()])
    {
        // 0x296C68
        const int requester = m_targetPlayerId[bot->getBotId()];
        if (requester >= 0)
        {
            const int type = requestMessageType(kit);
            if (type != 0 && communication->confirmReceivedMessage(static_cast<AIRadioMessageType>(type), requester))
                bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(6),
                                                                  static_cast<AIRadioChannelType>(6), 3, 1.0f, -1);
        }
        return bot->getPlan();
    }

    bot->getBotId();   // 0x2915EC, result unused
    bot->getSquadMember()->clearRequestAction();
    bot->getSensingAgent()->clearPlanTargetInformation();
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    if (bot->getPlan())
        dropPlan(bot);
    bot->getScriptContext()->clear();

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_target[bot->getBotId()]);
    if (!equipment || !equipment->getUnit())
        return 0;
    IPIMobile* const mobile = equipment->getMobile();
    if (!mobile || !target)
        return 0;
    IPIArmament* const armament = equipment->getArmament();
    IPIControlInfo* const controlInfo = equipment->getControlInfo();
    IPIPhysical* const physical = equipment->getPhysical();
    IPIPhysical* const targetPhysical = target->getPhysical();

    // 0x291753: the support kit picks among type-2 weapons, the others among type-1 weapons.
    int weaponIndex;
    if (bot->getPlayer()->getSpawnKit() == 3)
    {
        const int strengthType = targetPhysical->getStrengthType();
        const float range = distance(equipment->getPosition(), target->getPosition());
        weaponIndex = choseWeapon(armament->getWeapons(), m_weaponIndex[bot->getBotId()], range, strengthType, false,
                                  false, false, true);
    }
    else
    {
        const int strengthType = targetPhysical->getStrengthType();
        const float range = distance(equipment->getPosition(), target->getPosition());
        weaponIndex = choseWeapon(armament->getWeapons(), m_weaponIndex[bot->getBotId()], range, strengthType, false,
                                  true, false, false);
    }
    if (weaponIndex == -1)
    {
        equipment->getAIObject()->update();   // 0x296A89
        return 0;
    }
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));

    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_id, -1));
    // 0x291942 / 0x291956: IPIPhysical slot +0x34, getSmallestRadius, on both (not getRadius +0x2C).
    float radii = physical->getSmallestRadius();
    radii = radii + targetPhysical->getSmallestRadius();

    bool targetVacant;
    if (equipment == target)
    {
        // 0x295FDF: treating itself.
        BAPCombinerParallel* const self = checkPointer(new BAPCombinerParallel(m_id, -1, false));
        self->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_id)));
        self->addStatement(checkPointer(new BAPASoldierPose(SPStanding, SPLying, 3, SPCrouching, 0.0f,
                                                            checkPointer(new BAPConTrue()),
                                                            checkPointer(new BAPConFalse()), m_id, true)));
        self->addStatement(newKeyTrigger(weapon, &WeaponInfo::getWeaponActivate, m_id, true));
        serial->addStatement(self);
        targetVacant = false;
    }
    else
    {
        const bool targetIsSoldier = targetPhysical->getStrengthType() == 0;   // -0x1C9
        const float reach = weapon->getReplenishRadius() * 0.5f + radii;       // -0x1D0
        IPIUnit* const targetUnit = target->getUnit();
        // -0x1D1: approach the own unit's entry position instead of the target.
        bool toEntry = true;
        if (targetUnit)
        {
            bool noPathfinding = false;
            if (targetUnit->useNoPathfindingToGetToObject() || !target->getMobile())
                noPathfinding = true;
            toEntry = noPathfinding;
        }
        if (toEntry && !target->isPrimary())
            toEntry = !informationManager->getInformation(target->getPrimaryObject(bot->getSide()))->getMobile();   // 0x295F7F

        // 0x291A13
        BAPContext* const context = bot->getScriptContext();
        BAPVariable<WeaponInfo*>* const weaponVariable = newVariable<WeaponInfo*>(context, weapon);
        newVariable<int>(bot->getScriptContext(), 0);   // 0x291AE6, never read

        BAPConAnd* const treating = checkPointer(new BAPConAnd());
        treating->addCondition(checkPointer(new BAPConObjectExists(m_target[bot->getBotId()])));
        BAPCondition* needed;
        if (kit == 3)
        {
            // 0x295956
            if (humanPlayerInTarget(target, bot->getSide()))
                needed = checkPointer(new BAPConTotalAmmo(m_target[bot->getBotId()], 0.95f));
            else
                needed = checkPointer(new BAPConTotalAmmo(m_target[bot->getBotId()], 0.75f));
        }
        else if (humanPlayerInTarget(target, bot->getSide()))
        {
            needed = checkPointer(new BAPConObjectHealth(m_target[bot->getBotId()], 0.99f));
        }
        else
        {
            needed = checkPointer(new BAPConObjectHealth(m_target[bot->getBotId()], 0.75f));   // 0x295820
        }
        treating->addCondition(needed);

        BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_id, -1, false));
        parallel->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_id)));
        parallel->addStatement(checkPointer(new BAPAOverrideSensingAgent()));
        if (toEntry)
        {
            // 0x2952B1
            const Vec3 entry = equipment->getUnit()->getEntryPositionOfNonMobile();
            parallel->addStatement(checkPointer(new BAPAMoveToFinding(bot, entry, mobile->getMaxSpeed(), mobile->getMaxSpeed(),
                                                                      0, controlInfo->getDriveControls(), m_id, 0.5f,
                                                                      0.0f)));
        }
        else
        {
            parallel->addStatement(checkPointer(new BAPAMoveToObjectFinding(
                bot, m_target[bot->getBotId()], radii, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0,
                controlInfo->getDriveControls(), m_id, 0, 0.0f, -1)));
        }
        parallel->addStatement(checkPointer(new BAPALookAtObject(bot, m_target[bot->getBotId()], 0.0f,
                                                                 checkPointer(new BAPConFalse()),
                                                                 equipment->getControlInfo()->getLookControls(), m_id, 0,
                                                                 yOffset)));
        BAPASoldierPose* pose;
        if (target->isOfType(ITSoldier) && (kit == 5 || kit == 3))
            pose = checkPointer(new BAPASoldierPose(SPStanding, checkPointer(new BAPConTrue()), m_id,
                                                    m_target[bot->getBotId()], true));   // 0x294D90
        else
            pose = checkPointer(new BAPASoldierPose(SPStanding, checkPointer(new BAPConTrue()), m_id, true));
        parallel->addStatement(pose);

        BAPConAnd* const inReach = checkPointer(new BAPConAnd());
        inReach->addCondition(checkPointer(new BAPConObjectDistance(m_target[bot->getBotId()], reach, false, false, false)));
        BAPCombinerParallel* const treat = checkPointer(new BAPCombinerParallel(m_id, -1, false));
        treat->addStatement(newKeyTrigger(weapon, &WeaponInfo::getWeaponActivate, m_id, false));
        BAPAOverrideSensingAgent* const overrideSensing = checkPointer(new BAPAOverrideSensingAgent());
        BAPCombinerParallel* const aim = checkPointer(new BAPCombinerParallel(m_id, -1, false));
        if (!targetIsSoldier)
        {
            // 0x293B22
            aim->addStatement(overrideSensing);
            aim->addStatement(checkPointer(new BAPALookAtObject(bot, m_target[bot->getBotId()], 0.0f,
                                                                checkPointer(new BAPConFalse()),
                                                                equipment->getControlInfo()->getLookControls(), m_id, 0,
                                                                yOffset)));
            BAPATriggerContinously* const fire = checkPointer(new BAPATriggerContinously(
                static_cast<io::PlayerInputMap>(weapon->getWeaponFire()),
                checkPointer(new BAPConWeaponIsActive(weaponVariable)), checkPointer(new BAPConFalse()),
                checkPointer(new BAPConFalse()), 1 << weapon->getWeaponFire(), m_id, 0.0));
            aim->addStatement(checkPointer(new BAPFlowCIf(
                checkPointer(new BAPConObjectInFront(m_target[bot->getBotId()], 0.0f, 0.1f)), fire, 0, m_id, true, false)));
        }
        treat->addStatement(checkPointer(new BAPFlowCWhile(checkPointer(new BAPConTrue()), aim, true, m_id)));

        WeaponInfo* const other = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(2));   // 0x2923C1
        if (other)
            newKeyTrigger(other, &WeaponInfo::getWeaponActivate, m_id, false);   // built, never added

        // 0x292513: confirm the requester's radio request.
        const int requester = m_targetPlayerId[bot->getBotId()];
        if (requester >= 0 && requestMessageType(kit) != 0)
        {
            bool confirmed;
            if (kit == 4)
            {
                // 0x293670
                confirmed = communication->confirmReceivedMessage(static_cast<AIRadioMessageType>(4), requester);
                Bot* const requesterBot = IBotManager::instance->getBotFromPlayerId(requester);
                if (requesterBot)
                    static_cast<BotMain*>(requesterBot)->setIsEngineerTarget(true);
            }
            else
            {
                confirmed = communication->confirmReceivedMessage(static_cast<AIRadioMessageType>(requestMessageType(kit)),
                                                                  requester);
            }
            if (confirmed)
                serial->addStatement(checkPointer(new BAPASendRadioMessage(static_cast<AIRadioMessageType>(6),
                                                                           static_cast<AIRadioChannelType>(6), 2, 1.0f,
                                                                           m_id, -1)));
        }

        // 0x292558
        BAPFlowCIf* approach;
        if (targetIsSoldier || toEntry)
        {
            approach = checkPointer(new BAPFlowCIf(inReach, treat, 0, m_id, true, false));
        }
        else
        {
            // 0x292D47: a vehicle reachable by pathfinding: half a second of closing in first.
            BAPCombinerSerial* const sequence = checkPointer(new BAPCombinerSerial(m_id, 1));
            BAPAMoveToObjectFinding* const close = checkPointer(new BAPAMoveToObjectFinding(
                bot, m_target[bot->getBotId()], 0.0f, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0,
                controlInfo->getDriveControls(), m_id, 0, 0.0f, -1));
            BAPConTime* const time = checkPointer(new BAPConTime(0.5, false, false, false));
            sequence->addStatement(checkPointer(new BAPFlowCWhile(checkPointer(new BAPConNot(time)), close, false, m_id)));
            sequence->addStatement(treat);
            approach = checkPointer(new BAPFlowCIf(inReach, sequence, 0, m_id, true, false));
        }
        parallel->addStatement(approach);
        parallel->addStatement(checkPointer(new BAPAMergeSoldierInputs(m_id)));
        BAPFlowCWhile* const loop = checkPointer(new BAPFlowCWhile(treating, parallel, false, m_id));
        serial->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_id)));
        serial->addStatement(loop);
        targetVacant = false;
        if (targetUnit)
            targetVacant = !targetUnit->isOccupied();
    }

    // 0x292732: the squad request this bot now answers.
    if (bot->getPlayer()->getSpawnKit() == 5)
    {
        const unsigned int handle = target->getHandle();
        bot->getSquadMember()->setCurrentRequestAction(1, handle);
    }
    else if (bot->getPlayer()->getSpawnKit() == 4)
    {
        bot->setRepairTarget(m_target[bot->getBotId()]);
        const unsigned int handle = target->getHandle();
        bot->getSquadMember()->setCurrentRequestAction(2, handle);
    }
    else if (bot->getPlayer()->getSpawnKit() == 3)
    {
        const unsigned int handle = target->getHandle();
        bot->getSquadMember()->setCurrentRequestAction(4, handle);
    }

    // 0x29278F
    BAPIWFire* const root = checkPointer(
        new BAPIWFire(bot, m_target[bot->getBotId()], targetVacant, false, weaponIndex, -1, serial, m_id));
    root->setParent(this);   // +0x04 (BotScript::mpOwner)
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return root;
}

// ELF 0x291320: m_initiated (+0x04) [botId] = true, then the three per-bot slots, each indexed by
// a fresh Bot::getBotId (+0xE8).
void BBPMedicAssist::init(unsigned int target, int weaponIndex, int targetPlayerId, Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
    m_target[bot->getBotId()] = target;
    m_weaponIndex[bot->getBotId()] = weaponIndex;
    m_targetPlayerId[bot->getBotId()] = targetPlayerId;
}

// ELF 0x291390: instruction for instruction BBMedicAssist::isHumanInTarget (0x216980) - the
// primary object of `info` for `side`, then its secondary objects; true at the first unit occupied
// by a known non-AI player (IAIPlayerFactory +0x20 getPlayerFromId, IAIPlayer +0x88 getIsAIPlayer).
bool BBPMedicAssist::humanPlayerInTarget(Information* info, int side)
{
    bool human = false;
    if (info)
    {
        Information* primary = informationManager->getInformation(info->getPrimaryObject(side));
        if (primary)
        {
            IPIUnit* unit = primary->getUnit();
            if (unit && unit->isOccupied())
            {
                IAIPlayer* player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(unit->getOccupyingPlayerId(side));
                if (player && !player->getIsAIPlayer())
                    return true;
            }
            int iterator;
            for (Information* secondary =
                     informationManager->getInformation(primary->getFirstSecondaryObject(iterator, side));
                 secondary;
                 secondary = informationManager->getInformation(primary->getSecondaryObject(iterator, side)))
            {
                unit = secondary->getUnit();
                if (unit && unit->isOccupied())
                {
                    IAIPlayer* player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(unit->getOccupyingPlayerId(side));
                    if (player && !player->getIsAIPlayer())
                    {
                        human = true;
                        break;
                    }
                }
            }
        }
    }
    return human;
}

} // namespace ai
} // namespace hfe
} // namespace dice
