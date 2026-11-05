// Fire pattern of artillery drivers and other ground vehicles that aim with their own turret
// (AI/AIDLL/AI/AIBotBehaviourPattern/BBPFire2dDriver.cpp in the image, the init(Vec3) __FILE__).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// createPlan was rebuilt from the listing block by block: Ghidra drops the bool arguments of the
// condition and flow constructors, passes the findClosestValidPolygon out-values by value and
// shows the while loop's swapped arguments as constants; GCC evaluates the constructor arguments
// right to left, which fixes the source order of the nested allocations below.
// [PLATFORM] precision: the x87 build keeps the distance sums, the circle radius and the
// approach-distance products in extended precision before rounding; the float expressions below
// round each step.

#include "BBPFire2dDriver.h"

#include <cmath>
#include <list>
#include <vector>

#include "../../../../Core/Debug.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPActionInline.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPATurnTowardsObject.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConObjectDistance.h"
#include "../AIBotActionPlan/BAPConObjectHealth.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPConObjectOccupied.h"
#include "../AIBotActionPlan/BAPConObjectValidAiming.h"
#include "../AIBotActionPlan/BAPConObjectValidPitchAiming.h"
#include "../AIBotActionPlan/BAPConObjectValidYawAiming.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIBotActionPlan/BAPVariable.h"
#include "../AIBotActionPlan/BAPWrapperAiming.h"
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
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/TraceUtils.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "WeaponChoice.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x251060, __tcf_0 0x2510C0, _GLOBAL__I 0x2510F0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAISettings* settings()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

IAIObjectEnvironment* objectEnvironment()
{
    return static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
}

void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

// The image's inlined plan-variable construction: `checkPointer(new BAPVariable<T>(context, value))`
// followed by `std::list<BotScriptVariable*>::insert(context->begin(), &variable)`.
template <class T>
BAPVariable<T>* newVariable(BAPContext* context, const T& value)
{
    BAPVariable<T>* const variable = checkPointer(new BAPVariable<T>(context, value));
    context->addVariable(variable);
    return variable;
}

// |to - from| from the extended-precision sum, as the image computes both target distances.
float distanceBetween(const Vec3& from, const Vec3& to)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The approach move both the direct and the aiming branch build when there is no line of fire or
// the target is out of reach (0x24CC23..0x24CEE3 and 0x250310..0x2505CA, 0x24EB30.., 0x250A10..).
// A valid target position gets an object-finding move that ends near (50 m) or when the target
// moved 10 m; otherwise the closest valid polygon towards the target is approached up to a tenth
// of the weapon's range band. The findClosestValidPolygon answer is discarded as compiled.
BAPStatement* newApproach(Bot* bot, unsigned int target, unsigned int currentTarget, Information* self,
                          Information* targetInfo, IPIMobile* mobile, IPIControlInfo* controlInfo,
                          WeaponInfo* weaponInfo, int equipmentType, float targetDistance, int patternId)
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    if (pathfinding->isValidPosition(mobile->getVehicleNumber(), equipmentType, targetInfo->getPosition(), true))
    {
        BAPConAnd* const close = checkPointer(new BAPConAnd());
        close->addCondition(checkPointer(new BAPConObjectDistance(currentTarget, 50.0f, true, true, false)));
        close->addCondition(checkPointer(new BAPConObjectMoved(currentTarget, 10.0f)));
        // min(range * 0.95, distance * 0.75); unordered keeps the range term (`fcmovnbe`).
        const float rangeLimit = weaponInfo->getMaxRange() * 0.95f;
        const float approach = targetDistance * 0.75f;
        return checkPointer(new BAPAMoveToObjectFinding(bot, target, rangeLimit > approach ? approach : rangeLimit,
                                                        mobile->getMaxSpeed(), mobile->getMaxSpeed(),
                                                        checkPointer(new BAPConFalse()),
                                                        controlInfo->getDriveControls(), patternId, close, 0.0f, -1));
    }

    Vec3 goal;
    goal.x = goal.y = goal.z = 0.0f;
    Vec3 unused;   // zeroed beside the goal and never read
    unused.x = unused.y = unused.z = 0.0f;
    (void)unused;
    unsigned int island;
    unsigned int polygon;
    pathfinding->findClosestValidPolygon(polygon, island, goal, mobile->getVehicleNumber(), targetInfo->getPosition(),
                                         self->getPosition(), 10, equipmentType);
    return checkPointer(new BAPAMoveToFinding(
        bot, goal, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0, controlInfo->getDriveControls(), patternId,
        weaponInfo->getMinRange() + (weaponInfo->getMaxRange() - weaponInfo->getMinRange()) * 0.1f, 0.0f));
}

} // namespace

