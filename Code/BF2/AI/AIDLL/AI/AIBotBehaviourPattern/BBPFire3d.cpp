// Fire pattern of aircraft (AI/AIDLL/AI/AIBotBehaviourPattern/BBPFire3d.cpp in the image).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Ghidra pseudocode was checked against the listing for constructor overloads, constant arguments
// and statement order (it drops the bool arguments of the combiner constructors and misreads the
// last turn lock of mode 1).
// [PLATFORM] precision: the x87 build keeps the radius sum and the bounding-box sum in extended
// precision before doubling or dividing; the float expressions below round them first.

// static init: ELF 0x25B970 (__tcf_0 0x25B9D0, _GLOBAL__I 0x25BA00): <iostream>'s Init object only.
#include <iostream>

#include "BBPFire3d.h"

#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAAimAtObject3d.h"
#include "../AIBotActionPlan/BAPAAimAtPoint3d.h"
#include "../AIBotActionPlan/BAPAChangeWeapon.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIBotActionPlan/BAPAMoveTo3dObject.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPAUseFlareOnMissileLock.h"
#include "../AIBotActionPlan/BAPCConBombPrecision3d.h"
#include "../AIBotActionPlan/BAPCConPrecision3d.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConInsideBattleZone.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConObjectDistance.h"
#include "../AIBotActionPlan/BAPConObjectExists.h"
#include "../AIBotActionPlan/BAPConObjectHealth.h"
#include "../AIBotActionPlan/BAPConObjectInFront.h"
#include "../AIBotActionPlan/BAPConObjectLineOfFire.h"
#include "../AIBotActionPlan/BAPConObjectOccupied.h"
#include "../AIBotActionPlan/BAPConPointDistance.h"
#include "../AIBotActionPlan/BAPConPointInFront.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPConWeaponHasLockOnTarget.h"
#include "../AIBotActionPlan/BAPConWeaponHeat.h"
#include "../AIBotActionPlan/BAPConWeaponMagAmmo.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPICTurnLock.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIBotActionPlan/BAPVariable.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The image's inlined plan-variable construction: `checkPointer(new BAPVariable<T>(context, value))`
// followed by `std::list<BotScriptVariable*>::insert(context->begin(), &variable)`.
template <class T>
BAPVariable<T>* newVariable(BAPContext* context, const T& value)
{
    BAPVariable<T>* const variable = checkPointer(new BAPVariable<T>(context, value));
    context->addVariable(variable);
    return variable;
}

// `1 << input` sign-extended to the 64-bit control mask (`movl %eax,%edx; sarl $0x1f,%edx`).
unsigned long long inputMask(int input)
{
    return static_cast<unsigned long long>(static_cast<long long>(1 << input));
}

IAITerrain* terrain()
{
    return static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
}

void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

// "The target is still worth attacking": And{ Exists(target), [Occupied(target)], Not(Health(target, 0)),
// [WeaponHeat(weapon) < heat] }, the loop condition of createPlanInternal's three arms.
BAPConAnd* newTargetAlive(BAPContext* context, unsigned int target, IPIUnit* targetUnit, WeaponInfo* weapon,
                          float heat)
{
    BAPConAnd* const alive = checkPointer(new BAPConAnd());
    alive->addCondition(checkPointer(new BAPConObjectExists(target)));
    if (targetUnit && targetUnit->isOccupied())
        alive->addCondition(new BAPConObjectOccupied(target));   // no checkPointer in the image
    alive->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConObjectHealth(target, 0.0f)))));
    if (weapon->hasHeat())
        alive->addCondition(checkPointer(new BAPConWeaponHeat(newVariable<WeaponInfo*>(context, weapon), heat)));
    return alive;
}

} // namespace

// ELF 0x2512C0: BBPFire(), the vtable store, then the argument at +0x08.
BBPFire3d::BBPFire3d(int patternId)
    : BBPFire()
{
    m_patternId = patternId;
}

// ELF 0x251340 (D0 0x251380): the vtable store, then BBPFire's destructor; nothing of its own.
BBPFire3d::~BBPFire3d()
{
}

