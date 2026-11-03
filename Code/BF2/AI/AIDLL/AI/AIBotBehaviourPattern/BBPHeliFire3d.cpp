// Fire pattern of helicopters (AI/AIDLL/AI/AIBotBehaviourPattern/BBPHeliFire3d.cpp in the image).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// The builders were written from the listing block by block; Ghidra drops the bool constructor
// arguments and shows createPlanInternal's mode 0/1 arm once for both modes.
// [PLATFORM] precision: the x87 build keeps the radius sum, the bounding-box sums, the fire-rate
// scale and the 60 / shots quotient in extended precision before rounding or converting them; the
// float expressions below round them first.

// static init: ELF 0x28C0E0 (__tcf_0 0x28C140, _GLOBAL__I 0x28C170): <iostream>'s Init object only.
#include <iostream>

#include "BBPHeliFire3d.h"

#include <algorithm>

#include "../../../../Core/Debug.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAAimAtObject3d.h"
#include "../AIBotActionPlan/BAPAMoveAwayFrom3dObject.h"
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
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPConWeaponHeat.h"
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
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "WeaponChoice.h"

#include <string>

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

// [CHOSEN, not recovered] spelling of the image's inlined row copy (three loads at 4 * row floats).
Vec3 row(const Mat4& transformation, int index)
{
    const Vec3 result = { transformation.m[index * 4], transformation.m[index * 4 + 1],
                          transformation.m[index * 4 + 2] };
    return result;
}

void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

} // namespace

// ELF 0x287500: BBPFire(), the vtable store (GOT -0x1308 + 8), then the argument at +0x08
// (operator new 0x28: nothing beyond BBPFire).
BBPHeliFire3d::BBPHeliFire3d(int patternId)
    : BBPFire()
{
    m_patternId = patternId;
}

// ELF 0x287580 (D0 0x2875C0): the vtable store, then BBPFire's destructor; nothing of its own.
BBPHeliFire3d::~BBPHeliFire3d()
{
}

// ELF 0x287830. Keeps a fire plan of this pattern built for the same target and flag; otherwise
// drops the plan, chooses the strongest weapon with ammunition against the target's strength type
// regardless of range (7: every weapon class), and builds `IWFire(createPlanInternal)`. The mode
// comes from the own vehicle and the target, the precision from the target's bounding box (mode 0:
// 0.8 of the largest extent, at least 0.5; mode 1: the extent sum, at least 5; mode 2: the mean
// extent, at least 10). Without a usable target no plan is built; with no weapon the own object's
// update is called first, as compiled.
BotScript* BBPHeliFire3d::createPlan(Bot* bot)
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
        && m_flag20 == static_cast<BAPIWFire*>(bot->getPlan())->getFlagA())
        return bot->getPlan();

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    dropPlan(bot);
    bot->getScriptContext()->clear();

    const unsigned int target = m_targets[bot->getBotId()];
    Information* const targetInfo = informationManager->getInformation(target);
    if (!targetInfo)
        return 0;
    IPIArmament* const armament = self->getArmament();
    IPIPhysical* const selfPhysical = self->getPhysical();
    IPIControlInfo* const controlInfo = self->getControlInfo();
    IPIPhysical* const targetPhysical = targetInfo->getPhysical();
    if (!armament || !targetPhysical || !controlInfo || !selfPhysical)
        return 0;
    IPIMobile* const targetMobile = targetInfo->getMobile();
    LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(targetPhysical->getLocalBoundingBox());
    FireData data;
    data.m_weapon = choseWeaponDistanceIndependent(m_weapons[bot->getBotId()], targetPhysical->getStrengthType(),
                                                   armament->getWeapons(), 7);
    if (data.m_weapon == -1)
    {
        self->getAIObject()->update();
        return 0;
    }

    const float selfRadius = selfPhysical->getRadius();
    const float radius = targetMobile ? targetPhysical->getRadius() + selfRadius : selfRadius + 0.0f;
    data.m_approachRadius = radius + radius;
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(data.m_weapon));
    const Vec3 weaponPosition = row(weapon->getAbsoluteTransformation(), 3);   // stored, never read
    (void)weaponPosition;
    data.m_weaponDirection = row(weapon->getAbsoluteTransformation(), 2);
    data.m_position = row(self->getTransformation(), 3);
    data.m_direction = row(self->getTransformation(), 2);
    const bool indirect = weapon->getIndirect();
    const bool flag40 = (targetInfo->getObjectTypeFlags() & 0x40) != 0;
    bot->getSquadMember()->clearRequestAction();
    if (!self->getMobile())
    {
        data.m_mode = 3;
    }
    else
    {
        float limit;
        float precision;
        if (!flag40)
        {
            limit = 0.5f;
            data.m_mode = 0;
            precision = std::max(box->getSize().x, std::max(box->getSize().y, box->getSize().z)) * 0.8f;
        }
        else if (!indirect)
        {
            limit = 5.0f;
            data.m_mode = 1;
            precision = box->getSize().x + box->getSize().y + box->getSize().z;
        }
        else
        {
            limit = 10.0f;
            data.m_mode = 2;
            precision = (box->getSize().x + box->getSize().y + box->getSize().z) / 3.0f;
        }
        data.m_precision = std::max(limit, precision);
    }

    BAPIWFire* const plan = checkPointer(new BAPIWFire(bot, target, m_flag20, false, data.m_weapon, -1,
                                                       createPlanInternal(bot, data), m_patternId));
    plan->setParent(this);   // +0x04 (BotScript::m_owner)
    return plan;
}

