// Ground-vehicle firing plans, including the shipped pointer test and inverted heat branch.

// static init: ELF 0x24B620 (__tcf_0 0x24B680, _GLOBAL__I 0x24B6B0): <iostream>'s Init object only.
#include <iostream>

#include "BBPFire2d.h"

#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotSenses.h"
#include "../AIBot/SensedObject.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAAimAtObject.h"
#include "../AIBotActionPlan/BAPAChangeWeapon.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPANotifyBot.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCConPrecision.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConObjectDistance.h"
#include "../AIBotActionPlan/BAPConObjectHealth.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPConOpLessThan.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPConWeaponHeat.h"
#include "../AIBotActionPlan/BAPConWeaponMagAmmo.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIBotActionPlan/BAPWrapperWeapon.h"
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
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Core/Debug.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "WeaponChoice.h"

#include <cmath>
#include <list>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// [CHOSEN, not recovered] Local factoring of the image's inline variable, cleanup,
// distance, input-mask and repeated reload-trigger operations; these helper names have no symbols.

// Inline construction and context-list insertion in ELF 0x245B58 and subsequent allocations.
template <class T>
BAPVariable<T>* newVariable(BAPContext* context, const T& value)
{
    BAPVariable<T>* const variable = checkPointer(new BAPVariable<T>(context, value));
    context->addVariable(variable);
    return variable;
}

void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

float distanceBetween(const Vec3& a, const Vec3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}

// ELF 0x246B53 / 0x24769E: 32-bit shl, then sign extension to the 64-bit mask.
unsigned long long inputMask(int input)
{
    const int mask = static_cast<int>(1u << (static_cast<unsigned int>(input) & 31u));
    return static_cast<unsigned long long>(static_cast<long long>(mask));
}

// The identical reload triggers at ELF 0x245F18 and 0x248FC8.
BAPATrigger* reloadTrigger(int patternId)
{
    return checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(32),
        checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
        0x100000000ULL, patternId, 0.0, false));
}

} // namespace