// ELF 0x2514E0. Keeps a fire plan of this pattern built for the same target, flag and weapon;
// otherwise drops the plan and builds `IWFire(serial([change weapon], parallel(reset controls,
// flares on missile lock, attack)))`. The attack comes from createPlanLaserGuided when the target
// is no object but a designated point, otherwise from createPlanInternal with the FireData mode
// chosen from the own vehicle and the target. Without a usable target, or with no weapon chosen,
// no plan is built (with no weapon the own object's update is called first, as compiled).
BotScript* BBPFire3d::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }

    static_cast<BotMain*>(bot)->resetAllControls(false, true, true);
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->hasPlan() && static_cast<BAPInfoWrapper*>(bot->getPlan())->getGeneratorId() == m_patternId
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0
        && static_cast<BAPIWFire*>(bot->getPlan())->getTarget() == m_targets[bot->getBotId()]
        && m_flag20 == static_cast<BAPIWFire*>(bot->getPlan())->getFlagA()
        && static_cast<BAPIWFire*>(bot->getPlan())->getWeapon() == m_weapons[bot->getBotId()])
        return bot->getPlan();

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    dropPlan(bot);
    bot->getScriptContext()->clear();

    const unsigned int target = m_targets[bot->getBotId()];
    Information* const targetInfo = informationManager->getInformation(target);
    FireData data;
    data.m_position.x = data.m_position.y = data.m_position.z = 0.0f;
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(self->getArmament()->getWeapon(m_weapons[bot->getBotId()]));

    BAPStatement* attack;
    if (!targetInfo)
    {
        if (m_positionArgs[bot->getBotId()] == static_cast<unsigned int>(-1))
            return 0;
        attack = createPlanLaserGuided(bot, data);
    }
    else
    {
        IPIPhysical* const targetPhysical = targetInfo->getPhysical();
        IPIControlInfo* const controlInfo = self->getControlInfo();
        IPIPhysical* const selfPhysical = self->getPhysical();
        if (!targetPhysical || !controlInfo || !selfPhysical)
            return 0;
        IPIMobile* const targetMobile = targetInfo->getMobile();
        LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(targetPhysical->getLocalBoundingBox());
        if (m_weapons[bot->getBotId()] == -1)
        {
            self->getAIObject()->update();
            return 0;
        }
        bot->getSquadMember()->clearRequestAction();
        const float selfRadius = selfPhysical->getRadius();
        const float radius = targetMobile ? targetPhysical->getRadius() + selfRadius : selfRadius + 0.0f;
        data.m_approachRadius = radius + radius;
        const bool indirect = weapon->getIndirect();
        const bool flag40 = targetInfo->isOfType(ITGround);
        if (!self->getMobile())
        {
            data.m_mode = 3;
        }
        else if (!flag40)
        {
            data.m_mode = 0;
            data.m_precision = weapon->getAllowedDeviation();
        }
        else if (!indirect)
        {
            data.m_mode = 1;
            data.m_precision = weapon->getAllowedDeviation();
        }
        else
        {
            data.m_mode = 2;
            const float size = (box->getSize().x + box->getSize().y + box->getSize().z) / 3.0f;
            data.m_precision = size > 10.0f ? size : 10.0f;     // 0x251FD4 fcmovbe: NaN gives 10
        }
        attack = createPlanInternal(bot, data);
    }

    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
    parallel->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_patternId)));
    parallel->addStatement(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, m_patternId));   // no checkPointer
    parallel->addStatement(attack);
    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_patternId, true, -1));
    if (m_weapons[bot->getBotId()] != -1 && weapon->getWeaponActivate() != 0x40)
        serial->addStatement(checkPointer(new BAPAChangeWeapon(bot, m_weapons[bot->getBotId()], m_patternId)));
    serial->addStatement(parallel);
    BAPIWFire* const plan = checkPointer(
        new BAPIWFire(bot, target, m_flag20, false, m_weapons[bot->getBotId()], -1, serial, m_patternId));
    plan->setParent(this);   // +0x04 (BotScript::mpOwner)
    SquadManager::m_instance->shotsFired(bot->getPlayerId(), bot->getSide(), m_targets[bot->getBotId()]);
    return plan;
}