// ELF 0x287600
void BBPHeliFire3d::init(Bot* bot, unsigned int target, int weapon, bool flag20, bool flag21)
{
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = target;
    m_weapons[bot->getBotId()] = weapon;
    m_flag20 = flag20;
    m_flag21 = flag21;
    m_positionArgs[bot->getBotId()] = static_cast<unsigned int>(-1);
}

// ELF 0x287680. Helicopters have no laser-guided attack: the image reports it (an Error-type
// Debug, which aborts in this tree) and then stores the arguments like BBPFire3d's overload.
void BBPHeliFire3d::init(Bot* bot, unsigned int positionArg, const Vec3& position, int weapon)
{
    Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviourPattern/BBPHeliFire3d.cpp", 0x7D, "")
        << "LaserGuided missiles unsuported by helicopters.";
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_positionArgs[bot->getBotId()] = positionArg;
    m_targetPositions[bot->getBotId()] = position;
}

// ELF 0x28C190
int BBPHeliFire3d::getImplementationType()
{
    return 4;
}

// ELF 0x288130 (listing; the switch is at 0x2882B4). Modes 0 and 1 loop while the target is
// alive and, for a weapon with heat, not too hot: inside the battle zone (150) the helicopter
// attacks when the target is in the line of fire, in range (20 m to the weapon's range) and in
// front, and otherwise backs away and re-approaches it twice; outside it closes in on the target at
// 25 m. Controls are reset and flares answer missile locks throughout. Mode 3 (no IPIMobile of its
// own) is the plain attack; mode 2 builds nothing, as compiled.
BAPStatement* BBPHeliFire3d::createPlanInternal(Bot* bot, BBPHeliFire3d::FireData& data)
{
    static float dotLimit = 0.5f;    // 0x4BC124
    static float minHeight = 25.0f;  // 0x4BC128

    BAPContext* const context = bot->getScriptContext();
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIControlInfo* const controlInfo = self->getControlInfo();
    IPIArmament* const armament = self->getArmament();
    IPIMobile* const mobile = self->getMobile();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(data.m_weapon));
    if (data.m_weapon != -1)
        armament->setActiveWeapon(data.m_weapon, bot->getSide());

    const unsigned int target = m_targets[bot->getBotId()];
    Information* const targetInfo = informationManager->getInformation(target);   // no null test
    IPIUnit* const targetUnit = targetInfo->getUnit();
    const float worldSizeX = static_cast<float>(terrain()->getWorldSizeX());   // not read
    const float worldSizeZ = static_cast<float>(terrain()->getWorldSizeZ());   // not read
    (void)worldSizeX;
    (void)worldSizeZ;
    if (Bot* const targetBot = IBotManager::instance->getBot(targetInfo->getAIObject()))
        (void)targetBot->getLodLevel();   // called, result not used

    switch (data.m_mode)
    {
    case 0:
    case 1:
    {
        BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        BAPConAnd* const alive = checkPointer(new BAPConAnd());
        alive->addCondition(checkPointer(new BAPConObjectExists(target)));
        if (targetUnit && targetUnit->isOccupied())
            alive->addCondition(new BAPConObjectOccupied(target));   // no checkPointer in the image
        alive->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConObjectHealth(target, 0.0f)))));
        if (weapon->hasHeat())
            alive->addCondition(checkPointer(new BAPConWeaponHeat(newVariable<WeaponInfo*>(context, weapon), 0.8f)));

        BAPConInsideBattleZone* const inside = checkPointer(new BAPConInsideBattleZone(150.0f));
        BAPConAnd* const inRange = checkPointer(new BAPConAnd());
        BAPConObjectLineOfFire* const lineOfFire = checkPointer(new BAPConObjectLineOfFire(target, false));
        BAPConObjectDistance* const distance =
            checkPointer(new BAPConObjectDistance(target, 20.0f, weapon->getMaxRange(), false, true));
        inRange->addCondition(lineOfFire);
        inRange->addCondition(distance);
        inRange->addCondition(checkPointer(new BAPConObjectInFront(target, 0.0f, dotLimit)));

        BAPCombinerParallel* const attackBlock = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        attackBlock->addStatement(createMobileLessAttackPlan(bot, data));

        BAPCombinerSerial* const evade = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        BAPAMoveAwayFrom3dObject* const away1 = checkPointer(new BAPAMoveAwayFrom3dObject(
            target, 150.0f, checkPointer(new BAPConFalse()), controlInfo->getDriveControls(), m_patternId));
        BAPAMoveTo3dObject* const return1 = checkPointer(new BAPAMoveTo3dObject(
            target, data.m_approachRadius, mobile->getMaxSpeed(), mobile->getMaxSpeed(), minHeight,
            checkPointer(new BAPConObjectDistance(target, 30.0f, true, true, false)), controlInfo->getDriveControls(),
            m_patternId));
        BAPAMoveAwayFrom3dObject* const away2 = checkPointer(new BAPAMoveAwayFrom3dObject(
            target, 150.0f, checkPointer(new BAPConFalse()), controlInfo->getDriveControls(), m_patternId));
        BAPAMoveTo3dObject* const return2 = checkPointer(new BAPAMoveTo3dObject(
            target, data.m_approachRadius, mobile->getMaxSpeed(), mobile->getMaxSpeed(), minHeight,
            checkPointer(new BAPConObjectDistance(target, 30.0f, true, true, false)), controlInfo->getDriveControls(),
            m_patternId));
        evade->addStatement(away1);
        evade->addStatement(return1);
        evade->addStatement(away2);
        evade->addStatement(return2);

        BAPFlowCIf* const engage = checkPointer(new BAPFlowCIf(inRange, attackBlock, evade, m_patternId, true, false));
        BAPAMoveTo3dObject* const approach = checkPointer(new BAPAMoveTo3dObject(
            target, data.m_approachRadius, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 25.0f,
            checkPointer(new BAPConObjectDistance(target, 30.0f, true, true, false)), controlInfo->getDriveControls(),
            m_patternId));
        BAPFlowCIf* const zone = checkPointer(new BAPFlowCIf(inside, engage, approach, m_patternId, true, false));

        BAPCombinerParallel* const body = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
        body->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_patternId)));
        body->addStatement(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, m_patternId));   // no checkPointer
        body->addStatement(zone);
        root->addStatement(checkPointer(new BAPFlowCWhile(alive, body, m_patternId)));
        return root;
    }
    case 3:
        return createMobileLessAttackPlan(bot, data);   // 0x2882D0
    default:
        return 0;
    }
}