// ELF 0x24BBF0 (C1; C2 0x24BA80): BBPFire(), the vtable store, the argument at +0x08, then one
// flag per bot slot (IAISettings::getMaxNBots), not initialised.
BBPFire2dDriver::BBPFire2dDriver(int patternId)
    : BBPFire()
{
    m_patternId = patternId;
    m_flags28 = checkPointer(new bool[settings()->getMaxNBots()]);
}

// ELF 0x24BDB0 (D1; D2 0x24BD60, D0 0x24BE00): the flag array, then BBPFire's destructor.
BBPFire2dDriver::~BBPFire2dDriver()
{
    if (m_flags28)
        delete[] m_flags28;
}

// ELF 0x24C100. An uninitiated pattern clears the debugger and drops the plan (the context is left)
// and builds nothing. Otherwise the controls are reset (true, false, true), a weapon is chosen
// for the distance and the target's strength type (none: the own object's update, no plan), and
// the line of fire is classified: 1 when the target is sensed directly, else 2 or 0 from the
// indirect line-of-fire test against the sensed part (or the target's position). A fire plan of
// this pattern built for the same target, classification and closeness is kept. Otherwise the
// plan is rebuilt as IWFire over
//   0: an approach move (newApproach);
//   too close (inside the minimum range + 0.5 m): a move to the best point found by tracing a
//      circle of 2 * minimum range + 5 m around the target, 15 degree steps alternating to both
//      sides of the bearing away from the target, ended once outside the minimum range + 1 m;
//   else: parallel(aiming wrapper, if(within minimum range + 0.5 m: nothing,
//      else if(out of 0.9 * range or no valid aim: approach move, else fire loop))),
// the fire loop being while(alive and occupied and not within the minimum range + 0.5 m: reset
// controls), behind a yaw check that turns towards the target first for a turnable vehicle.
// Shipped quirks kept: the target information, its physical, the armament and the equipment are
// not null-checked; the target's local bounding box is fetched and dropped; two plan variables
// (weapon, target) are created and never used; the while loop gets (m_patternId != 0, 0) for
// (testOnFinish, patternId); the turn takes the raw drive-turn input as its control mask.
BotScript* BBPFire2dDriver::createPlan(Bot* bot)
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
    const unsigned int target = m_targets[bot->getBotId()];
    Information* const targetInfo = informationManager->getInformation(target);
    IAIObjectEnvironment* const environment = objectEnvironment();
    Vec2 targetPosition2d;   // converted and never read
    targetPosition2d.x = targetPosition2d.y = 0.0f;
    Vec2 selfPosition2d;
    selfPosition2d.x = selfPosition2d.y = 0.0f;
    IPIPhysical* const targetPhysical = targetInfo->getPhysical();
    vec3ToVec2XZ(targetInfo->getPosition(), targetPosition2d);
    vec3ToVec2XZ(self->getPosition(), selfPosition2d);
    IPIMobile* const mobile = self->getMobile();
    IPIControlInfo* const controlInfo = self->getControlInfo();
    int armamentIterator;
    IPIArmament* const armament =
        informationManager->getInformation(self->getFirstSecondaryObject(armamentIterator, bot->getSide()))->getArmament();
    const int strengthType = targetPhysical->getStrengthType();
    const float distance = distanceBetween(self->getPosition(), targetInfo->getPosition());
    const int weapon = choseWeapon(armament->getWeapons(), m_weapons[bot->getBotId()], distance, strengthType, false,
                                   false, false, false);
    if (weapon == -1)
    {
        self->getAIObject()->update();
        return 0;
    }

    WeaponInfo* const weaponInfo = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weapon));
    const float targetDistance = distanceBetween(self->getPosition(), targetInfo->getPosition());
    const bool tooClose = weaponInfo->getMinRange() + 0.5f > targetDistance;
    SensedObject* const sensed = bot->getSensedObjectFromHandle(m_targets[bot->getBotId()]);
    targetPhysical->getLocalBoundingBox();   // [UNRESOLVED] result unused in the image (0x24C57D)

    int lineOfFire;
    if (sensed && !(sensed->m_flags & 8))
        lineOfFire = 1;
    else
    {
        Vec3 aimPoint;
        aimPoint.x = aimPoint.y = aimPoint.z = 0.0f;
        if (sensed)
        {
            // The sensed part's offset in the target's frame, in world space (0x250DC0).
            const Mat4& transform = targetInfo->getTransformation();
            const Vec3& part = sensed->m_seenPosition;
            aimPoint.x = transform.m[0] * part.x + transform.m[4] * part.y + transform.m[8] * part.z + transform.m[12];
            aimPoint.y = transform.m[1] * part.x + transform.m[5] * part.y + transform.m[9] * part.z + transform.m[13];
            aimPoint.z = part.x * transform.m[2] + part.y * transform.m[6] + part.z * transform.m[10] + transform.m[14];
        }
        else
            aimPoint = targetInfo->getPosition();
        std::vector<IAIObject*> excluded;
        excluded.push_back(self->getAIObject());
        excluded.push_back(targetInfo->getAIObject());
        lineOfFire = environment->hasIndirectLineOfFire(weaponInfo->getWeapon(), weaponInfo->getPosition(), aimPoint,
                                                        excluded)
            ? 2
            : 0;
    }

    if (bot->hasPlan() && static_cast<BAPInfoWrapper*>(bot->getPlan())->getGeneratorId() == m_patternId
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getId() == BAPSTInfoWrapper
        && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0
        && static_cast<BAPIWFire*>(bot->getPlan())->getTarget() == m_targets[bot->getBotId()]
        && static_cast<BAPIWFire*>(bot->getPlan())->getInt28() == lineOfFire
        && static_cast<BAPIWFire*>(bot->getPlan())->getFlagB() == tooClose)
        return bot->getPlan();

    bot->getSquadMember()->clearRequestAction();
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    dropPlan(bot);
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    const int equipmentType = self->getUnit()->getEquipmentType();

    BAPStatement* statement;
    if (lineOfFire == 0)
    {
        statement = newApproach(bot, target, m_targets[bot->getBotId()], self, targetInfo, mobile, controlInfo,
                                weaponInfo, equipmentType, targetDistance, m_patternId);
    }
    else if (tooClose)
    {
        float angle = 0.0f;
        float bestDistanceSqr = 0.0f;
        Vec2 best = selfPosition2d;
        const float minRange = weaponInfo->getMinRange();
        const float radius = minRange + minRange + 5.0f;
        const float limitSqr = (radius - 1.0f) * (radius - 1.0f);
        const Vec3& targetPosition = targetInfo->getPosition();
        const Vec3& selfPosition = self->getPosition();
        Vec3 away = bapinline::vec3(selfPosition.x - targetPosition.x, selfPosition.y - targetPosition.y,
                                    selfPosition.z - targetPosition.z);
        if (bapinline::lengthSqr(away) > 0.1f)
            bapinline::normalize(away);
        else
            away = bapinline::vec3(1.0f, 0.0f, 0.0f);
        while (bestDistanceSqr <= limitSqr && angle <= 1.5707964f)
        {
            Vec3 point = bapinline::vec3(0.0f, 0.0f, 0.0f);
            const bool reached = TraceUtils::tracePointOnCircle(away, targetInfo->getPosition(), self->getPosition(),
                                                                mobile->getVehicleNumber(), angle, radius, point);
            angle = -angle;
            if (angle >= 0.0f)
                angle += 0.2617994f;   // 15 degrees (0x3E860A92)
            if (reached)
            {
                const Vec3& centre = targetInfo->getPosition();
                const float dx = centre.x - point.x;
                const float dy = centre.y - point.y;
                const float dz = centre.z - point.z;
                const float distanceSqr = dx * dx + dy * dy + dz * dz;
                if (distanceSqr > bestDistanceSqr)
                {
                    bestDistanceSqr = distanceSqr;
                    vec3ToVec2XZ(point, best);
                }
            }
        }

        Vec3 goal = bapinline::vec3(0.0f, 0.0f, 0.0f);
        BAPConObjectDistance* const inside =
            checkPointer(new BAPConObjectDistance(target, weaponInfo->getMinRange() + 1.0f, true, false, false));
        BAPConNot* const outside = checkPointer(new BAPConNot());
        outside->addCondition(inside);
        vec2ToVec3XZ(best, goal);
        statement = checkPointer(new BAPAMoveToFinding(bot, goal, mobile->getMaxSpeed(), mobile->getMaxSpeed(), outside,
                                                       controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f));
    }
    else
    {
        const bool turnable = mobile->isTurnable();
        int secondaryIterator;
        const unsigned int secondary = self->getFirstSecondaryObject(secondaryIterator, self->getSide());
        newVariable<WeaponInfo*>(context, weaponInfo);   // never read
        newVariable<unsigned int>(context, target);       // never read
        BAPVariable<bool>* const aimValid = newVariable<bool>(context, false);
        BAPVariable<Vec3>* const aimDirection = newVariable<Vec3>(context, bapinline::vec3(0.0f, 1.0f, 0.0f));
        BAPVariable<Vec3>* const aimPosition = newVariable<Vec3>(context, bapinline::vec3(0.0f, 1.0f, 0.0f));

        BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        parallel->addStatement(
            checkPointer(new BAPWrapperAiming(aimDirection, aimValid, aimPosition, secondary, target, false)));

        // Reposition when out of 0.9 * range or without a valid aim.
        BAPConOr* const reposition = checkPointer(new BAPConOr());
        {
            BAPConObjectDistance* const inRange =
                checkPointer(new BAPConObjectDistance(target, weaponInfo->getMaxRange() * 0.9f, true, false, false));
            BAPConNot* const outOfRange = checkPointer(new BAPConNot());
            outOfRange->addCondition(inRange);
            reposition->addCondition(outOfRange);
        }
        if (turnable)
        {
            BAPConObjectValidPitchAiming* const pitch =
                checkPointer(new BAPConObjectValidPitchAiming(target, secondary, aimValid, aimDirection));
            BAPConNot* const noPitch = checkPointer(new BAPConNot());
            noPitch->addCondition(pitch);
            reposition->addCondition(noPitch);
        }
        else
        {
            BAPConObjectValidAiming* const aim =
                checkPointer(new BAPConObjectValidAiming(target, secondary, aimValid, aimDirection, false));
            BAPConNot* const noAim = checkPointer(new BAPConNot());
            noAim->addCondition(aim);
            reposition->addCondition(noAim);
        }

        BAPStatement* const approach = newApproach(bot, target, m_targets[bot->getBotId()], self, targetInfo, mobile,
                                                   controlInfo, weaponInfo, equipmentType, targetDistance, m_patternId);

        BAPConAnd* keepFiring;
        {
            std::list<BAPCondition*> lost;
            lost.push_back(checkPointer(new BAPConObjectHealth(target, 0.0f)));
            BAPConObjectOccupied* const occupied = checkPointer(new BAPConObjectOccupied(secondary));
            BAPConNot* const empty = checkPointer(new BAPConNot());
            empty->addCondition(occupied);
            lost.push_back(empty);
            BAPConOr* const lostTarget = checkPointer(new BAPConOr(lost));
            std::list<BAPCondition*> wrapped;
            wrapped.push_back(lostTarget);
            BAPConNot* const alive = checkPointer(new BAPConNot(wrapped));
            BAPConObjectDistance* const inside =
                checkPointer(new BAPConObjectDistance(target, weaponInfo->getMinRange() + 0.5f, true, false, false));
            BAPConNot* const outside = checkPointer(new BAPConNot());
            outside->addCondition(inside);
            keepFiring = checkPointer(new BAPConAnd());
            keepFiring->addCondition(outside);
            keepFiring->addCondition(alive);
        }
        BAPAResetControls* const reset = checkPointer(new BAPAResetControls(true, true, true, m_patternId));
        // The image passes (m_patternId != 0, 0) for (testOnFinish, patternId) (0x24D2CF): kept.
        BAPFlowCWhile* const fireLoop = checkPointer(new BAPFlowCWhile(keepFiring, reset, m_patternId != 0, 0));
        BAPStatement* fire = fireLoop;
        if (turnable)
        {
            BAPConObjectValidYawAiming* const yaw =
                checkPointer(new BAPConObjectValidYawAiming(target, secondary, aimValid, aimDirection));
            // The drive-turn input itself, zero-extended, is the control mask (0x24D958).
            BAPATurnTowardsObject* const turn = checkPointer(new BAPATurnTowardsObject(
                bot, target, 0.08726646f, checkPointer(new BAPConFalse()),   // 5 degrees (0x3DB2B8C2)
                static_cast<unsigned long long>(static_cast<unsigned int>(controlInfo->getDriveTurnControl())),
                m_patternId));
            fire = checkPointer(new BAPFlowCIf(yaw, fireLoop, turn, m_patternId, true, false));
        }
        BAPFlowCIf* const engage = checkPointer(new BAPFlowCIf(reposition, approach, fire, m_patternId, true, false));
        BAPConObjectDistance* const inside =
            checkPointer(new BAPConObjectDistance(target, weaponInfo->getMinRange() + 0.5f, true, false, false));
        parallel->addStatement(checkPointer(new BAPFlowCIf(inside, 0, engage, m_patternId, true, false)));
        statement = parallel;
    }

    BAPIWFire* const plan = checkPointer(new BAPIWFire(bot, m_targets[bot->getBotId()], m_flag20, tooClose, weapon, -1,
                                                       statement, m_patternId));
    plan->setInt28(lineOfFire);
    plan->setParent(this);   // +0x04 (BotScript::mpOwner)
    SquadManager::m_instance->shotsFired(bot->getPlayerId(), bot->getSide(), m_targets[bot->getBotId()]);
    return plan;
}