// ELF 0x2513C0
void BBPFire3d::init(Bot* bot, unsigned int target, int weapon, bool flag20, bool flag21)
{
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = target;
    m_positionArgs[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_flag20 = flag20;
    m_flag21 = flag21;
}

// ELF 0x251440
void BBPFire3d::init(Bot* bot, unsigned int positionArg, const Vec3& position, int weapon)
{
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_positionArgs[bot->getBotId()] = positionArg;
    m_targetPositions[bot->getBotId()] = position;
}

// ELF 0x25BA20
int BBPFire3d::getImplementationType()
{
    return 4;
}

// ELF 0x252260 (listing; the switch is `cmpl $1/2/3` at 0x252447). Modes 1 and 2 (the target has
// type flag 0x40) and mode 0 loop while the target is alive and, for a weapon with heat, not too
// hot: inside the battle zone (150) the bot attacks when the target is in range (mode 0: and in the
// line of fire; modes 1/2: and in front) and otherwise approaches it; outside it flies back to the
// map centre at 200 m. Modes 1 and 2 wrap the loop in a serial with a trailing turn unlock and lock
// turning during the attack. Mode 3 (no IPIMobile of its own) is the plain attack.
BAPStatement* BBPFire3d::createPlanInternal(Bot* bot, BBPFire3d::FireData& data)
{
    BAPContext* const context = bot->getScriptContext();
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIControlInfo* const controlInfo = self->getControlInfo();
    IPIArmament* const armament = self->getArmament();
    IPIMobile* const mobile = self->getMobile();
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(m_weapons[bot->getBotId()]));
    if (m_weapons[bot->getBotId()] != -1)
        armament->setActiveWeapon(m_weapons[bot->getBotId()], bot->getSide());

    const unsigned int target = m_targets[bot->getBotId()];
    Information* const targetInfo = informationManager->getInformation(target);   // no null test
    IPIUnit* const targetUnit = targetInfo->getUnit();
    const float worldSizeX = static_cast<float>(terrain()->getWorldSizeX());
    const float worldSizeZ = static_cast<float>(terrain()->getWorldSizeZ());
    bool targetIsLodBot = false;
    if (Bot* const targetBot = IBotManager::instance->getBot(targetInfo->getAIObject()))
        targetIsLodBot = targetBot->getLodLevel() > 0;

    switch (data.m_mode)
    {
    case 1:
    case 2:
    {
        // 0x255A14 (mode 1) and 0x252490 (mode 2) differ only in the order of the range
        // conditions and in the approach (200 / 350) and return (200 / 300) distances.
        const bool bomb = data.m_mode == 2;
        BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        BAPConAnd* const alive = newTargetAlive(context, target, targetUnit, weapon, 0.8f);
        BAPConInsideBattleZone* const inside = checkPointer(new BAPConInsideBattleZone(150.0f));
        BAPConAnd* const inRange = checkPointer(new BAPConAnd());
        BAPConObjectInFront* const inFront = checkPointer(new BAPConObjectInFront(target, 10.0f, 0.0f));
        BAPConObjectDistance* const distance =
            checkPointer(new BAPConObjectDistance(target, 0.9f, weapon->getMaxRange(), false, false));
        if (bomb)
        {
            inRange->addCondition(distance);
            inRange->addCondition(inFront);
        }
        else
        {
            inRange->addCondition(inFront);
            inRange->addCondition(distance);
        }
        BAPCombinerParallel* const attackBlock = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        BAPICTurnLock* const lock = checkPointer(new BAPICTurnLock(true));
        BAPStatement* const attack = createMobileLessAttackPlan(bot, data);
        attackBlock->addStatement(lock);
        attackBlock->addStatement(attack);
        BAPCombinerParallel* const approachBlock = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        BAPICTurnLock* const unlock = checkPointer(new BAPICTurnLock(false));
        BAPCombinerSerial* const approach = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        approach->addStatement(checkPointer(new BAPAMoveTo3dObject(
            target, data.m_approachRadius, mobile->getMaxSpeed(), mobile->getMaxSpeed(), bomb ? 350.0f : 200.0f,
            checkPointer(new BAPConFalse()), controlInfo->getDriveControls(), m_patternId)));
        approachBlock->addStatement(unlock);
        approachBlock->addStatement(approach);
        BAPFlowCIf* const engage =
            checkPointer(new BAPFlowCIf(inRange, attackBlock, approachBlock, m_patternId, true, false));
        const Vec3 centre = { worldSizeX * 0.5f, 200.0f, worldSizeZ * 0.5f };
        BAPAMoveTo3d* const back = checkPointer(new BAPAMoveTo3d(
            bot, centre, mobile->getMaxSpeed(), mobile->getMaxSpeed(), checkPointer(new BAPConInsideBattleZone(200.0f)),
            controlInfo->getDriveControls(), bomb ? 300.0f : 200.0f, m_patternId));
        BAPFlowCIf* const zone = checkPointer(new BAPFlowCIf(inside, engage, back, m_patternId, true, false));
        BAPFlowCWhile* const loop = checkPointer(new BAPFlowCWhile(alive, zone, m_patternId));
        BAPICTurnLock* const release = checkPointer(new BAPICTurnLock(false));
        root->addStatement(loop);
        root->addStatement(release);
        return root;
    }
    case 0:
    {
        // 0x254214
        BAPConAnd* const alive = newTargetAlive(context, target, targetUnit, weapon, 0.5f);
        BAPConInsideBattleZone* const inside = checkPointer(new BAPConInsideBattleZone(150.0f));
        BAPConAnd* const inRange = checkPointer(new BAPConAnd());
        inRange->addCondition(checkPointer(new BAPConObjectDistance(target, 0.9f, weapon->getMaxRange(), false, false)));
        inRange->addCondition(checkPointer(new BAPConObjectLineOfFire(target, targetIsLodBot)));
        BAPStatement* const attack = createMobileLessAttackPlan(bot, data);
        BAPConAnd* const stopApproach = checkPointer(new BAPConAnd());
        BAPConObjectDistance* const distance =
            checkPointer(new BAPConObjectDistance(target, 0.8f, weapon->getMaxRange(), false, false));
        BAPConObjectLineOfFire* const lineOfFire = checkPointer(new BAPConObjectLineOfFire(target, targetIsLodBot));
        stopApproach->addCondition(distance);
        stopApproach->addCondition(lineOfFire);
        BAPAMoveTo3dObject* const approach = checkPointer(new BAPAMoveTo3dObject(
            target, data.m_approachRadius, mobile->getMaxSpeed(), mobile->getMaxSpeed() * 0.5f, 50.0f, stopApproach,
            controlInfo->getDriveControls(), m_patternId));
        BAPFlowCIf* const engage = checkPointer(new BAPFlowCIf(inRange, attack, approach, m_patternId, false, false));
        const Vec3 centre = { worldSizeX * 0.5f, 200.0f, worldSizeZ * 0.5f };
        BAPAMoveTo3d* const back = checkPointer(new BAPAMoveTo3d(
            bot, centre, mobile->getMaxSpeed(), mobile->getMaxSpeed(), checkPointer(new BAPConInsideBattleZone(200.0f)),
            controlInfo->getDriveControls(), 200.0f, m_patternId));
        BAPFlowCIf* const zone = checkPointer(new BAPFlowCIf(inside, engage, back, m_patternId, true, false));
        return checkPointer(new BAPFlowCWhile(alive, zone, m_patternId));
    }
    case 3:
        return createMobileLessAttackPlan(bot, data);   // 0x25246A
    default:
        return 0;
    }
}