// ELF 0x28A7B0. The attack itself: serial([press the weapon's activation input unless it is 0x40],
// parallel(aim at the target, trigger)). The trigger fires while the precision holds (modes 0, 1
// and 3 also need the weapon's range; mode 2 uses the bomb precision) and, for a weapon with a fire
// rate, at most at 1.5 times that rate scaled down by the bot skill; burst weapons hold the
// trigger continuously.
BAPStatement* BBPHeliFire3d::createMobileLessAttackPlan(Bot* bot, BBPHeliFire3d::FireData& data)
{
    bot->getScriptContext();   // called, result not used
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIControlInfo* const controlInfo = self->getControlInfo();
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(self->getArmament()->getWeapon(data.m_weapon));
    int shots = static_cast<int>(weapon->getFireRate() * 1.5f);
    const unsigned int target = m_targets[bot->getBotId()];
    BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    const Vec3 offset = { 0.0f, 0.0f, 0.0f };

    if (weapon->getWeaponActivate() != 0x40)
        root->addStatement(checkPointer(new BAPATrigger(
            static_cast<io::PlayerInputMap>(weapon->getWeaponActivate()), checkPointer(new BAPConTrue()),
            checkPointer(new BAPConFalse()), inputMask(weapon->getWeaponActivate()), m_patternId, 0.0, false)));

    BAPCombinerParallel* const fire = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
    BAPAAimAtObject3d* const aim =
        checkPointer(new BAPAAimAtObject3d(bot, target, offset, checkPointer(new BAPConFalse()),
                                           controlInfo->getAimControls(), m_patternId, 40.0f, 0.0f));
    BAPConAnd* const ready = checkPointer(new BAPConAnd());
    BAPCondition* precision = 0;
    if (data.m_mode == 1)
    {
        BAPConObjectDistance* const inRange =
            checkPointer(new BAPConObjectDistance(target, weapon->getMaxRange(), false, false, false));
        precision = checkPointer(new BAPCConPrecision3d(target, aim, weapon->getAllowedDeviation(), false));
        ready->addCondition(inRange);
    }
    else if (data.m_mode == 2)
    {
        precision = checkPointer(new BAPCConBombPrecision3d(aim, data.m_precision, true));
    }
    else if (data.m_mode == 0 || data.m_mode == 3)
    {
        BAPConObjectDistance* const inRange =
            checkPointer(new BAPConObjectDistance(target, weapon->getMaxRange(), false, false, false));
        precision = checkPointer(
            new BAPCConPrecision3d(target, aim, weapon->getAllowedDeviation(), !weapon->getBurst()));
        ready->addCondition(inRange);
    }
    if (shots != 0)
    {
        const float skill = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill();
        shots = static_cast<int>(shots * (1.0f - (1.0f - skill) * 0.8f));
        ready->addCondition(checkPointer(new BAPConTime(60.0f / shots, false, false, true)));
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

} // namespace ai
} // namespace hfe
} // namespace dice