// ELF 0x24BE50: the six-argument form with the last flag false.
void BBPFire2dDriver::init(Bot* bot, unsigned int target, int weapon, bool flag20, bool flag21)
{
    init(bot, target, weapon, flag20, flag21, false);
}

// ELF 0x24BF50. The Debug error is the image's (it aborts in this tree); the arguments are then
// stored like BBPFire3d's overload.
void BBPFire2dDriver::init(Bot* bot, unsigned int positionArg, const Vec3& position, int weapon)
{
    Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviourPattern/BBPFire2dDriver.cpp", 0x93, "")
        << "LaserGuided missiles unsuported by artillry drivers.";
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_positionArgs[bot->getBotId()] = positionArg;
    m_targetPositions[bot->getBotId()] = position;
}

// ELF 0x251110 (weak)
int BBPFire2dDriver::getImplementationType()
{
    return 3;
}

// ELF 0x24BEB0
void BBPFire2dDriver::init(Bot* bot, unsigned int target, int weapon, bool flag20, bool flag21, bool flag28)
{
    m_initiated[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = target;
    m_flags28[bot->getBotId()] = flag28;
    m_weapons[bot->getBotId()] = weapon;
    m_flag20 = flag20;
    m_flag21 = flag21;
    m_positionArgs[bot->getBotId()] = static_cast<unsigned int>(-1);
}

} // namespace ai
} // namespace hfe
} // namespace dice