// ELF 0x2577D0. The attack itself: serial(arm the weapon, [reload when the magazine is empty],
// parallel(aim, trigger)). Arming is a reset of the controls, plus pressing the weapon's activation
// input unless it is 0x40. The aim is at the target object, or at FireData::m_position when there
// is no target. The trigger fires while the aim precision holds (modes 0, 1 and 3 also need the
// weapon's range; mode 3 a missile lock for guidance type 1; mode 2 uses the bomb precision);
// burst weapons hold the trigger continuously.
BAPStatement* BBPFire3d::createMobileLessAttackPlan(Bot* bot, BBPFire3d::FireData& data)
{
    BAPContext* const context = bot->getScriptContext();
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = self->getArmament();
    IPIControlInfo* const controlInfo = self->getControlInfo();
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(m_weapons[bot->getBotId()]));
    const unsigned int target = m_targets[bot->getBotId()];
    BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, -1));

    BAPStatement* arm;
    if (weapon->getWeaponActivate() == 0x40)
    {
        arm = checkPointer(new BAPAResetControls(false, true, true, m_patternId));
    }
    else
    {
        BAPCombinerParallel* const block = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        BAPAResetControls* const reset = checkPointer(new BAPAResetControls(false, true, true, m_patternId));
        BAPATrigger* const activate = checkPointer(new BAPATrigger(
            static_cast<io::PlayerInputMap>(weapon->getWeaponActivate()), checkPointer(new BAPConTrue()),
            checkPointer(new BAPConFalse()), inputMask(weapon->getWeaponActivate()), m_patternId, 0.0, false));
        block->addStatement(reset);
        block->addStatement(activate);
        arm = block;
    }
    root->addStatement(arm);

    if (!weapon->hasAutoReload())
    {
        BAPVariable<int>* const ammo = newVariable<int>(context, 0);
        BAPVariable<WeaponInfo*>* const weaponVariable = newVariable<WeaponInfo*>(context, weapon);
        BAPConWeaponMagAmmo* const empty = checkPointer(new BAPConWeaponMagAmmo(weaponVariable, ammo, false));
        // Input 0x20 with the mask constant 0x100000000 (bit 32).
        BAPATrigger* const reload = checkPointer(new BAPATrigger(
            static_cast<io::PlayerInputMap>(0x20), checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
            0x100000000ULL, m_patternId, 0.0, false));
        root->addStatement(checkPointer(new BAPFlowCIf(empty, reload, 0, m_patternId, false, false)));
    }

    BAPCombinerParallel* const fire = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
    BAPAAimAt3d* aim;
    if (target == static_cast<unsigned int>(-1))
    {
        Vec3 offset = { 0.0f, 0.0f, 0.0f };
        aim = checkPointer(new BAPAAimAtPoint3d(data.m_position, offset, checkPointer(new BAPConFalse()),
                                                controlInfo->getAimControls(), m_patternId, 50.0f, 350.0f));
    }
    else
    {
        const Vec3 offset = { 0.0f, 0.0f, 0.0f };
        aim = checkPointer(new BAPAAimAtObject3d(bot, target, offset, checkPointer(new BAPConFalse()),
                                                 controlInfo->getAimControls(), m_patternId,
                                                 data.m_mode != 1 ? 40.0f : 50.0f, 350.0f));
    }

    BAPConAnd* const ready = checkPointer(new BAPConAnd());
    BAPCondition* precision = 0;
    if (data.m_mode == 1)
    {
        ready->addCondition(checkPointer(new BAPConObjectDistance(target, weapon->getMaxRange(), false, false, false)));
        precision = checkPointer(new BAPCConPrecision3d(target, aim, data.m_precision, !weapon->getBurst()));
    }
    else if (data.m_mode == 2)
    {
        precision = checkPointer(new BAPCConBombPrecision3d(aim, data.m_precision, true));
    }
    else if (data.m_mode == 0 || data.m_mode == 3)
    {
        ready->addCondition(checkPointer(new BAPConObjectDistance(target, weapon->getMaxRange(), false, false, false)));
        if (weapon->getWeaponGuidanceType() == 1)
            precision = checkPointer(new BAPConWeaponHasLockOnTarget(newVariable<WeaponInfo*>(context, weapon), target));
        else
            precision = checkPointer(new BAPCConPrecision3d(target, aim, data.m_precision, !weapon->getBurst()));
    }
    ready->addCondition(precision);   // a null condition for any other mode, as compiled

    BAPStatement* trigger;
    if (!weapon->getBurst())
        trigger = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(weapon->getWeaponFire()), ready,
                                               checkPointer(new BAPConFalse()), inputMask(weapon->getWeaponFire()),
                                               m_patternId, 0.0, true));
    else
        trigger = checkPointer(new BAPATriggerContinously(static_cast<io::PlayerInputMap>(weapon->getWeaponFire()),
                                                          ready, 0, checkPointer(new BAPConFalse()),
                                                          inputMask(weapon->getWeaponFire()), m_patternId, 0.0));
    fire->addStatement(aim);
    fire->addStatement(trigger);
    root->addStatement(fire);
    return root;
}

