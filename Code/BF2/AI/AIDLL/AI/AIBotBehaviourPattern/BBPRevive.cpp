// Revive pattern: init keeps the man-down soldier and the revive weapon per bot; createPlan builds
// the revive script. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body
// carries its ELF address.
//
// static init: ELF 0x29B200 (__tcf_0 0x29B260, _GLOBAL__I 0x29B290): the <iostream> guard object only.

#include "BBPRevive.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPAMergeSoldierInputs.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPAOverrideSensingAgent.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPARevive.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConObjectDistance.h"
#include "../AIBotActionPlan/BAPConObjectExists.h"
#include "../AIBotActionPlan/BAPConObjectManDown.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "WeaponChoice.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

#include <cmath>
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x298A60 (C2) / 0x298CF0 (C1): BBPPattern(), +0x08 = the argument, +0x0C and +0x10 =
// checkPointer(new[getMaxNBots]) of 4 bytes each (not initialised).
BBPRevive::BBPRevive(int id)
    : BBPPattern()
{
    m_id = id;
    m_target = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_weapon = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x298F80 (D2) / 0x298FE0 (D1) / 0x299040 (D0): delete[] +0x0C, +0x10 when set, then ~BBPPattern.
BBPRevive::~BBPRevive()
{
    if (m_target)
        delete[] m_target;
    if (m_weapon)
        delete[] m_weapon;
}

namespace {

void dropPlan(Bot* bot)
{
    delete bot->getPlan();
    bot->setPlan(0);
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

// ELF 0x299110 (listing; Ghidra cross-check). Revives the man-down soldier m_target[bot] with the
// shock paddles:
//   serial { reset controls (true, false, true) when the weapon's activate key is 64, else
//              trigger(activate key),
//            while (target exists && man down) parallel { reset controls, override sensing,
//              look at target, if (within 1.6 m) revive else move to the target with pathfinding
//              (re-planned when within 10 m and the target moved 0.2 m), merge soldier inputs } }
// under BAPIWFire(bot, target, true, false, weapon, -1, ..). The current plan is kept when it is
// this pattern's BAPIWFire on the same target. The medic kit answers the squad request (action 1
// with the target handle). Returns 0 without equipment, unit, mobile, target or a type-3 weapon
// (then the equipment's object is updated).
BotScript* BBPRevive::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        // 0x29B182
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
            dropPlan(bot);
        return 0;
    }

    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_id
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0
        && static_cast<BAPIWFire*>(bot->getPlan())->getTarget() == m_target[bot->getBotId()])
        return bot->getPlan();   // 0x29B11C

    bot->getSquadMember()->clearRequestAction();
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

    const int strengthType = target->getPhysical()->getStrengthType();
    const float range = distance(equipment->getPosition(), target->getPosition());
    const int weaponIndex = choseWeapon(armament->getWeapons(), m_weapon[bot->getBotId()], range, strengthType, false,
                                        false, true, false);
    if (weaponIndex == -1)
    {
        equipment->getAIObject()->update();   // 0x29B08D
        return 0;
    }
    bot->setReviveTarget(m_target[bot->getBotId()]);
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));

    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_id, -1));
    BAPStatement* select;
    if (weapon->getWeaponActivate() == 64)
        select = checkPointer(new BAPAResetControls(true, false, true, m_id));   // 0x29AE90
    else
        select = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(weapon->getWeaponActivate()),
                                              checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
                                              1 << weapon->getWeaponActivate(), m_id, 0.0, false));
    serial->addStatement(select);

    // 0x2995B9: the revive distance, stored as a float local and scaled at run time.
    const float reviveDistance = 0.4f;
    BAPConObjectDistance* const close = checkPointer(
        new BAPConObjectDistance(m_target[bot->getBotId()], reviveDistance * 4.0f, false, true, false));
    BAPConAnd* const replan = checkPointer(new BAPConAnd());
    replan->addCondition(checkPointer(new BAPConObjectDistance(m_target[bot->getBotId()], 10.0f, true, true, false)));
    replan->addCondition(checkPointer(new BAPConObjectMoved(m_target[bot->getBotId()], reviveDistance * 0.5f)));
    BAPAMoveToObjectFinding* const move = checkPointer(new BAPAMoveToObjectFinding(
        bot, m_target[bot->getBotId()], 0.0f, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0,
        controlInfo->getDriveControls(), m_id, replan, 0.0f, -1));
    BAPFlowCIf* const revive = checkPointer(new BAPFlowCIf(close, checkPointer(new BAPARevive(m_id)), move, m_id, true, false));

    BAPConAnd* const down = checkPointer(new BAPConAnd());
    BAPConObjectExists* const exists = checkPointer(new BAPConObjectExists(m_target[bot->getBotId()]));
    BAPConObjectManDown* const manDown = checkPointer(new BAPConObjectManDown(m_target[bot->getBotId()]));
    down->addCondition(exists);
    down->addCondition(manDown);

    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_id, -1, false));
    parallel->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_id)));
    parallel->addStatement(checkPointer(new BAPAOverrideSensingAgent()));
    parallel->addStatement(checkPointer(new BAPALookAtObject(bot, m_target[bot->getBotId()], 0.0f,
                                                             checkPointer(new BAPConFalse()),
                                                             equipment->getControlInfo()->getLookControls(), m_id, 0,
                                                             0.0f)));
    parallel->addStatement(revive);
    parallel->addStatement(checkPointer(new BAPAMergeSoldierInputs(m_id)));
    serial->addStatement(checkPointer(new BAPFlowCWhile(down, parallel, false, m_id)));

    if (bot->getPlayer()->getSpawnKit() == 5)
    {
        // 0x299E4D
        const unsigned int handle = m_target[bot->getBotId()];
        bot->getSquadMember()->setCurrentRequestAction(1, handle);
    }

    BAPIWFire* const root = checkPointer(
        new BAPIWFire(bot, m_target[bot->getBotId()], true, false, weaponIndex, -1, serial, m_id));
    root->setParent(this);   // +0x04 (BotScript::mpOwner)
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return root;
}

// ELF 0x2990B0: m_initiated[id] = true; +0x0C[id] = target; +0x10[id] = weapon.
void BBPRevive::init(unsigned int target, int weapon, Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
    m_target[bot->getBotId()] = target;
    m_weapon[bot->getBotId()] = weapon;
}

} // namespace ai
} // namespace hfe
} // namespace dice