// ELF 0x245140: BBPFire(), the vtable store (GOT -0x178 + 8), the argument at +0x08, then
// `new bool[getMaxNBots()]` (IAISettings slot 0, not initialised) through the inlined checkPointer
// into +0x28 (operator new 0x2C).
BBPFire2d::BBPFire2d(int patternId)
    : BBPFire()
{
    m_patternId = patternId;
    m_perBotFlag = checkPointer(new bool[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x2452B0 (D2) / 0x245300 (D1) / 0x245350 (D0): delete[] +0x28 when set, then ~BBPFire.
BBPFire2d::~BBPFire2d()
{
    if (m_perBotFlag)
        delete[] m_perBotFlag;
}

// ELF 0x245650
BotScript* BBPFire2d::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        dropPlan(bot);
        return 0;
    }
    static_cast<BotMain*>(bot)->resetAllControls(true, false, true);
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const targetInfo = informationManager->getInformation(m_targets[bot->getBotId()]);
    if (!targetInfo)
        return 0;
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    IPIPhysical* const physical = targetInfo->getPhysical();
    IPIArmament* const armament = self->getArmament();
    IPIMobile* const mobile = self->getMobile();
    IPIControlInfo* const controls = self->getControlInfo();
    const unsigned long long driveControls = controls->getDriveControls();
    const unsigned long long aimControls = controls->getAimControls();
    const bool separateControls = (driveControls & aimControls) == 0;
    SensedObject* const sensed = bot->getSensedObjectFromHandle(m_targets[bot->getBotId()]);
    LocalBoundingBox* const bounds = const_cast<LocalBoundingBox*>(physical->getLocalBoundingBox());
    const int strength = physical->getStrengthType();
    const Vec3& selfPosition = self->getPosition();
    const Vec3& targetPosition = targetInfo->getPosition();
    const float distance = distanceBetween(selfPosition, targetPosition);
    const unsigned int botId = bot->getBotId();
    const std::vector<unsigned int>& weapons = armament->getWeapons();
    const int weapon = choseWeapon(weapons, m_weapons[botId], distance, strength, false, false, false, false);
    if (weapon == -1)
    {
        self->getAIObject()->update();
        return 0;
    }
    WeaponInfo* const weaponInfo = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weapon));
    armament->setActiveWeapon(weapon, bot->getSide());
    // Three inline size getters are evaluated even though their values are discarded.
    (void)bounds->getSize().x;
    (void)bounds->getSize().y;
    (void)bounds->getSize().z;
    const Vec3& currentSelfPosition = self->getPosition();
    const Vec3& currentTargetPosition = targetInfo->getPosition();
    const float targetDistance = distanceBetween(currentSelfPosition, currentTargetPosition);
    const float maxRange = weaponInfo->getMaxRange();
    int lineOfFire;
    if (targetDistance > static_cast<double>(maxRange) * 0.95)
        lineOfFire = mobile ? 0 : 3;
    else if (sensed && !(sensed->m_flags & SensedObject::SENSE_INDIRECT))
        lineOfFire = 1;
    else
    {
        Vec3 aimPoint = {0.0f, 0.0f, 0.0f};
        if (sensed)
        {
            const Mat4& m = targetInfo->getTransformation();
            const Vec3& p = sensed->m_seenPosition;
            aimPoint.x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
            aimPoint.y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
            aimPoint.z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
        }
        else
            aimPoint = targetInfo->getPosition();
        std::vector<IAIObject*> excluded;
        excluded.push_back(self->getAIObject());
        excluded.push_back(targetInfo->getAIObject());
        lineOfFire = environment->hasIndirectLineOfFire(weaponInfo->getWeapon(), weaponInfo->getPosition(), aimPoint, excluded)
            ? 2 : (mobile ? 0 : 3);
    }
    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId
        && static_cast<BAPStatement*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0)
    {
        BAPIWFire* const existing = static_cast<BAPIWFire*>(bot->getPlan());
        if (existing->getTarget() == m_targets[bot->getBotId()] && existing->getInt28() == lineOfFire)
            return bot->getPlan();
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    dropPlan(bot);
    bot->getSquadMember()->clearRequestAction();
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    BAPVariable<WeaponInfo*>* const weaponVariable = newVariable(context, weaponInfo);
    BAPVariable<unsigned int>* const targetVariable = newVariable(context, m_targets[bot->getBotId()]);
    BAPStatement* statement = 0;
    if (lineOfFire == 3)
    {
        SensedObject* blocked = bot->getSensedObjectFromHandle(m_targets[bot->getBotId()]);
        if (!blocked)
        {
            const unsigned int primary = targetInfo->getPrimaryObject(targetInfo->getSide());
            Information* const primaryInfo = informationManager->getInformation(primary);
            if (primaryInfo)
            {
                const Vec3 zero = {0.0f, 0.0f, 0.0f};
                bot->getSensingAgent()->getSenses()->event_seenObject(primary, zero, static_cast<unsigned int>(-1),
                                                                   primaryInfo, false, false);
                blocked = bot->getSensedObjectFromHandle(primary);
            }
        }
        // shipped defect: no guard if the primary information/sensed record is absent.
        blocked->m_flags |= 0x20;
        blocked->m_fireRating = -100;
        blocked->m_lastSense = 0x20;
        return 0;
    }
    if (lineOfFire == 0)
    {
        IAIPathfinding* const pathfinding = IAIPathfinding::instance;
        BAPCombinerParallel* const approach = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        const int equipment = self->getUnit()->getEquipmentType();
        BAPStatement* movement;
        if (pathfinding->isValidPosition(mobile->getVehicleNumber(), equipment, targetInfo->getPosition(), true))
        {
            BAPConAnd* const close = checkPointer(new BAPConAnd());
            close->addCondition(checkPointer(new BAPConObjectDistance(m_targets[bot->getBotId()], 50.0f, true, true, false)));
            close->addCondition(checkPointer(new BAPConObjectMoved(m_targets[bot->getBotId()], 10.0f)));
            const unsigned long long drive = controls->getDriveControls();
            const float arriveSpeed = mobile->getMaxSpeed();
            const float speed = mobile->getMaxSpeed();
            const float approachDistance = targetDistance * 0.75f;
            const float rangeLimit = weaponInfo->getMaxRange() * 0.95f;
            movement = checkPointer(new BAPAMoveToObjectFinding(bot, m_targets[bot->getBotId()],
                rangeLimit > approachDistance ? approachDistance : rangeLimit, speed, arriveSpeed, 0,
                drive, m_patternId, close, 0.0f, -1));
        }
        else
        {
            Vec3 goal = {0.0f, 0.0f, 0.0f};
            unsigned int polygon, island;
            const Vec3& from = self->getPosition();
            const Vec3& to = targetInfo->getPosition();
            pathfinding->findClosestValidPolygon(polygon, island, goal, mobile->getVehicleNumber(), to, from, 10, equipment);
            const float maximum = weaponInfo->getMaxRange();
            const float minimum = weaponInfo->getMinRange();
            const float band = (maximum - minimum) * 0.1f;
            const float stoppingDistance = weaponInfo->getMinRange() + band;
            const unsigned long long drive = controls->getDriveControls();
            const float arriveSpeed = mobile->getMaxSpeed();
            const float speed = mobile->getMaxSpeed();
            movement = checkPointer(new BAPAMoveToFinding(bot, goal, speed, arriveSpeed, 0,
                drive, m_patternId, stoppingDistance, 0.0f));
        }
        BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(bot, m_targets[bot->getBotId()], 0.034906585f,
            checkPointer(new BAPConFalse()), controls->getLookControls(), m_patternId, 0, 0.0f));
        approach->addStatement(movement);
        approach->addStatement(look);
        statement = approach;
    }
    else if (lineOfFire == 1 || lineOfFire == 2)
    {
        BAPCombinerSerial* const sequence = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        if (weaponInfo->getWeaponActivate() != 64)
            sequence->addStatement(checkPointer(new BAPAChangeWeapon(bot, weapon, m_patternId)));
        BAPConWeaponMagAmmo* const empty = checkPointer(new BAPConWeaponMagAmmo(weaponVariable, newVariable(context, 0), false));
        BAPATrigger* const reload = reloadTrigger(m_patternId);
        sequence->addStatement(checkPointer(new BAPFlowCIf(empty, reload, 0, m_patternId, false, false)));
        BAPVariable<int>* const unused = newVariable(context, 0);
        (void)unused;
        BAPConNot* alive;
        {
            std::list<BAPCondition*> lost;
            lost.push_back(checkPointer(new BAPConObjectHealth(m_targets[bot->getBotId()], 0.0f)));
            lost.push_back(checkPointer(new BAPConWeaponMagAmmo(weaponVariable, newVariable(context, 0), false)));
            BAPConOr* const any = checkPointer(new BAPConOr(lost));
            std::list<BAPCondition*> wrapped;
            wrapped.push_back(any);
            alive = checkPointer(new BAPConNot(wrapped));
        }
        BAPCombinerSerial* const repeat = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        if (!weaponInfo->hasAutoReload())
        {
            BAPConWeaponMagAmmo* const noAmmo = checkPointer(new BAPConWeaponMagAmmo(weaponVariable, newVariable(context, 0), false));
            BAPATrigger* const trigger = reloadTrigger(m_patternId);
            repeat->addStatement(checkPointer(new BAPFlowCWhile(noAmmo, trigger, m_patternId)));
        }
        // shipped defect: this cooling branch is selected when hasHeat() is false.
        if (!weaponInfo->hasHeat())
        {
            BAPConWeaponHeat* const hot = checkPointer(new BAPConWeaponHeat(weaponVariable, 0.8f));
            BAPConWeaponHeat* const cooled = checkPointer(new BAPConWeaponHeat(weaponVariable, 0.7f));
            BAPCombinerSerial* const cool = checkPointer(new BAPCombinerSerial(m_patternId, -1));
            BAPAResetControls* const reset = checkPointer(new BAPAResetControls(false, false, true, m_patternId));
            BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(bot, m_targets[bot->getBotId()], 0.1f,
                cooled, controls->getLookControls(), m_patternId, 0, 0.0f));
            cool->addStatement(look);
            cool->addStatement(reset);
            repeat->addStatement(checkPointer(new BAPFlowCIf(hot, cool, 0, m_patternId, false, false)));
        }
        BAPVariable<int>* const ammo = newVariable(context, 0);
        repeat->addStatement(checkPointer(new BAPWrapperWeapon(static_cast<BAPWrapperWeapon::ValueId>(0), ammo, weaponVariable, false)));
        BAPCombinerParallel* const firing = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
        const Vec3 zero = {0.0f, 0.0f, 0.0f};
        // shipped defect: tests the allocated array pointer, not this bot's flag.
        BAPAAimAtObject* const aim = checkPointer(new BAPAAimAtObject(bot, m_targets[bot->getBotId()], zero,
            checkPointer(new BAPConFalse()), controls->getAimControls(), m_patternId, m_perBotFlag != 0, 0, 0));
        firing->addStatement(aim);
        BAPVariable<float>* const heat = newVariable(context, 0.0f);
        firing->addStatement(checkPointer(new BAPWrapperWeapon(static_cast<BAPWrapperWeapon::ValueId>(1), heat, weaponVariable, true)));
        LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(physical->getLocalBoundingBox());
        const Vec3& selfPositionNow = self->getPosition();
        const Vec3& targetPositionNow = targetInfo->getPosition();
        const float rangeDistance = distanceBetween(selfPositionNow, targetPositionNow);
        const float minimum = weaponInfo->getMinRange();
        const float maximum = weaponInfo->getMaxRange();
        const float rangeMinimum = weaponInfo->getMinRange();
        const float fraction = (rangeDistance - minimum) / (maximum - rangeMinimum);
        const float precisionScale = (1.0f - fraction) * 0.1f + fraction * 5.0f;
        float precisionFactor;
        float minimumPrecision;
        if (targetInfo->getObjectTypeFlags() & 0x10)
        {
            precisionFactor = 0.8f;
            minimumPrecision = 0.5f;
        }
        else if (m_perBotFlag)
        {
            precisionFactor = 2.5f;
            minimumPrecision = 10.0f;
        }
        else
        {
            precisionFactor = 1.5f;
            minimumPrecision = 5.0f;
        }
        const float xSize = box->getSize().x;
        const float ySize = box->getSize().y;
        const float zSize = box->getSize().z;
        const float yz = zSize > ySize ? zSize : ySize;
        const float size = yz > xSize ? yz : xSize;
        const float scaled = size * precisionFactor;
        const float precision = scaled > minimumPrecision ? scaled : minimumPrecision;
        BAPCConPrecision* const accurate = checkPointer(new BAPCConPrecision(m_targets[bot->getBotId()], aim,
            precision * precisionScale, !weaponInfo->getBurst(), false, false));
        BAPConOr* stopFiring;
        {
            std::list<BAPCondition*> stop;
            stop.push_back(checkPointer(new BAPConWeaponMagAmmo(weaponVariable, ammo, true)));
            stop.push_back(checkPointer(new BAPConOpLessThan<float>(newVariable(context, 0.5f), heat)));
            stopFiring = checkPointer(new BAPConOr(stop));
        }
        BAPStatement* fire;
        if (weaponInfo->getBurst())
        {
            const unsigned long long mask = inputMask(weaponInfo->getWeaponFire());
            fire = checkPointer(new BAPATriggerContinously(static_cast<io::PlayerInputMap>(weaponInfo->getWeaponFire()), accurate, 0,
                stopFiring, mask, m_patternId, 0.0));
        }
        else
        {
            const unsigned long long mask = inputMask(weaponInfo->getWeaponFire());
            BAPConFalse* const end = checkPointer(new BAPConFalse());
            BAPATrigger* const trigger = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(weaponInfo->getWeaponFire()), accurate,
                end, mask, m_patternId, 0.0, false));
            std::list<BAPCondition*> conditions;
            conditions.push_back(stopFiring);
            BAPConNot* const keepFiring = checkPointer(new BAPConNot(conditions));
            fire = checkPointer(new BAPFlowCWhile(keepFiring, trigger, m_patternId));
        }
        firing->addStatement(fire);
        repeat->addStatement(firing);
        repeat->addStatement(checkPointer(new BAPANotifyBot(static_cast<BAPANotifyBot::Type>(0),
            newVariable(context, 1), targetVariable)));
        sequence->addStatement(checkPointer(new BAPFlowCWhile(alive, repeat, m_patternId)));
        BAPCombiner* root;
        if (separateControls)
            root = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        else
            root = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        root->addStatement(checkPointer(new BAPAResetControls(true, false, false, m_patternId)));
        root->addStatement(sequence);
        statement = root;
    }
    BAPIWFire* const plan = checkPointer(new BAPIWFire(bot, m_targets[bot->getBotId()], m_flag20, false,
        weapon, -1, statement, m_patternId));
    plan->setInt28(lineOfFire);
    plan->setParent(this);
    const unsigned int firingBotId = bot->getBotId();
    const unsigned int* const targets = m_targets;
    const int side = bot->getSide();
    const int player = bot->getPlayerId();
    SquadManager::m_instance->shotsFired(player, side, targets[firingBotId]);
    return plan;
}

// ELF 0x2453A0
void BBPFire2d::init(Bot* bot, unsigned int target, int weapon, bool flag20, bool flag21)
{
    init(bot, target, weapon, flag20, flag21, false);
}

// ELF 0x2454A0
void BBPFire2d::init(Bot* bot, unsigned int positionArg, const Vec3& position, int weapon)
{
    Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviourPattern/BBPFire2d.cpp", 0x8D, "")
        << "LaserGuided missiles unsuported by artillry.";
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_positionArgs[bot->getBotId()] = positionArg;
    m_targetPositions[bot->getBotId()] = position;
}

// ELF 0x24B6D0
int BBPFire2d::getImplementationType()
{
    return 2;
}

// ELF 0x245400
void BBPFire2d::init(Bot* bot, unsigned int target, int weapon, bool flag20, bool flag21, bool perBotFlag)
{
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = target;
    m_perBotFlag[bot->getBotId()] = perBotFlag;
    m_weapons[bot->getBotId()] = weapon;
    m_flag20 = flag20;
    m_flag21 = flag21;
    m_positionArgs[bot->getBotId()] = static_cast<unsigned int>(-1);
}

} // namespace ai
} // namespace hfe
} // namespace dice