// ELF 0x25A260. Attack on a designated point (init's Vec3 overload): mode 2 with radius 20 and
// precision 40 at the stored position; the weapon is told its target (the handle argument).
// Inside the battle zone (150) the bot attacks when the point is in range and in front, otherwise
// flies to it at 50 m; outside it returns to the map centre at 300 m.
BAPStatement* BBPFire3d::createPlanLaserGuided(Bot* bot, BBPFire3d::FireData& data)
{
    data.m_mode = 2;
    data.m_approachRadius = 20.0f;
    data.m_precision = 40.0f;
    data.m_position = m_targetPositions[bot->getBotId()];
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = self->getMobile();
    IPIArmament* const armament = self->getArmament();
    IPIControlInfo* const controlInfo = self->getControlInfo();
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(m_weapons[bot->getBotId()]));
    weapon->setTarget(m_positionArgs[bot->getBotId()]);

    BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    BAPConInsideBattleZone* const inside = checkPointer(new BAPConInsideBattleZone(150.0f));
    BAPConAnd* const inRange = checkPointer(new BAPConAnd());
    BAPConPointInFront* const inFront = checkPointer(new BAPConPointInFront(data.m_position, 10.0f, 0.0f));
    BAPConPointDistance* const distance =
        checkPointer(new BAPConPointDistance(data.m_position, 0.9f, weapon->getMaxRange(), false));
    inRange->addCondition(distance);
    inRange->addCondition(inFront);
    BAPCombinerParallel* const attackBlock = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
    BAPICTurnLock* const lock = checkPointer(new BAPICTurnLock(true));
    BAPStatement* const attack = createMobileLessAttackPlan(bot, data);
    attackBlock->addStatement(lock);
    attackBlock->addStatement(attack);
    BAPCombinerParallel* const approachBlock = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
    BAPICTurnLock* const unlock = checkPointer(new BAPICTurnLock(false));
    BAPCombinerSerial* const approach = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    approach->addStatement(checkPointer(new BAPAMoveTo3d(bot, data.m_position, mobile->getMaxSpeed(),
                                                         mobile->getMaxSpeed(), checkPointer(new BAPConFalse()),
                                                         controlInfo->getDriveControls(), 50.0f, m_patternId)));
    approachBlock->addStatement(unlock);
    approachBlock->addStatement(approach);
    BAPFlowCIf* const engage =
        checkPointer(new BAPFlowCIf(inRange, attackBlock, approachBlock, m_patternId, true, false));
    const float worldSizeX = static_cast<float>(terrain()->getWorldSizeX());
    const float worldSizeZ = static_cast<float>(terrain()->getWorldSizeZ());
    const Vec3 centre = { worldSizeX * 0.5f, 200.0f, worldSizeZ * 0.5f };
    BAPAMoveTo3d* const back = checkPointer(new BAPAMoveTo3d(
        bot, centre, mobile->getMaxSpeed(), mobile->getMaxSpeed(), checkPointer(new BAPConInsideBattleZone(200.0f)),
        controlInfo->getDriveControls(), 300.0f, m_patternId));
    BAPFlowCIf* const zone = checkPointer(new BAPFlowCIf(inside, engage, back, m_patternId, true, false));
    BAPICTurnLock* const release = checkPointer(new BAPICTurnLock(false));
    root->addStatement(zone);
    root->addStatement(release);
    return root;
}

} // namespace ai
} // namespace hfe
} // namespace dice
