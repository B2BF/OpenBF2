// Infantry fire pattern: chooses how a soldier bot engages its target (direct weapon, indirect
// weapon from here or from a firing position) and builds the matching action plan.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, with symbols); every body carries its ELF
// address. x87 code was read with the AT&T fsub(r)p/fdiv(r)p operand swap in mind. Package E
// (docs/macos-ai-behaviour-plan.md; state and next steps in docs/macos-ai-w1-e-fire.md).

// static init: ELF 0x27BA40 (__tcf_0 0x27BAA0, _GLOBAL__I 0x27BAD0): <iostream>'s Init object only.
#include <iostream>

#include "BBPFireInfantery.h"

#include <cmath>
#include <string>
#include <vector>

#include "../../../../Core/Debug.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAAimAt.h"
#include "../AIBotActionPlan/BAPAAimAtPoint.h"
#include "../AIBotActionPlan/BAPALook.h"
#include "../AIBotActionPlan/BAPALookAtPoint.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCConLookDeviation.h"
#include "../AIBotActionPlan/BAPCConPrecision.h"
#include "../AIBotActionPlan/BAPConAllowFire.h"
#include "../AIBotActionPlan/BAPConDistanceToProj.h"
#include "../AIBotActionPlan/BAPConOkToThrowGrenade.h"
#include "../AIBotActionPlan/BAPConSplashDamage.h"
#include "../AIBotActionPlan/BAPConWeaponHasLockOnTarget.h"
#include "../AIBotActionPlan/BAPConWeaponHeat.h"
#include "../AIBotActionPlan/BAPConWeaponIsZoomed.h"
#include "../AIBotActionPlan/BAPAAimAtObject.h"
#include "../AIBotActionPlan/BAPAChangeWeapon.h"
#include "../AIBotActionPlan/BAPAEvaluateCondition.h"
#include "../AIBotActionPlan/BAPAForgetTarget.h"
#include "../AIBotActionPlan/BAPAIdle.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPAMergeSoldierInputs.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPAOverrideSensingAgent.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPAUseFlareOnMissileLock.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConIsSprinting.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConObjectDistance.h"
#include "../AIBotActionPlan/BAPConObjectHealth.h"
#include "../AIBotActionPlan/BAPConObjectLineOfFire.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPConObjectValidAiming.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPConPointDistance.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConSharedCondition.h"
#include "../AIBotActionPlan/BAPConStrategicObjectStatus.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPConWeaponDetonatorIsActive.h"
#include "../AIBotActionPlan/BAPConWeaponMagAmmo.h"
#include "../AIBotActionPlan/BAPConWeaponReadyToFire.h"
#include "../AIBotActionPlan/BAPFlowCEnd.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPICTurnLock.h"
#include "../AIBotActionPlan/BAPWrapperAiming.h"
#include "../AIBotActionPlan/BAPWrapperLookAt.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIBotBehaviour/BBFire.h"
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
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../AIUtils/TraceUtils.h"
#include "../Main/AIRandom.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

double gameTime()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

int maxBots()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
}

// [INFERRED] helper shape: the image inlines AIRandom's tempering and computes
// `r * 2.3283064e-10 * scale + offset` at every site (createPlan 0x25E382, 0x25DF17, ...).
float randomFloat(float scale, float offset)
{
    return AIRandom::m_instance->nextFloat() * scale + offset;
}

IAIObjectEnvironment* objectEnvironment()
{
    return static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
}

// ELF 0x25C5F0. Finds a firing position for an indirect weapon: 10 m short of the target, then
// moved along the line towards (too far) or away from (too close) the target so that the
// weapon's minimum range plus 3 m fits, snapped to the navmesh. Returns false (no position) when
// the 10-m point is already inside [minRange, maxRange]; outBase is written in every case.
bool getIndirectWeaponCloseToFiringPositions(Vec3& outPosition, Vec3& outBase, Information* self,
                                             IPIMobile* mobile, int equipmentType, WeaponInfo* weapon,
                                             Information* target, const Vec3& targetPosition)
{
    const Vec3& selfPosition = self->getPosition();
    Vec3 aim = targetPosition;
    Vec3 dir = { aim.x - selfPosition.x, aim.y - selfPosition.y, aim.z - selfPosition.z };
    float distance = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (distance > 0.0001f)
    {
        const float inv = 1.0f / distance;
        dir.x *= inv;
        dir.y *= inv;
        dir.z *= inv;
    }
    distance -= 10.0f;
    const Vec3 step = { dir.x * 10.0f, dir.y * 10.0f, dir.z * 10.0f };
    aim.x -= step.x;
    aim.y -= step.y;
    aim.z -= step.z;
    outBase = aim;

    unsigned int polygon;
    unsigned int cluster;
    Vec3 position;
    if (!(weapon->getMinRange() > distance))     // 0x25C705 jbe: NaN takes this arm
    {
        if (!(distance > weapon->getMaxRange()))     // 0x25C7F0 jbe: NaN returns false
            return false;

        // Too far: walk towards the target.
        const Vec3& from = self->getPosition();
        const float move = (distance + 3.0f) - weapon->getMinRange();
        position.x = dir.x * move + from.x;
        position.y = dir.y * move + from.y;
        position.z = move * dir.z + from.z;
    }
    else
    {
        // Too close: back away from the target.
        const Vec3& from = self->getPosition();
        const float move = (weapon->getMinRange() + 3.0f) - distance;
        const Vec3 offset = { dir.x * move, dir.y * move, move * dir.z };
        position.x = from.x - offset.x;
        position.y = from.y - offset.y;
        position.z = from.z - offset.z;
    }
    const Vec3& reference = target->getPosition();
    const int vehicleNumber = mobile->getVehicleNumber();
    IAIPathfinding::instance->findClosestValidPolygon(polygon, cluster, outPosition, vehicleNumber, position,
                                                      reference, 10, equipmentType);
    return true;
}

// ELF 0x25C8E0: `xorl %eax,%eax; ret` - aiming failures are never detected in this build.
bool detectAimingFailure(BotMain*)
{
    return false;
}

// The image's "drop the plan" sequence (createPlan 0x25C8F0 head and 0x25D769).
void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        BotScript* const plan = bot->getPlan();
        delete plan;
        bot->setPlan(0);
    }
}

// Phase 3 (firing) is over when the target is dead or the magazine is empty (m_shots == -1), or
// when the burst is fired; a finished decision goes to the front of the history
// (createPlan 0x25E93D..0x25E906, three copies in the image).
bool burstFinished(BBPFireInfantery::Data& data, const BBPFireInfantery::FiringState* state,
                   IPIPhysical* targetPhysical, WeaponInfo* weapon, IAIPlayer* player)
{
    if (state->m_shots == -1)
    {
        if (targetPhysical->getHealth() <= 0.0f || weapon->getAmmo() == 0)
        {
            data.m_history.push_front(data.m_decision);
            return true;
        }
        return false;
    }
    if (state->m_shots <= player->getNumShotsFired(weapon->getWeapon()) - data.m_shotsAtPhaseStart
        || weapon->getAmmo() == 0)
    {
        data.m_history.push_front(data.m_decision);
        return true;
    }
    return false;
}

} // namespace

// ELF 0x25BAA0
BBPFireInfantery::FiringState::FiringState()
    : m_target(static_cast<unsigned int>(-1))
    , m_weapon(-1)
    , m_shots(-1)
    , m_bool0C(false)
    , m_bool14(false)
    , m_backOff(false)
    , m_independentAim(false)
    , m_hasFiringPosition(false)
    , m_bool24(false)
    , m_bool25(false)
    , m_bool26(false)
    , m_arrivalRadius(0.0f)
{
    m_position.x = m_position.y = m_position.z = 0.0f;
    m_firingPosition.x = m_firingPosition.y = m_firingPosition.z = 0.0f;
}

// ELF 0x25BC60
BBPFireInfantery::Data::Data()
    : m_decision(FireDecisionNone)
    , m_history()
    , m_phase(0)
    , m_lockStartTime(0.0)
    , m_lockDuration(10.0)
    , m_int4C(-1)
    , m_int50(-1)
{
    m_vec24.x = m_vec24.y = m_vec24.z = 0.0f;
    m_phaseTargetPosition.x = m_phaseTargetPosition.y = m_phaseTargetPosition.z = 0.0f;
    m_firePosition.x = m_firePosition.y = m_firePosition.z = 0.0f;
}

// ELF 0x25BFE0
BBPFireInfantery::BBPFireInfantery(int patternId)
    : BBPFire()
{
    m_patternId = patternId;
    m_firingStates = checkPointer(new FiringState[maxBots()]);
    m_data = checkPointer(new Data[maxBots()]);
}

// ELF 0x25C350
BBPFireInfantery::~BBPFireInfantery()
{
    delete[] m_firingStates;
    delete[] m_data;
}

// ELF 0x27BC00
int BBPFireInfantery::getImplementationType()
{
    return 5;
}

// ELF 0x25C480. Both bools are unused.
void BBPFireInfantery::init(Bot* bot, unsigned int target, int weapon, bool, bool)
{
    const unsigned int id = bot->getBotId();
    m_targetChanged[id] = target != m_targets[id];
    m_targets[id] = target;
    m_weapons[id] = weapon;
    if (!m_initiated[id])
    {
        Data& data = m_data[id];
        data.m_decision = Data::FireDecisionNone;
        data.m_phase = 0;
        data.m_lockStartTime = 0.0;
        data.m_lockDuration = 10.0;
        m_firingStates[id].m_target = static_cast<unsigned int>(-1);
    }
    m_initiated[id] = true;
}

// ELF 0x25C530
void BBPFireInfantery::init(Bot* bot, unsigned int positionArg, const Vec3& position, int weapon)
{
    const unsigned int id = bot->getBotId();
    m_initiated[id] = true;
    m_targets[id] = static_cast<unsigned int>(-1);
    m_weapons[id] = weapon;
    m_positionArgs[id] = positionArg;
    m_targetPositions[id] = position;
    Data& data = m_data[id];
    data.m_decision = Data::FireDecisionNone;
    data.m_phase = 0;
    data.m_lockStartTime = 0.0;
    data.m_lockDuration = 10.0;
    m_firingStates[id].m_target = static_cast<unsigned int>(-1);
}

// ELF 0x25C8F0 (2,906 instructions; Ghidra pseudocode cross-checked against the listing at the
// jump tables 0x4A00C4 / 0x4A00DC, the isFireAllowed sites 0x25D7FC / 0x25D60E and the range
// checks 0x25D830..0x25DA1E). Keeps the current plan while it is this pattern's fire plan for the
// same target, weapon and static-attack mode; otherwise re-evaluates the per-bot decision (Data),
// fills the bot's FiringState and asks createPlanInternal for the plan. Returns 0 and drops the
// current plan while a new decision is pending.
BotScript* BBPFireInfantery::createPlan(Bot* bot)
{
    const unsigned int id = bot->getBotId();
    FiringState* const state = &m_firingStates[id];
    Data& data = m_data[id];

    if (!m_initiated[id])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }

    BBFire* const urgencyGenerator = static_cast<BBFire*>(m_urgencyGenerator);
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = self->getArmament();
    IPIPhysical* const selfPhysical = self->getPhysical();
    IPIControlInfo* const controlInfo = self->getControlInfo();
    IAIPlayer* const player = bot->getPlayer();
    if (!armament || !selfPhysical || !controlInfo || !player)
    {
        m_initiated[id] = false;
        return bot->getPlan();
    }

    const int equipmentType = self->getUnit()->getEquipmentType();
    const Vec3& selfPosition = self->getPosition();
    IPIMobile* const selfMobile = self->getMobile();
    bool independentAim = false;
    if (selfMobile)
    {
        independentAim = (controlInfo->getDriveControls() & controlInfo->getAimControls()) == 0;
        selfMobile->getSector();   // [UNRESOLVED] result unused in the image (0x25CA96)
    }

    bool freshPlan = false;
    if (!bot->hasPlan() || static_cast<BAPInfoWrapper*>(bot->getPlan())->getGeneratorId() != m_patternId
        || static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill() < 0.3f)
        freshPlan = true;
    m_freshPlan = freshPlan;

    // Keep the current plan while nothing it was built from has changed (0x25F541..0x25F5D8).
    int previousWeapon = -1;
    if (bot->hasPlan())
    {
        if (static_cast<BAPInfoWrapper*>(bot->getPlan())->getGeneratorId() == m_patternId
            && static_cast<BAPInfoWrapper*>(bot->getPlan())->getId() == BAPSTInfoWrapper
            && static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == 0
            && static_cast<BAPIWFire*>(bot->getPlan())->getTarget() == m_targets[bot->getBotId()]
            && m_flag20 == static_cast<BAPIWFire*>(bot->getPlan())->getFlagA())
        {
            previousWeapon = static_cast<BAPIWFire*>(bot->getPlan())->getWeapon();
            if (previousWeapon == m_weapons[bot->getBotId()]
                && static_cast<BAPIWFire*>(bot->getPlan())->getUseStaticAttack()
                       == static_cast<BotMain*>(bot)->getUseStaticAttack())
                return bot->getPlan();
        }
    }

    unsigned int target = m_targets[id];
    Information* targetInfo = informationManager->getInformation(target);
    IPIUnit* const targetUnit = targetInfo ? targetInfo->getUnit() : 0;
    Information* vehicleInfo = 0;
    SensedObject* sensed = 0;
    if (targetUnit && targetUnit->getAlwaysTargetSoldier())
    {
        // Aim at the soldier in the vehicle instead, when this bot senses him.
        Information* const occupant =
            informationManager->getInformation(targetUnit->getOccupiedObject(bot->getSide()));
        if (occupant)
        {
            sensed = bot->getSensedObjectFromHandle(targetUnit->getOccupiedObject(bot->getSide()));
            if (sensed)
            {
                target = targetUnit->getOccupiedObject(bot->getSide());
                vehicleInfo = targetInfo;
                targetInfo = occupant;
            }
        }
    }
    IPIPhysical* const targetPhysical = targetInfo ? targetInfo->getPhysical() : 0;
    if (!sensed)
        sensed = bot->getSensedObjectFromHandle(target);
    if (!targetInfo || !targetPhysical || !sensed)
    {
        m_initiated[id] = false;
        return bot->getPlan();
    }

    bool targetIsLodBot = false;
    if (Bot* const targetBot = IBotManager::instance->getBot(targetInfo->getAIObject()))
        targetIsLodBot = targetBot->getLodLevel() > 0;

    const Vec3 targetPosition = targetInfo->getPosition();
    if (IPIMobile* const targetMobile = targetInfo->getMobile())
        targetMobile->getSector();   // [UNRESOLVED] result unused in the image (0x25CC3D)

    if (!(sensed->m_flags & 0x20))
    {
        sensed->m_fireRating = 0;
        sensed->m_unresolved44 = 0;
        sensed->m_flags |= 0x20;
        sensed->m_lastSense = 0x20;
    }

    std::vector<IAIObject*> excluded;
    excluded.push_back(self->getAIObject());
    excluded.push_back(targetInfo->getAIObject());
    if (vehicleInfo)
        excluded.push_back(vehicleInfo->getAIObject());

    const Vec3 toTarget = { targetPosition.x - selfPosition.x, targetPosition.y - selfPosition.y,
                            targetPosition.z - selfPosition.z };
    const float targetDistance =
        std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
    Vec3 direction = { targetPosition.x - selfPosition.x, targetPosition.y - selfPosition.y,
                       targetPosition.z - selfPosition.z };
    const float directionLength =
        std::sqrt(direction.z * direction.z + direction.x * direction.x + direction.y * direction.y);
    if (directionLength > 0.0001f)
    {
        const float inv = 1.0f / directionLength;
        direction.x *= inv;
        direction.y *= inv;
        direction.z *= inv;
    }

    const int weapon = m_weapons[id];
    WeaponInfo* directWeapon = 0;
    WeaponInfo* indirectWeapon = 0;
    WeaponInfo* previousWeaponInfo = 0;
    if (weapon != -1)
    {
        // The image looks the same weapon up twice (0x25CEF6 / 0x25CF28).
        directWeapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weapon));
        indirectWeapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weapon));
    }
    if (previousWeapon != -1)
        previousWeaponInfo = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(previousWeapon));

    if (weapon == -1)
    {
        sensed->m_fireRating = -100;
        return bot->getPlan();
    }

    // The sensed part's offset in the target's frame, in world space.
    const Mat4& targetTransform = targetInfo->getTransformation();
    const Vec3& part = sensed->m_seenPosition;
    const Vec3 aimPoint = {
        targetTransform.m[8] * part.z + targetTransform.m[0] * part.x + targetTransform.m[4] * part.y
            + targetTransform.m[12],
        targetTransform.m[1] * part.x + targetTransform.m[5] * part.y + targetTransform.m[9] * part.z
            + targetTransform.m[13],
        part.x * targetTransform.m[2] + part.y * targetTransform.m[6] + part.z * targetTransform.m[10]
            + targetTransform.m[14]
    };
    // The camera position (translation row) is copied to the stack and never read (0x25D029).
    const float* const camera = static_cast<const float*>(bot->getPlayer()->getCameraTransformation());
    const Vec3 cameraPosition = { camera[12], camera[13], camera[14] };
    (void)cameraPosition;

    const double now = gameTime();
    if (state->m_target != target || now - data.m_lastUpdateTime > 5.0)
    {
        data.m_decision = Data::FireDecisionNone;
        data.m_history.clear();
        data.m_int4C = -1;
        data.m_int50 = -1;
        data.m_lockStartTime = 0.0;
        data.m_lockDuration = 0.0;
    }

    float firingPositionDistance = 0.0f;   // XZ distance to the state's position
    if (state->m_hasFiringPosition)
    {
        const float dx = state->m_position.x - selfPosition.x;
        const float dz = state->m_position.z - selfPosition.z;
        firingPositionDistance = std::sqrt(dz * dz + dx * dx);
    }

    // 1. Is the current decision finished or invalid? (jump table 0x4A00C4: 0, 1, 2, 4, 5; 3 and
    // anything above 5 report and keep the decision)
    bool redecide = false;
    bool reset = false;   // the image's `decision = phase = 0` tail at 0x25E924
    switch (data.m_decision)
    {
    case Data::FireDecisionNone:
        redecide = true;
        reset = true;
        break;

    case Data::FireDecision1:
    case Data::FireDecision2:
    {
        WeaponInfo* const current = data.m_decision == Data::FireDecision1 ? directWeapon : indirectWeapon;
        if (!current)
        {
            redecide = false;
            reset = true;
            break;
        }
        const int phase = data.m_phase;
        if (phase == 0)
        {
            redecide = false;
        }
        else if (data.m_decision == Data::FireDecision1 && phase == 2 && self->getMobile())
        {
            if (!previousWeaponInfo)
                redecide = true;
            else if (state->m_bool14)
                redecide = previousWeaponInfo->getMinRange() + 1.0f < targetDistance;
            else
            {
                redecide = true;
                reset = true;
            }
        }
        else if (phase == 1 && self->getMobile())
        {
            if (!previousWeaponInfo)
                redecide = true;
            else if (state->m_bool14)
                redecide = previousWeaponInfo->getMinRange() < targetDistance
                           && targetDistance < previousWeaponInfo->getMaxRange();
            else if (state->m_hasFiringPosition)
            {
                const float dx = targetPosition.x - data.m_phaseTargetPosition.x;
                const float dy = targetPosition.y - data.m_phaseTargetPosition.y;
                const float dz = targetPosition.z - data.m_phaseTargetPosition.z;
                if (std::sqrt(dz * dz + dx * dx + dy * dy) > 10.0f)
                {
                    redecide = true;
                    reset = true;
                }
                else
                {
                    redecide = firingPositionDistance < state->m_arrivalRadius
                               || (previousWeaponInfo->getMinRange() < targetDistance
                                   && targetDistance < previousWeaponInfo->getMaxRange());
                }
            }
        }
        else if (phase == 3)
        {
            if (!previousWeaponInfo || !current)
            {
                redecide = true;
                reset = true;
                break;
            }
            const bool aimingFailed = detectAimingFailure(static_cast<BotMain*>(bot));
            bool inRange = false;
            if ((self->getMobile() || previousWeaponInfo->getMinRange() < targetDistance)
                && targetDistance < previousWeaponInfo->getMaxRange())
                inRange = true;
            if (aimingFailed)
            {
                urgencyGenerator->getBBPFeedback(target)->m_time = gameTime();
                redecide = true;
                reset = true;
                break;
            }
            redecide = true;
            if (inRange)
                redecide = burstFinished(data, state, targetPhysical, previousWeaponInfo, player);
            reset = redecide;
        }
        break;
    }

    case Data::FireDecision4:
    {
        if (!indirectWeapon)
        {
            redecide = false;
            reset = true;
            break;
        }
        const int phase = data.m_phase;
        if (phase == 0)
            redecide = false;
        else if (phase == 1 && self->getMobile())
            redecide = firingPositionDistance < state->m_arrivalRadius;
        else if (phase == 3)
        {
            redecide = true;
            if (previousWeaponInfo)
            {
                if (detectAimingFailure(static_cast<BotMain*>(bot)))
                    urgencyGenerator->getBBPFeedback(target)->m_time = gameTime();
                else
                    redecide = burstFinished(data, state, targetPhysical, previousWeaponInfo, player);
            }
            reset = redecide;
        }
        break;
    }

    case Data::FireDecision5:
        if (data.m_phase == 0 && !self->getMobile())
            redecide = false;
        else if (data.m_phase == 1)
        {
            redecide = firingPositionDistance < state->m_arrivalRadius;
            if (redecide)
                data.m_history.push_front(data.m_decision);
            reset = redecide;
        }
        break;

    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviourPattern/BBPFireInfantery.cpp", 0x417, "")
            << "I don't belive we should be here!";
        break;
    }
    if (reset)
    {
        data.m_decision = Data::FireDecisionNone;
        data.m_phase = 0;
    }

    // 2. Draw the next decision. This always runs; it replaces the current decision only when a
    // candidate wins its draw or none is set.
    while (static_cast<float>(data.m_history.size()) > 8.0f)
        data.m_history.erase(--data.m_history.end());
    int counts[8];
    for (int i = 0; i < 8; ++i)
        counts[i] = 0;
    for (std::list<Data::FireDecision>::iterator it = data.m_history.begin(); it != data.m_history.end(); ++it)
        ++counts[*it];

    const int candidates[7] = { 1, 2, 3, 4, 7, 6, 5 };
    const float weight = self->getMobile() ? 1.0f : 0.0f;
    const float weights[7] = { 1.0f, 1.0f, 1.0f, weight, weight, weight, weight };   // stored, never read
    (void)weights;

    const bool locked = gameTime() - data.m_lockStartTime < data.m_lockDuration;
    const int decision = data.m_decision;
    const bool moving = (decision == 5 || decision == 7 || decision == 6) && self->getMobile();
    const int indirectCount = counts[2] + counts[3] + counts[4];
    bool lastWasIndirect = false;
    if (indirectCount > 0
        && (data.m_history.front() == 2 || data.m_history.front() == 3 || data.m_history.front() == 4))
        lastWasIndirect = true;
    const bool fewIndirect = indirectCount < 2 && !lastWasIndirect;

    bool chosen = false;
    int choice = 0;
    for (int i = 0; i < 7 && !chosen; ++i)
    {
        const int candidate = candidates[i];
        if (candidate == 0)
            break;
        if (candidate == 2)
        {
            if (!indirectWeapon || locked || (!moving && decision != 0) || (!fewIndirect && directWeapon))
                continue;
            bool clear = false;
            if (targetIsLodBot)
                clear = true;
            else if (objectEnvironment()->hasIndirectLineOfFire(indirectWeapon->getWeapon(),
                                                                indirectWeapon->getPosition(), aimPoint, excluded))
                clear = true;
            const float chance = clear ? 0.5f : 0.05f;
            if (!choice)
                choice = candidate;
            if (randomFloat(1.0f, 0.0f) < chance)
            {
                choice = candidate;
                chosen = true;
            }
        }
        else if (candidate == 1)
        {
            if (!directWeapon || decision == 1)
                continue;
            float chance = 1.0f;
            if (counts[1] > 0 && static_cast<float>(counts[1]) * 0.1f > 1e-05f)
                chance = 1.0f / (static_cast<float>(counts[1]) * 0.1f);
            if (moving)
                chance /= 5.0f;
            if (static_cast<unsigned int>(decision - 2) < 3 && data.m_phase == 3)
                chance = 0.0f;
            if (!choice)
                choice = candidate;
            if (randomFloat(1.0f, 0.0f) < chance)
            {
                choice = candidate;
                chosen = true;
            }
        }
        else if (candidate == 4)
        {
            if (!indirectWeapon || locked || !(moving || decision == 0) || !(fewIndirect || !directWeapon))
                continue;
            const Vec3 shortPoint = { targetPosition.x - direction.x * 5.0f, targetPosition.y - direction.y * 5.0f,
                                      targetPosition.z - direction.z * 5.0f };
            if (!targetIsLodBot
                && !objectEnvironment()->hasIndirectLineOfFire(indirectWeapon->getWeapon(),
                                                               indirectWeapon->getPosition(), shortPoint, excluded))
                continue;
            if (!choice)
                choice = candidate;
            if (randomFloat(1.0f, 0.0f) < 0.5f)
            {
                choice = candidate;
                chosen = true;
            }
        }
    }
    if (chosen || data.m_decision == Data::FireDecisionNone)
    {
        data.m_decision = static_cast<Data::FireDecision>(choice);
        data.m_phase = 0;
        redecide = true;
    }
    data.m_lastUpdateTime = gameTime();

    // 3. Fill the next firing state from the current one (jump table 0x4A00DC: 0, 1, 2, 4, 5).
    FiringState next = *state;
    next.m_target = target;
    next.m_position = selfPosition;
    switch (data.m_decision)
    {
    case Data::FireDecisionNone:
        next.m_hasFiringPosition = false;
        next.m_bool25 = false;
        next.m_weapon = -1;
        break;

    case Data::FireDecision1:
    {
        int phase = data.m_phase;
        bool startBurst = false;
        if (phase == 0)
        {
            redecide = true;
            if (self->getMobile())
            {
                next.m_bool26 = false;
                next.m_shots = 0;
                next.m_bool25 = true;
                next.m_bool14 = true;
                next.m_backOff = false;
                next.m_independentAim = independentAim;
                next.m_hasFiringPosition = false;
                next.m_bool24 = true;
                data.m_phase = 1;
                data.m_phaseTargetPosition = targetPosition;
                next.m_weapon = weapon;
                phase = 1;
            }
            else
                startBurst = true;
        }
        if (phase == 1 || startBurst)
        {
            if (redecide)
            {
                // 0x25DAD9: start a burst of 2..4 shots, or 1..min(magazine, 10) for burst weapons.
                next.m_weapon = weapon;
                if (!directWeapon->getBurst())
                {
                    const float draw = randomFloat(1.0f, 0.0f);
                    next.m_shots = static_cast<int>(3.0f * draw + 2.0f);
                }
                else
                {
                    const float magazine = static_cast<float>(directWeapon->getMaxAmmoInMag());
                    float limit = 10.0f;
                    if (1.0f <= magazine && magazine < 10.0f)
                        limit = magazine;
                    const float range = limit - 1.0f;
                    const float draw = randomFloat(1.0f, 0.0f);
                    next.m_shots = static_cast<int>(draw * range + 1.0f);
                }
                next.m_bool26 = false;
                next.m_bool25 = true;
                next.m_bool14 = self->getMobile() != 0;
                next.m_backOff = false;
                next.m_independentAim = independentAim;
                next.m_hasFiringPosition = false;
                data.m_phase = 3;
                data.m_phaseTargetPosition = targetPosition;
                data.m_shotsAtPhaseStart = player->getNumShotsFired(directWeapon->getWeapon());
                data.m_phaseStartTime = gameTime();
                redecide = false;
            }
            phase = 3;
        }
        bool backOffCheck = phase == 2;
        if (phase == 3)
        {
            // 0x25D7F4 (the image tests isFireAllowed here and again at 0x25D60E).
            next.m_int10 = bot->isFireAllowed() ? next.m_int10 : 1;
            if (!redecide)
                next.m_backOff = false;
            else
                data.m_phase = 0;
            backOffCheck = true;
        }
        if (backOffCheck)
        {
            // 0x25D830: inside the weapon's minimum range - back off.
            const float minRange = directWeapon->getMinRange();
            bool handled = false;
            if (selfMobile)
            {
                const float dx = targetPosition.x - selfPosition.x;
                const float dy = targetPosition.y - selfPosition.y;
                const float dz = targetPosition.z - selfPosition.z;
                if (dx * dx + dy * dy + dz * dz < (minRange + 1.0f) * (minRange + 1.0f))     // 0x25D89D..0x25D8C2
                {
                    const float minRange2 = directWeapon->getMinRange();
                    bool keepPosition = false;
                    if (data.m_phase == 2)
                    {
                        const float fx = targetPosition.x - state->m_firingPosition.x;
                        const float fy = targetPosition.y - state->m_firingPosition.y;
                        const float fz = targetPosition.z - state->m_firingPosition.z;
                        // A squared distance against an unsquared bound, as in the image (0x25D9C2).
                        keepPosition = fx * fx + fy * fy + fz * fz > (minRange2 + minRange2 + 5.0f) * 0.75f;
                    }
                    if (keepPosition)
                    {
                        data.m_firePosition = state->m_position;
                        next.m_position = state->m_position;
                    }
                    else
                    {
                        next.m_position.x = 1.0f;
                        next.m_position.y = 0.0f;
                        next.m_position.z = 1.0f;
                        data.m_firePosition = next.m_position;
                        next.m_backOff = true;
                        data.m_phase = 2;
                    }
                    redecide = false;
                    handled = true;
                }
            }
            if (!handled && data.m_phase == 2)
            {
                data.m_phase = 0;
                next.m_backOff = false;
                redecide = true;
            }
        }
        break;
    }

    case Data::FireDecision2:
    case Data::FireDecision4:
    {
        const bool fromPosition = data.m_decision == Data::FireDecision4;
        int phase = data.m_phase;
        bool start = false;
        if (phase == 0)
        {
            redecide = true;
            if (self->getMobile())
            {
                next.m_bool26 = false;
                next.m_shots = 0;
                next.m_bool25 = false;
                next.m_weapon = weapon;
                if (fromPosition)
                {
                    next.m_bool14 = false;
                    next.m_independentAim = false;
                    const bool found = getIndirectWeaponCloseToFiringPositions(
                        next.m_position, next.m_firingPosition, self, selfMobile, equipmentType, indirectWeapon,
                        targetInfo, targetPosition);
                    next.m_hasFiringPosition = found;
                    next.m_bool24 = false;
                    redecide = !found;
                }
                else
                {
                    next.m_independentAim = independentAim;
                    next.m_bool24 = true;
                }
                data.m_phase = 1;
                data.m_phaseTargetPosition = targetPosition;
                phase = 1;
            }
            else
                start = true;
        }
        if ((phase == 1 && redecide) || start)
        {
            // 0x25DDF6 / 0x25E063: start firing, then lock indirect fire for 5..10 s.
            next.m_bool26 = fromPosition;
            next.m_bool25 = false;
            next.m_shots = 1;
            if (fromPosition)
            {
                next.m_bool14 = false;
                next.m_independentAim = false;
            }
            else
            {
                next.m_bool14 = self->getMobile() != 0;
                next.m_independentAim = independentAim;
                next.m_hasFiringPosition = false;
            }
            data.m_phase = 3;
            data.m_phaseTargetPosition = targetPosition;
            next.m_weapon = weapon;
            data.m_shotsAtPhaseStart = player->getNumShotsFired(indirectWeapon->getWeapon());
            data.m_lockStartTime = gameTime();
            data.m_lockDuration = static_cast<double>(randomFloat(5.0f, 5.0f));
            data.m_phaseStartTime = gameTime();
            redecide = false;
        }
        else if (phase == 3 && redecide)
            data.m_phase = 0;
        break;
    }

    case Data::FireDecision5:
        if (data.m_phase == 0)
        {
            redecide = true;
            if (self->getMobile())
            {
                Vec2 unused = { 0.0f, 0.0f };   // computed and never read in the image (0x25E1F0)
                vec3ToVec2XZ(targetInfo->getPosition(), unused);
                next.m_bool26 = false;
                next.m_shots = 0;
                next.m_independentAim = independentAim;
                next.m_bool14 = true;
                next.m_hasFiringPosition = false;
                next.m_bool25 = true;
                next.m_bool24 = false;
                data.m_phase = 1;
                data.m_phaseTargetPosition = targetPosition;
                redecide = false;
                next.m_weapon = weapon;
            }
            else
                data.m_phase = 0;
        }
        else if (data.m_phase == 1 && redecide)
            data.m_phase = 0;
        break;

    default:
        break;
    }

    // 0x25D606
    next.m_int10 = bot->isFireAllowed() ? next.m_int10 : 1;
    if (!redecide)
    {
        bot->getSquadMember()->clearRequestAction();
        next.m_arrivalRadius = selfPhysical->getRadius() + 2.0f;
        m_firingStates[id] = next;
        return createPlanInternal(bot, targetIsLodBot);
    }
    dropPlan(bot);
    return 0;
}

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

// Component-wise comparison with Vec3::getZero() at FLT_EPSILON, as inlined (0x276E3A, 0x267A0B).
bool isZero(const Vec3& v)
{
    const Vec3& zero = Vec3::getZero();
    return std::fabs(v.x - zero.x) <= 1.1920929e-07f && std::fabs(v.y - zero.y) <= 1.1920929e-07f
        && std::fabs(v.z - zero.z) <= 1.1920929e-07f;
}

// The inlined normalisation (0x2653F0..): a unit vector is kept, a near-zero one becomes zero.
void normalize(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSq - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lengthSq) < 1.1920929e-07f)
    {
        v.x = v.y = v.z = 0.0f;
        return;
    }
    const float inv = 1.0f / std::sqrt(lengthSq);
    v.x *= inv;
    v.y *= inv;
    v.z *= inv;
}

// `1 << input` sign-extended to the 64-bit control mask (`movl %eax,%edx; sarl $0x1f,%edx`).
unsigned long long inputMask(int input)
{
    return static_cast<unsigned long long>(static_cast<long long>(1 << input));
}

// Every branch starts its parallel block with the sensing-agent override; bots whose own
// object has type flag 0x200000 only override while not sprinting.
BAPStatement* newOverrideSensingAgent(bool selfFlag200000, int patternId)
{
    if (selfFlag200000)
        return checkPointer(new BAPFlowCIf(checkPointer(new BAPConNot(checkPointer(new BAPConIsSprinting()))),
                                           checkPointer(new BAPAOverrideSensingAgent()), 0, patternId, true, false));
    return checkPointer(new BAPAOverrideSensingAgent());
}

// "Fire while the target lives": And{ Not(ObjectHealth(target, 0)) }.
BAPCondition* newTargetAlive(unsigned int target)
{
    BAPConAnd* const alive = checkPointer(new BAPConAnd());
    alive->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConObjectHealth(target, 0.0f)))));
    return alive;
}

float atLeast(float value, float minimum)
{
    return value <= minimum ? minimum : value;
}

} // namespace

// ELF 0x2650F0 (18,046 instructions). Builds the fire plan for the FiringState createPlan filled:
// a serial root { change weapon, parallel(reset controls, sensing override, the branch's block) }
// wrapped in a BAPIWFire. Branches (FiringState flags):
//   m_bool14 && m_independentAim                 move and fire (infantry; 0x265B1A)       recovered
//   m_bool14 && !independent && weapon type 4    place and detonate (0x26B5xx)             recovered
//   m_bool14 && !independent, other weapons      move then fire (0x271F4A)                 recovered
//   !m_bool14 && m_hasFiringPosition             go to the firing position, fire           recovered
//   otherwise / no mobile                        fire from here (0x2709A0)                 recovered
BotScript* BBPFireInfantery::createPlanInternal(Bot* bot, bool targetIsLodBot)
{
    const unsigned int id = bot->getBotId();
    FiringState* const state = &m_firingStates[id];
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(gameTime());
    dropPlan(bot);

    const unsigned int self = bot->getCurrentEquipment();
    Information* const selfInfo = informationManager->getInformation(self);
    IPIArmament* const armament = selfInfo->getArmament();
    IPIPhysical* const selfPhysical = selfInfo->getPhysical();
    IPIControlInfo* const controlInfo = selfInfo->getControlInfo();
    if (!armament || !selfPhysical || !controlInfo)
        return 0;
    IPIMobile* const selfMobile = selfInfo->getMobile();
    if (selfMobile)
        selfMobile->isTurnable();          // result unused in the image (0x265258)
    controlInfo->getDriveControls();       // results unused in the image (0x265266 / 0x265272)
    controlInfo->getAimControls();
    const Vec3& selfPosition = selfInfo->getPosition();

    const unsigned int target = state->m_target;
    Information* const targetInfo = informationManager->getInformation(target);
    if (!targetInfo)
        return 0;
    IPIPhysical* const targetPhysical = targetInfo->getPhysical();
    if (!targetPhysical)
        return 0;

    WeaponInfo* weaponInfo = 0;
    if (m_weapons[bot->getBotId()] != -1)
    {
        weaponInfo = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(m_weapons[bot->getBotId()]));
        armament->setActiveWeapon(m_weapons[bot->getBotId()], bot->getSide());
    }

    // The target, or its primary object, can move.
    bool targetMobile = targetInfo->getMobile() != 0;
    if (!targetMobile && !targetInfo->isPrimary())
        targetMobile = informationManager->getInformation(targetInfo->getPrimaryObject(bot->getSide()))->getMobile() != 0;

    // A reachable spot 30 m from the target on the bot's side (0x265300..0x2653D1).
    bool pathFound = true;
    Vec3 movePosition = { 0.0f, 0.0f, 0.0f };
    if (selfMobile && !targetMobile)
    {
        const Vec3& from = targetInfo->getPosition();
        const Vec3& own = selfInfo->getPosition();
        Vec3 direction = { own.x - from.x, own.y - from.y, own.z - from.z };
        normalize(direction);
        const Vec3 point = { direction.x * 30.0f + from.x, direction.y * 30.0f + from.y, direction.z * 30.0f + from.z };
        const int equipmentType = selfInfo->getUnit()->getEquipmentType();
        unsigned int polygon;
        unsigned int cluster;
        pathFound = IAIPathfinding::instance->findClosestValidPolygon(polygon, cluster, movePosition,
                                                                      selfMobile->getVehicleNumber(), from, point,
                                                                      10, equipmentType);
    }

    const int guidance = weaponInfo->getWeaponGuidanceType();
    if (guidance != 0 && (selfInfo->getObjectTypeFlags() & 0x200000))
        return createGuidedPlanInternalInfantry(bot, targetIsLodBot, movePosition);   // 0x2779B1

    // The plan variables (0x26543B..0x265819), shared by the wrappers and the fire plans.
    BAPVariable<WeaponInfo*>* const weaponVariable = newVariable<WeaponInfo*>(context, weaponInfo);
    BAPVariable<unsigned int>* const targetVariable = newVariable<unsigned int>(context, target);
    BAPVariable<bool>* const aimValidVariable = newVariable<bool>(context, false);
    const Vec3 up = { 0.0f, 1.0f, 0.0f };
    const Vec3 zero = { 0.0f, 0.0f, 0.0f };
    BAPVariable<Vec3>* const aimVariable = newVariable<Vec3>(context, up);
    BAPVariable<Vec3>* const aimOffsetVariable = newVariable<Vec3>(context, zero);
    BAPVariable<Vec3>* const lookVariable = newVariable<Vec3>(context, zero);

    BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, 0));
    if (m_weapons[bot->getBotId()] != -1 && weaponInfo->getWeaponActivate() != 0x40)
        root->addStatement(checkPointer(new BAPAChangeWeapon(bot, m_weapons[bot->getBotId()], m_patternId)));
    BAPCombinerParallel* const block = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
    root->addStatement(block);
    block->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_patternId)));

    const bool placedExplosive = weaponInfo->getWeaponType() == 4;
    const int antiAircraft = armament->isAntiAircraft();
    const bool targetFlag10 = (targetInfo->getObjectTypeFlags() & 0x10) != 0;
    const bool noMove = targetFlag10 && antiAircraft == 1;                     // -0x246
    bool fireFromHere = (targetFlag10 && antiAircraft == 2) || !pathFound;     // -0x247
    if (weaponInfo->getMaxRange() < 10.0f)
        fireFromHere = false;

    // The squad's strategic-area position, when the squad has a mission (0x277055).
    SquadMember* const squadMember = bot->getSquadMember();
    bool squadPositionValid = false;
    Vec3 squadPosition = { 0.0f, 0.0f, 0.0f };
    if (squadMember && squadMember->getSquad() && squadMember->getSquad()->squadHasMission()
        && squadMember->getSquad()->getValidSaPos(squadMember, squadPosition))
        squadPositionValid = true;

    const bool selfFlag = (selfInfo->getObjectTypeFlags() & 0x200000) != 0;       // -0x249
    const bool targetFlag = (targetInfo->getObjectTypeFlags() & 0x200000) != 0;   // -0x24A
    const bool targetIsBot = IBotManager::instance->getBotFromPlayerId(targetInfo->getPlayerId()) != 0;

    BAPStatement* branch = 0;   // added to the parallel block at 0x2671C0
    bool stationary = false;    // 0x2709A0
    if (!fireFromHere && m_targetChanged[id])
    {
        if (!selfMobile)
            stationary = true;
        else if (squadPositionValid && !isZero(squadPosition))
        {
            // 0x276E2B: fire from here when the squad position lies behind the bot, away from a
            // target beyond 30 m (50 m unless either side has type flag 0x200000).
            const Vec3 toSquad = { squadPosition.x - selfPosition.x, squadPosition.y - selfPosition.y,
                                   squadPosition.z - selfPosition.z };
            const Vec3& targetPosition = targetInfo->getPosition();
            const Vec3 toTarget = { targetPosition.x - selfPosition.x, targetPosition.y - selfPosition.y,
                                    targetPosition.z - selfPosition.z };
            const float range = (selfFlag || targetFlag) ? 30.0f : 50.0f;
            if (range * range < toTarget.z * toTarget.z + toTarget.y * toTarget.y + toTarget.x * toTarget.x
                && toTarget.z * toSquad.z + toTarget.x * toSquad.x + toTarget.y * toSquad.y < 0.0f)
                fireFromHere = true;
        }
    }

    // 0x265AF0
    if (!stationary && !selfMobile)
        stationary = true;
    if (!stationary && state->m_bool14 && state->m_independentAim)
    {
        // ---- Move and fire (0x265B1A..0x2671BB) ----
        if (!fireFromHere)
        {
            if (IPIMobile* const targetMover = targetInfo->getMobile())
            {
                const IPIMobile::ValidPos& valid = targetMover->getLastValidPosition(selfMobile->getVehicleNumber());
                if (valid.quadTree == static_cast<unsigned int>(-1) || valid.polygon == static_cast<unsigned int>(-1))
                    fireFromHere = true;
            }
            if (!fireFromHere && targetIsBot && targetFlag)
                fireFromHere = true;
        }
        const bool splash = weaponInfo->getExplosionRadius() != 0.0f && guidance == 0;
        const bool staticAttack = static_cast<BotMain*>(bot)->getUseStaticAttack() && splash && targetFlag;
        block->addStatement(newOverrideSensingAgent(selfFlag, m_patternId));

        BAPConSharedCondition* const lineOfFire =
            staticAttack ? checkPointer(new BAPConSharedCondition(checkPointer(new BAPConTrue())))
                         : checkPointer(new BAPConSharedCondition(checkPointer(new BAPConObjectLineOfFire(target, targetIsLodBot))));

        // A control point close to the bot: fire at any range while it is not ours (0x265D8A).
        BAPConSharedCondition* controlPointCondition = 0;
        AIStrategicObject* const object =
            AIStrategicObjectManager::getInstance()->getClosestObject(selfPosition, bot->getSide(), false);
        Vec3 controlPoint = { 0.0f, 0.0f, 0.0f };
        if (object && !noMove && !fireFromHere
            && static_cast<AIStrategicArea*>(object)->m_controlPointId != static_cast<unsigned int>(-1))
        {
            const int equipmentType = selfInfo->getUnit()->getEquipmentType();
            const int vehicleNumber = selfMobile->getVehicleNumber();
            static_cast<AIStrategicArea*>(object)->getControlpointPos(object, bot->getSide(), vehicleNumber,
                                                                     equipmentType, selfPosition, controlPoint);
            const float radius = object->m_size[bot->getSide()];
            Vec3 objectPosition = { 0.0f, 0.0f, 0.0f };
            vec2ToVec3XZ(object->getPosition(bot->getSide()), objectPosition);
            objectPosition.y = controlPoint.y;
            const Vec3 offset = { objectPosition.x - controlPoint.x, objectPosition.y - controlPoint.y,
                                  objectPosition.z - controlPoint.z };
            if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z < radius * radius)
            {
                // 0x26A7A5 also computes controlPoint - selfPosition into a slot it never reads.
                BAPConAnd* const near = checkPointer(new BAPConAnd());
                near->addCondition(checkPointer(new BAPConStrategicObjectStatus(object, bot->getSide(), false)));
                near->addCondition(checkPointer(new BAPConPointDistance(controlPoint, 40.0f, true)));
                controlPointCondition = checkPointer(new BAPConSharedCondition(near));
            }
        }

        BAPConAnd* const fireCondition = checkPointer(new BAPConAnd());
        fireCondition->addCondition(lineOfFire->getNewSharedCondition());
        if (!controlPointCondition)
            fireCondition->addCondition(checkPointer(new BAPConObjectDistance(
                target, weaponInfo->getMinRange(), weaponInfo->getMaxRange(), true, false)));
        else
        {
            BAPConOr* const inRange = checkPointer(new BAPConOr());
            inRange->addCondition(checkPointer(new BAPConObjectDistance(
                target, weaponInfo->getMinRange(), weaponInfo->getMaxRange(), true, false)));
            inRange->addCondition(controlPointCondition->getNewSharedCondition());
            fireCondition->addCondition(inRange);
        }
        BAPConSharedCondition* const validAiming = checkPointer(new BAPConSharedCondition(checkPointer(
            new BAPConObjectValidAiming(target, self, aimValidVariable, aimVariable, staticAttack))));

        BAPStatement* fireOrAim;
        if (guidance == 0)
        {
            BAPStatement* const firePlan = createFirePlan(bot, state, weaponVariable, targetVariable, aimValidVariable,
                                                          aimVariable, aimOffsetVariable, lookVariable, weaponInfo,
                                                          selfInfo, controlInfo, targetInfo, targetPhysical, WGSNone);
            const Vec3 noOffset = { 0.0f, 0.0f, 0.0f };
            BAPAAimAtObject* const aim = checkPointer(new BAPAAimAtObject(
                bot, target, noOffset, checkPointer(new BAPConFalse()), controlInfo->getAimControls(), m_patternId,
                false, aimValidVariable, aimVariable));
            fireOrAim = checkPointer(new BAPFlowCIf(fireCondition, firePlan, aim, m_patternId, true, false));
        }
        else
        {
            BAPStatement* const firePlan = createFirePlanGuided(bot, state, weaponVariable, targetVariable, lookVariable,
                                                                weaponInfo, selfInfo, controlInfo, targetInfo,
                                                                targetPhysical, static_cast<WeaponGuidingSystem>(guidance));
            BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
                bot, target, 0.0f, checkPointer(new BAPConFalse()), controlInfo->getLookControls(), m_patternId,
                lookVariable, 0.0f));
            fireOrAim = checkPointer(new BAPFlowCIf(fireCondition, firePlan, look, m_patternId, true, false));
        }

        BAPCombinerParallel* const engage = checkPointer(new BAPCombinerParallel(m_patternId, 4, false));
        engage->addStatement(checkPointer(new BAPAEvaluateCondition(lineOfFire, m_patternId)));
        engage->addStatement(checkPointer(new BAPAEvaluateCondition(validAiming, m_patternId)));
        if (guidance == 0)
            engage->addStatement(checkPointer(new BAPWrapperAiming(aimVariable, aimValidVariable, aimOffsetVariable,
                                                                   self, target, true)));
        else
            engage->addStatement(checkPointer(new BAPWrapperLookAt(target, lookVariable, true, guidance == 3,
                                                                   aimVariable, aimValidVariable)));

        // Movement while engaging (0x2664xx..0x266Fxx).
        BAPStatement* movement = 0;
        if (selfMobile && !noMove && !fireFromHere)
        {
            // No line of fire: walk to within 1.1 x minimum range (>= 15 m), forget the target, end.
            BAPStatement* away;
            if (targetMobile)
                away = checkPointer(new BAPAMoveToObjectFinding(
                    bot, state->m_target, 0.0f, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                    controlInfo->getDriveControls(), m_patternId, 0, atLeast(weaponInfo->getMinRange() * 1.1f, 15.0f), 3));
            else
                away = checkPointer(new BAPAMoveToFinding(
                    bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                    controlInfo->getDriveControls(), m_patternId, 0.0f, atLeast(weaponInfo->getMinRange() * 1.1f, 15.0f)));
            BAPCombinerSerial* const lost = checkPointer(new BAPCombinerSerial(m_patternId, 300));
            lost->addStatement(away);
            lost->addStatement(checkPointer(new BAPAForgetTarget(state->m_target)));
            lost->addStatement(checkPointer(new BAPFlowCEnd(m_patternId, static_cast<BAPFlowCEnd::EndType>(0), 0)));

            // Aim valid: close in while beyond half the maximum range or inside 1.05 x minimum (>= 18 m).
            BAPConOr* const approachCondition = checkPointer(new BAPConOr());
            approachCondition->addCondition(checkPointer(new BAPConObjectDistance(target, weaponInfo->getMaxRange() * 0.5f, true, false, true)));
            approachCondition->addCondition(checkPointer(new BAPConObjectDistance(target, atLeast(weaponInfo->getMinRange() * 1.05f, 18.0f), true, false, false)));
            BAPStatement* approachMove;
            if (targetMobile)
                approachMove = checkPointer(new BAPAMoveToObjectFinding(
                    bot, state->m_target, weaponInfo->getMaxRange() * 0.35f, selfMobile->getMaxSpeed(),
                    selfMobile->getMaxSpeed() * 0.5f, 0, controlInfo->getDriveControls(), m_patternId, 0,
                    atLeast(weaponInfo->getMinRange() * 1.1f, 15.0f), 4));
            else
                approachMove = checkPointer(new BAPAMoveToFinding(
                    bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                    controlInfo->getDriveControls(), m_patternId, weaponInfo->getMaxRange() * 0.35f,
                    atLeast(weaponInfo->getMinRange() * 1.05f, 15.0f)));
            BAPFlowCIf* const approach = checkPointer(new BAPFlowCIf(approachCondition, approachMove, 0, m_patternId, false, false));

            // Aim not valid: reposition, near (beyond max(minimum, 15) + 5 m) or far.
            BAPConObjectDistance* const beyond = checkPointer(new BAPConObjectDistance(
                target, atLeast(weaponInfo->getMinRange(), 15.0f) + 5.0f, true, false, true));
            BAPStatement* nearMove;
            BAPStatement* farMove;
            if (targetMobile)
            {
                nearMove = checkPointer(new BAPAMoveToObjectFinding(
                    bot, state->m_target, atLeast(weaponInfo->getMinRange() * 1.1f, 10.0f), selfMobile->getMaxSpeed(),
                    selfMobile->getMaxSpeed() * 0.5f, 0, controlInfo->getDriveControls(), m_patternId, 0, 0.0f, 5));
                BAPConAnd* const moved = checkPointer(new BAPConAnd());
                moved->addCondition(checkPointer(new BAPConObjectDistance(m_targets[bot->getBotId()], 50.0f, true, true, false)));
                moved->addCondition(checkPointer(new BAPConObjectMoved(m_targets[bot->getBotId()], 5.0f)));
                farMove = checkPointer(new BAPAMoveToObjectFinding(
                    bot, state->m_target, weaponInfo->getMaxRange() * 0.7f, selfMobile->getMaxSpeed(),
                    selfMobile->getMaxSpeed() * 0.5f, 0, controlInfo->getDriveControls(), m_patternId, moved,
                    atLeast(weaponInfo->getMinRange() * 2.2f, 30.0f), 6));
            }
            else
            {
                nearMove = checkPointer(new BAPAMoveToFinding(
                    bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                    controlInfo->getDriveControls(), m_patternId, atLeast(weaponInfo->getMinRange() * 1.1f, 10.0f), 0.0f));
                farMove = checkPointer(new BAPAMoveToFinding(
                    bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                    controlInfo->getDriveControls(), m_patternId, weaponInfo->getMaxRange() * 0.7f,
                    atLeast(weaponInfo->getMinRange() * 2.2f, 30.0f)));
            }
            BAPFlowCIf* const reposition = checkPointer(new BAPFlowCIf(beyond, nearMove, farMove, m_patternId, false, false));
            BAPFlowCIf* const aimed = checkPointer(new BAPFlowCIf(
                checkPointer(new BAPConNot(validAiming->getNewSharedCondition())), reposition, approach, m_patternId,
                false, false));
            movement = checkPointer(new BAPFlowCIf(checkPointer(new BAPConNot(lineOfFire->getNewSharedCondition())),
                                                   lost, aimed, m_patternId, true, false));
        }
        if (!controlPointCondition)
        {
            if (movement)
                engage->addStatement(movement);
            else if (fireFromHere && squadPositionValid && !isZero(squadPosition))
            {
                static_cast<BotMain*>(bot)->setIsGoingForFlag(true);   // 0x267A42
                engage->addStatement(checkPointer(new BAPAMoveToFinding(
                    bot, squadPosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                    controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f)));
            }
        }
        else
        {
            BAPAMoveToFinding* const toControlPoint = checkPointer(new BAPAMoveToFinding(
                bot, controlPoint, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f));
            engage->addStatement(checkPointer(new BAPFlowCIf(controlPointCondition, toControlPoint, movement,
                                                             m_patternId, false, false)));
        }
        engage->addStatement(fireOrAim);

        branch = checkPointer(new BAPFlowCWhile(newTargetAlive(target), engage, false, m_patternId));
        if (!selfFlag)
            block->addStatement(checkPointer(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, m_patternId)));
    }
    else if (!stationary && state->m_bool14 && placedExplosive)
    {
        // ---- Place and detonate (weapon type 4; 0x26B5xx..) ----
        block->addStatement(checkPointer(new BAPFlowCIf(
            checkPointer(new BAPConNot(checkPointer(new BAPConIsSprinting()))),
            checkPointer(new BAPAOverrideSensingAgent()), 0, m_patternId, true, false)));
        BAPCombinerParallel* const engage = checkPointer(new BAPCombinerParallel(m_patternId, 4, false));
        branch = checkPointer(new BAPFlowCWhile(newTargetAlive(target), engage, false, m_patternId));
        const Vec3 noOffset = { 0.0f, 0.0f, 0.0f };
        engage->addStatement(checkPointer(new BAPAAimAtObject(bot, target, noOffset, checkPointer(new BAPConFalse()),
                                                              controlInfo->getAimControls(), m_patternId, false, 0, 0)));
        BAPCombinerSerial* const steps = checkPointer(new BAPCombinerSerial(m_patternId, 1));
        const float contact = targetPhysical->getSmallestRadius() + selfPhysical->getSmallestRadius();

        // 1. Walk up to the target.
        BAPStatement* walkUp;
        if (targetMobile)
        {
            steps->addStatement(checkPointer(new BAPAMoveToObjectFinding(
                bot, state->m_target, contact, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                controlInfo->getDriveControls(), m_patternId, 0, 0.0f, 1)));
            BAPAMoveToObjectFinding* const follow = checkPointer(new BAPAMoveToObjectFinding(
                bot, state->m_target, 0.0f, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                controlInfo->getDriveControls(), m_patternId, 0, 0.0f, 3));
            walkUp = checkPointer(new BAPFlowCWhile(
                checkPointer(new BAPConNot(checkPointer(new BAPConTime(0.20000000298023224, false, false, false)))),
                follow, false, m_patternId));
        }
        else
            walkUp = checkPointer(new BAPAMoveToFinding(
                bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f));
        steps->addStatement(walkUp);

        // 2. Place: alternate fire while the detonator is active, then fire, then lock turning.
        const int altFire = weaponInfo->getWeaponAltFire();
        steps->addStatement(checkPointer(new BAPFlowCWhile(
            checkPointer(new BAPConWeaponDetonatorIsActive(weaponVariable)),
            checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(altFire), checkPointer(new BAPConTrue()),
                                         checkPointer(new BAPConFalse()), inputMask(altFire), m_patternId, 0.0, true)),
            false, m_patternId)));
        const int fire = weaponInfo->getWeaponFire();
        steps->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(fire), checkPointer(new BAPConTrue()),
                                                         checkPointer(new BAPConFalse()), inputMask(fire), m_patternId, 0.0, true)));
        steps->addStatement(checkPointer(new BAPICTurnLock(true)));

        // 3. Keep close until the weapon is ready again, then back off 30 m.
        BAPStatement* stay;
        if (targetMobile)
            stay = checkPointer(new BAPAMoveToObjectFinding(
                bot, state->m_target, 0.0f, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                controlInfo->getDriveControls(), m_patternId, 0, 0.0f, 2));
        else
            stay = checkPointer(new BAPAMoveToFinding(
                bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f));
        steps->addStatement(checkPointer(new BAPFlowCWhile(
            checkPointer(new BAPConNot(checkPointer(new BAPConWeaponReadyToFire(weaponVariable)))), stay, false, m_patternId)));
        BAPStatement* backOff;
        if (targetMobile)
            backOff = checkPointer(new BAPAMoveToObjectFinding(
                bot, state->m_target, 30.0f, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                controlInfo->getDriveControls(), m_patternId, 0, contact + 9.0f, 2));
        else
            backOff = checkPointer(new BAPAMoveToFinding(
                bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, 30.0f, contact + 9.0f));
        steps->addStatement(backOff);

        // 4. Crouch (pose 2) and detonate: alternate fire until the detonator is active, wait 0.7 s, fire.
        BAPCombinerParallel* const detonate = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        detonate->addStatement(checkPointer(new BAPASoldierPose(SPCrouching, checkPointer(new BAPConTrue()), m_patternId, true)));
        BAPFlowCWhile* const arm = checkPointer(new BAPFlowCWhile(
            checkPointer(new BAPConNot(checkPointer(new BAPConWeaponDetonatorIsActive(weaponVariable)))),
            checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(altFire), checkPointer(new BAPConTrue()),
                                         checkPointer(new BAPConFalse()), inputMask(altFire), m_patternId, 0.0, false)),
            false, m_patternId));
        BAPCombinerSerial* const sequence = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        sequence->addStatement(arm);
        sequence->addStatement(checkPointer(new BAPAIdle(
            checkPointer(new BAPConTime(0.699999988079071, false, false, false)), 0, m_patternId)));
        sequence->addStatement(checkPointer(new BAPATrigger(
            static_cast<io::PlayerInputMap>(fire), checkPointer(new BAPConWeaponDetonatorIsActive(weaponVariable)),
            checkPointer(new BAPConFalse()), inputMask(fire), m_patternId, 0.0, false)));
        detonate->addStatement(sequence);
        steps->addStatement(detonate);
        steps->addStatement(checkPointer(new BAPICTurnLock(false)));
        engage->addStatement(steps);
        engage->addStatement(checkPointer(new BAPAMergeSoldierInputs(m_patternId)));
    }
    else if (!stationary && state->m_bool14)
    {
        // ---- Move then fire, drive and aim controls overlapping (0x271F4A..0x272B1F, with the
        // out-of-line blocks 0x273020.., 0x273C3A..0x27422C, 0x2754D0, 0x2759CA, 0x275C5A, 0x275DC0,
        // 0x2762A1, 0x2763B1, 0x276504, 0x276D76..0x276E26) ----
        const bool splash = weaponInfo->getExplosionRadius() != 0.0f && guidance == 0;   // -0x5D5
        SensedObject* const sensed = bot->getSensedObjectFromHandle(target);
        bool targetAbove = false;   // -0x5D6
        if (bot->getSpawnKit() != 1 && !weaponInfo->getIsThrown())
        {
            if (30.0f < targetInfo->getPosition().y - selfInfo->getPosition().y)
                targetAbove = true;
        }
        bool grenadeChecks = false;   // -0x5D7 (no splash-at-target term, unlike createFirePlan)
        if (static_cast<BotMain*>(bot)->getUseStaticAttack() && splash && targetFlag
            && ((sensed->m_flags & 8) || targetAbove))
            grenadeChecks = true;

        // Ranges: a target more than 5 m above adds 1.25 x the height to the maximum (not for
        // thrown weapons, only beyond 10 m); thrown weapons in static attack keep 5 m minimum.
        const float dy = targetInfo->getPosition().y - selfInfo->getPosition().y;   // -0x5DC
        float heightBonus = 0.0f;                                                     // -0x5E0
        if (dy > 5.0f && weaponInfo->getMaxRange() > 10.0f && !weaponInfo->getIsThrown())
            heightBonus = dy * 1.25f;
        const float maxRange = weaponInfo->getMaxRange() + heightBonus;               // -0x5E4
        float minRange = weaponInfo->getMinRange();                                   // -0x5E8
        if (dy > 3.0f && grenadeChecks && weaponInfo->getIsThrown())
            minRange = 5.0f;   // 0x276D8C cmovne
        block->addStatement(newOverrideSensingAgent(selfFlag, m_patternId));

        // The fire condition: the shared "can hit" And (valid aiming when the bot will fire,
        // line of fire unless grenade checks), then the range (or a control point nearby).
        BAPConAnd* const fireCondition = checkPointer(new BAPConAnd());   // -0x604
        BAPConAnd* const canHit = checkPointer(new BAPConAnd());
        if (m_weapons[bot->getBotId()] != -1 && state->m_shots != 0 && guidance == 0)
            canHit->addCondition(checkPointer(new BAPConObjectValidAiming(target, self, aimValidVariable, aimVariable,
                                                                          grenadeChecks)));   // 0x276504
        if (!grenadeChecks)
            canHit->addCondition(checkPointer(new BAPConObjectLineOfFire(target, targetIsLodBot)));   // 0x2763B1
        BAPConSharedCondition* const canHitShared = checkPointer(new BAPConSharedCondition(canHit));   // -0x61C
        fireCondition->addCondition(canHitShared->getNewSharedCondition());

        BAPConSharedCondition* controlPointCondition = 0;   // -0x620
        AIStrategicObject* const object =
            AIStrategicObjectManager::getInstance()->getClosestObject(selfPosition, bot->getSide(), false);
        Vec3 controlPoint = { 0.0f, 0.0f, 0.0f };
        if ((!selfFlag || weaponInfo->getMaxRange() > 5.0f) && object && !fireFromHere
            && static_cast<AIStrategicArea*>(object)->m_controlPointId != static_cast<unsigned int>(-1))
        {
            const int equipmentType = selfInfo->getUnit()->getEquipmentType();
            const int vehicleNumber = selfMobile->getVehicleNumber();
            static_cast<AIStrategicArea*>(object)->getControlpointPos(object, bot->getSide(), vehicleNumber,
                                                                     equipmentType, selfPosition, controlPoint);
            const float radius = object->m_size[bot->getSide()];
            Vec3 objectPosition = { 0.0f, 0.0f, 0.0f };
            vec2ToVec3XZ(object->getPosition(bot->getSide()), objectPosition);
            objectPosition.y = controlPoint.y;
            const Vec3 offset = { objectPosition.x - controlPoint.x, objectPosition.y - controlPoint.y,
                                  objectPosition.z - controlPoint.z };
            if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z < radius * radius)
            {
                // 0x275DC0 also computes controlPoint - selfPosition into a slot it never reads.
                BAPConAnd* const near = checkPointer(new BAPConAnd());
                near->addCondition(checkPointer(new BAPConStrategicObjectStatus(object, bot->getSide(), false)));
                near->addCondition(checkPointer(new BAPConPointDistance(controlPoint, 40.0f, true)));
                controlPointCondition = checkPointer(new BAPConSharedCondition(near));
            }
        }
        if (!grenadeChecks)
        {
            if (!controlPointCondition)
                fireCondition->addCondition(checkPointer(new BAPConObjectDistance(
                    target, minRange * 0.9f, maxRange * 1.1f, true, false)));   // 0x275C5A
            else
            {
                BAPConOr* const inRange = checkPointer(new BAPConOr());
                inRange->addCondition(checkPointer(new BAPConObjectDistance(
                    target, minRange * 0.9f, maxRange * 1.1f, true, false)));
                inRange->addCondition(controlPointCondition->getNewSharedCondition());
                fireCondition->addCondition(inRange);
            }
        }

        // Fire (createFirePlan with the weapon's guidance) or keep aiming (0x272573..0x2726B6).
        BAPStatement* firePlan = 0;   // -0x654
        if (m_weapons[bot->getBotId()] != -1 && state->m_shots != 0)
            firePlan = createFirePlan(bot, state, weaponVariable, targetVariable, aimValidVariable, aimVariable,
                                      aimOffsetVariable, lookVariable, weaponInfo, selfInfo, controlInfo, targetInfo,
                                      targetPhysical, static_cast<WeaponGuidingSystem>(guidance));   // 0x275A5C
        const Vec3 noOffset = { 0.0f, 0.0f, 0.0f };
        BAPAAimAtObject* const aim = checkPointer(new BAPAAimAtObject(
            bot, target, noOffset, checkPointer(new BAPConFalse()), controlInfo->getAimControls(), m_patternId, false,
            aimValidVariable, aimVariable));
        BAPFlowCIf* const fireOrAim = checkPointer(new BAPFlowCIf(fireCondition, firePlan, aim, m_patternId, true, false));

        BAPCombinerParallel* const engage = checkPointer(new BAPCombinerParallel(m_patternId, 4, false));   // -0x674
        if (guidance == 0)
            engage->addStatement(checkPointer(new BAPWrapperAiming(aimVariable, aimValidVariable, aimOffsetVariable,
                                                                   self, target, true)));
        else
            engage->addStatement(checkPointer(new BAPWrapperLookAt(target, lookVariable, true, guidance == 3,
                                                                   aimVariable, aimValidVariable)));   // 0x2754D0

        // Movement (0x273C3A..0x27422C): no line of fire or fire not allowed -> walk up to
        // 1.1 x minimum range, forget the target, end; otherwise keep the distance at
        // min + (max - min) x optimal range (randomised 0.8..1.2 beyond 10 m).
        BAPStatement* movement = 0;   // -0x680
        if (selfMobile && !fireFromHere)
        {
            BAPConOr* const noFire = checkPointer(new BAPConOr());   // -0x684
            if (!grenadeChecks)
                noFire->addCondition(checkPointer(new BAPConNot(canHitShared->getNewSharedCondition())));
            else
                noFire->addCondition(checkPointer(new BAPConFalse()));   // 0x2751C0
            noFire->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConAllowFire()))));

            BAPStatement* away;
            if (targetMobile)   // -0x695, recomputed as before
                away = checkPointer(new BAPAMoveToObjectFinding(
                    bot, state->m_target, 0.0f, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                    controlInfo->getDriveControls(), m_patternId, 0, minRange * 1.1f, 7));
            else
                away = checkPointer(new BAPAMoveToFinding(
                    bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                    controlInfo->getDriveControls(), m_patternId, 0.5f, minRange * 1.1f));   // 0x274CD7
            BAPCombinerSerial* const lost = checkPointer(new BAPCombinerSerial(m_patternId, 300));   // -0x6B4
            lost->addStatement(away);
            lost->addStatement(checkPointer(new BAPAForgetTarget(state->m_target)));
            lost->addStatement(checkPointer(new BAPFlowCEnd(m_patternId, static_cast<BAPFlowCEnd::EndType>(0), 0)));

            float optimal = weaponInfo->getOptimalRangePercentage() * 0.01f;   // -0x6C4
            if (weaponInfo->getMaxRange() > 10.0f)
                optimal = (randomFloat(0.4f, 0.0f) + 0.8f) * optimal;
            float desired = (maxRange - minRange) * optimal + minRange;   // -0x6CC
            if (grenadeChecks && !weaponInfo->getIsThrown())
            {
                const float height = targetInfo->getPosition().y - selfInfo->getPosition().y;
                if (height > 0.0f)
                    desired += height * 0.25f;   // 0x2748D6
            }
            BAPStatement* keep;   // -0x6D0
            if (targetMobile)
                keep = checkPointer(new BAPAMoveToObjectFinding(
                    bot, state->m_target, desired, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                    controlInfo->getDriveControls(), m_patternId, 0, minRange * 1.1f, 8));
            else
                keep = checkPointer(new BAPAMoveToFinding(
                    bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                    controlInfo->getDriveControls(), m_patternId, desired, minRange * 1.1f));   // 0x2746D1
            BAPConObjectDistance* const inBand = checkPointer(new BAPConObjectDistance(
                target, minRange, desired * 1.1f, true, false));
            BAPFlowCIf* const approach = checkPointer(new BAPFlowCIf(checkPointer(new BAPConNot(inBand)), keep, 0,
                                                                     m_patternId, false, false));
            movement = checkPointer(new BAPFlowCIf(noFire, lost, approach, m_patternId, true, true));
        }

        // Reload once the magazine is empty (0x2727A5; no auto-reload check here).
        BAPConWeaponMagAmmo* const empty = checkPointer(new BAPConWeaponMagAmmo(
            weaponVariable, newVariable<int>(context, 0), false));
        BAPATrigger* const reload = checkPointer(new BAPATrigger(
            static_cast<io::PlayerInputMap>(0x20), checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
            0x100000000ULL, m_patternId, 0.0, false));
        engage->addStatement(checkPointer(new BAPFlowCIf(empty, reload, 0, m_patternId, true, false)));
        engage->addStatement(checkPointer(new BAPAEvaluateCondition(canHitShared, m_patternId)));

        if (controlPointCondition && !fireFromHere)
        {
            BAPAMoveToFinding* const toControlPoint = checkPointer(new BAPAMoveToFinding(
                bot, controlPoint, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f));   // 0x27329B
            engage->addStatement(checkPointer(new BAPFlowCIf(controlPointCondition, toControlPoint, movement,
                                                             m_patternId, false, false)));
        }
        else if (movement)
            engage->addStatement(movement);
        else if (fireFromHere && squadPositionValid && !isZero(squadPosition))
        {
            static_cast<BotMain*>(bot)->setIsGoingForFlag(true);   // 0x2730D8
            engage->addStatement(checkPointer(new BAPAMoveToFinding(
                bot, squadPosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f)));
        }
        engage->addStatement(fireOrAim);
        if (selfFlag)
            engage->addStatement(checkPointer(new BAPAMergeSoldierInputs(m_patternId)));   // 0x272EE2

        branch = checkPointer(new BAPFlowCWhile(newTargetAlive(target), engage, false, m_patternId));   // 0x272B18
    }
    else if (!stationary && state->m_hasFiringPosition)
    {
        // ---- Go to the firing position, then fire (0x26FBxx..0x26FF9x) ----
        block->addStatement(newOverrideSensingAgent(selfFlag, m_patternId));
        const Vec2 firingPosition = { state->m_position.x, state->m_position.z };
        BAPConPosition* const arrived = checkPointer(new BAPConPosition(firingPosition, state->m_arrivalRadius * 0.5f));
        block->addStatement(checkPointer(new BAPAMoveToFinding(
            bot, state->m_position, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, arrived,
            selfInfo->getControlInfo()->getDriveControls(), m_patternId, 0.0f, 0.0f)));
        if (m_weapons[bot->getBotId()] != -1 && state->m_shots != 0)
        {
            BAPCombinerParallel* const fireBlock = checkPointer(new BAPCombinerParallel(m_patternId, 6, false));
            if (guidance == 0)
                fireBlock->addStatement(checkPointer(new BAPWrapperAiming(aimVariable, aimValidVariable, aimOffsetVariable,
                                                                          self, target, true)));
            else
                fireBlock->addStatement(checkPointer(new BAPWrapperLookAt(target, lookVariable, true, guidance == 3,
                                                                          aimVariable, aimValidVariable)));
            if (guidance == 0)
                fireBlock->addStatement(createFirePlan(bot, state, weaponVariable, targetVariable, aimValidVariable, aimVariable,
                                                       aimOffsetVariable, lookVariable, weaponInfo, selfInfo, controlInfo,
                                                       targetInfo, targetPhysical, WGSNone));
            else
                fireBlock->addStatement(createFirePlanGuided(bot, state, weaponVariable, targetVariable, lookVariable,
                                                             weaponInfo, selfInfo, controlInfo, targetInfo, targetPhysical,
                                                             static_cast<WeaponGuidingSystem>(guidance)));
            branch = fireBlock;
        }
    }
    else
        stationary = true;

    if (stationary)
    {
        // ---- Fire from here (0x2709A0..0x270D26) ----
        if (m_weapons[bot->getBotId()] == -1 || state->m_shots == 0)
            return 0;   // the variables and the statements built so far are not released (as in the image)
        block->addStatement(newOverrideSensingAgent(selfFlag, m_patternId));
        BAPCombinerParallel* const fireBlock = checkPointer(new BAPCombinerParallel(m_patternId, 7, false));
        if (!weaponInfo->hasAutoReload() && weaponInfo->getNumberOfMags() != 1)
        {
            // Reload (input 0x20) once the magazine is empty.
            BAPConWeaponMagAmmo* const empty = checkPointer(new BAPConWeaponMagAmmo(
                weaponVariable, newVariable<int>(context, 0), false));
            BAPATrigger* const reload = checkPointer(new BAPATrigger(
                static_cast<io::PlayerInputMap>(0x20), checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
                0x100000000ULL, m_patternId, 0.0, false));
            fireBlock->addStatement(checkPointer(new BAPFlowCIf(empty, reload, 0, m_patternId, true, false)));
        }
        BAPStatement* firePlan;
        if (guidance == 0)
        {
            fireBlock->addStatement(checkPointer(new BAPWrapperAiming(aimVariable, aimValidVariable, aimOffsetVariable,
                                                                      self, target, true)));
            firePlan = createFirePlan(bot, state, weaponVariable, targetVariable, aimValidVariable, aimVariable,
                                      aimOffsetVariable, lookVariable, weaponInfo, selfInfo, controlInfo, targetInfo,
                                      targetPhysical, WGSNone);
        }
        else
        {
            fireBlock->addStatement(checkPointer(new BAPWrapperLookAt(target, lookVariable, true, guidance == 3,
                                                                      aimVariable, aimValidVariable)));
            firePlan = createFirePlanGuided(bot, state, weaponVariable, targetVariable, lookVariable, weaponInfo, selfInfo,
                                            controlInfo, targetInfo, targetPhysical,
                                            static_cast<WeaponGuidingSystem>(guidance));
        }
        fireBlock->addStatement(checkPointer(new BAPFlowCIf(
            checkPointer(new BAPConObjectValidAiming(target, self, aimValidVariable, aimVariable, false)), firePlan, 0,
            m_patternId, true, false)));
        branch = fireBlock;
    }

    // 0x2671C0 / 0x2671C3
    if (branch)
        block->addStatement(branch);
    int weapon = -1;
    if (weaponInfo)
        weapon = m_weapons[bot->getBotId()];
    BAPIWFire* const plan = checkPointer(new BAPIWFire(bot, target, m_flag20, state->m_backOff, weapon, -1, root, m_patternId));
    plan->setParent(this);
    SquadManager::m_instance->shotsFired(bot->getPlayerId(), bot->getSide(), m_targets[bot->getBotId()]);
    return plan;
}

// ELF 0x25F740 (5,526 instructions; returns the BAPIWFire). The plan for a guided weapon of a bot
// whose object has type flag 0x200000 (createPlanInternal 0x2779B1): look at the target, fire the
// guided plan while the line of fire and the range (or a control point nearby) hold, keep within
// 0.5 x max .. min range, reload when empty and no own projectile is within 1000 m.
BotScript* BBPFireInfantery::createGuidedPlanInternalInfantry(Bot* bot, bool targetIsLodBot, const Vec3& movePosition)
{
    const unsigned int id = bot->getBotId();
    FiringState* const state = &m_firingStates[id];
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(gameTime());
    dropPlan(bot);

    Information* const selfInfo = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = selfInfo->getArmament();
    IPIControlInfo* const controlInfo = selfInfo->getControlInfo();
    if (!armament || !selfInfo->getPhysical() || !controlInfo)
        return 0;
    IPIMobile* const selfMobile = selfInfo->getMobile();
    if (selfMobile)
        selfMobile->isTurnable();          // result unused in the image
    controlInfo->getDriveControls();       // results unused in the image
    controlInfo->getAimControls();
    const Vec3& selfPosition = selfInfo->getPosition();

    const unsigned int target = state->m_target;
    Information* const targetInfo = informationManager->getInformation(target);
    if (!targetInfo)
        return 0;
    IPIPhysical* const targetPhysical = targetInfo->getPhysical();
    if (!targetPhysical)
        return 0;

    // No -1 check on the weapon index here (createPlanInternal made it).
    const int weapon = m_weapons[bot->getBotId()];
    WeaponInfo* const weaponInfo = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weapon));
    armament->setActiveWeapon(weapon, bot->getSide());

    BAPVariable<WeaponInfo*>* const weaponVariable = newVariable<WeaponInfo*>(context, weaponInfo);
    BAPVariable<unsigned int>* const targetVariable = newVariable<unsigned int>(context, target);
    BAPVariable<bool>* const aimValidVariable = newVariable<bool>(context, false);
    const Vec3 up = { 0.0f, 1.0f, 0.0f };
    const Vec3 zero = { 0.0f, 0.0f, 0.0f };
    BAPVariable<Vec3>* const aimVariable = newVariable<Vec3>(context, up);
    BAPVariable<Vec3>* const lookVariable = newVariable<Vec3>(context, zero);

    BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, 0));
    if (weaponInfo->getWeaponActivate() != 0x40)
        root->addStatement(checkPointer(new BAPAChangeWeapon(bot, weapon, m_patternId)));
    BAPCombinerParallel* const block = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
    root->addStatement(block);
    const bool selfFlag = (selfInfo->getObjectTypeFlags() & 0x200000) != 0;
    block->addStatement(newOverrideSensingAgent(selfFlag, m_patternId));   // before the reset here
    block->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_patternId)));
    if (!selfFlag)
        block->addStatement(checkPointer(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, m_patternId)));
    const int guidance = weaponInfo->getWeaponGuidanceType();

    // A control point close to the bot (no fire-from-here / no-move checks in this function).
    BAPConSharedCondition* controlPointCondition = 0;   // -0x220
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getClosestObject(selfPosition, bot->getSide(), false);
    Vec3 controlPoint = { 0.0f, 0.0f, 0.0f };
    if (object && static_cast<AIStrategicArea*>(object)->m_controlPointId != static_cast<unsigned int>(-1))
    {
        // The image reads the vehicle number of a null mobile here as well (non-virtual field read).
        const int equipmentType = selfInfo->getUnit()->getEquipmentType();
        const int vehicleNumber = selfMobile->getVehicleNumber();
        static_cast<AIStrategicArea*>(object)->getControlpointPos(object, bot->getSide(), vehicleNumber,
                                                                 equipmentType, selfPosition, controlPoint);
        const float radius = object->m_size[bot->getSide()];
        Vec3 objectPosition = { 0.0f, 0.0f, 0.0f };
        vec2ToVec3XZ(object->getPosition(bot->getSide()), objectPosition);
        objectPosition.y = controlPoint.y;
        const Vec3 offset = { objectPosition.x - controlPoint.x, objectPosition.y - controlPoint.y,
                              objectPosition.z - controlPoint.z };
        if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z < radius * radius)
        {
            // The image also computes controlPoint - selfPosition into a slot it never reads.
            BAPConAnd* const near = checkPointer(new BAPConAnd());
            near->addCondition(checkPointer(new BAPConStrategicObjectStatus(object, bot->getSide(), false)));   // 0x263FCF
            near->addCondition(checkPointer(new BAPConPointDistance(controlPoint, 40.0f, true)));
            controlPointCondition = checkPointer(new BAPConSharedCondition(near));
        }
    }

    BAPConSharedCondition* const lineOfFire =
        checkPointer(new BAPConSharedCondition(checkPointer(new BAPConObjectLineOfFire(target, targetIsLodBot))));
    BAPConAnd* const fireCondition = checkPointer(new BAPConAnd());
    fireCondition->addCondition(lineOfFire->getNewSharedCondition());
    if (!controlPointCondition)
        fireCondition->addCondition(checkPointer(new BAPConObjectDistance(
            target, weaponInfo->getMinRange() * 0.9f, weaponInfo->getMaxRange() * 1.1f, true, false)));
    else
    {
        BAPConOr* const inRange = checkPointer(new BAPConOr());
        inRange->addCondition(checkPointer(new BAPConObjectDistance(
            target, weaponInfo->getMinRange() * 0.9f, weaponInfo->getMaxRange() * 1.1f, true, false)));
        inRange->addCondition(controlPointCondition->getNewSharedCondition());
        fireCondition->addCondition(inRange);
    }

    BAPStatement* firePlan = 0;   // -0x268
    if (m_weapons[bot->getBotId()] != -1 && state->m_shots != 0)
        firePlan = createFirePlanGuided(bot, state, weaponVariable, targetVariable, lookVariable, weaponInfo, selfInfo,
                                        controlInfo, targetInfo, targetPhysical,
                                        static_cast<WeaponGuidingSystem>(guidance));   // 0x2638FA
    BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
        bot, target, 0.0f, checkPointer(new BAPConFalse()), controlInfo->getLookControls(), m_patternId,
        lookVariable, 0.0f));
    BAPFlowCIf* const fireOrLook = checkPointer(new BAPFlowCIf(fireCondition, firePlan, look, m_patternId, true, false));

    BAPCombinerParallel* const engage = checkPointer(new BAPCombinerParallel(m_patternId, 4, false));
    engage->addStatement(checkPointer(new BAPAEvaluateCondition(lineOfFire, m_patternId)));
    engage->addStatement(checkPointer(new BAPWrapperLookAt(target, lookVariable, true, guidance == 3, aimVariable,
                                                           aimValidVariable)));

    // Movement (0x260456..0x260B3F): no line of fire or fire not allowed -> walk to 1.1 x minimum
    // range, forget the target, end; else close in to 0.85 x max while beyond 0.5 x max or inside min.
    BAPStatement* movement = 0;   // -0x294
    if (selfMobile)
    {
        BAPConOr* const noFire = checkPointer(new BAPConOr());
        noFire->addCondition(checkPointer(new BAPConNot(lineOfFire->getNewSharedCondition())));
        noFire->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConAllowFire()))));
        bool targetMobile = targetInfo->getMobile() != 0;
        if (!targetMobile && !targetInfo->isPrimary())
            targetMobile = informationManager->getInformation(targetInfo->getPrimaryObject(bot->getSide()))->getMobile() != 0;
        BAPStatement* away;
        if (targetMobile)
            away = checkPointer(new BAPAMoveToObjectFinding(
                bot, state->m_target, 0.0f, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.5f, 0,
                controlInfo->getDriveControls(), m_patternId, 0, weaponInfo->getMinRange() * 1.1f, 1));
        else
            away = checkPointer(new BAPAMoveToFinding(
                bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, 0.5f, weaponInfo->getMinRange() * 1.1f));
        BAPCombinerSerial* const lost = checkPointer(new BAPCombinerSerial(m_patternId, 300));
        lost->addStatement(away);
        lost->addStatement(checkPointer(new BAPAForgetTarget(state->m_target)));
        lost->addStatement(checkPointer(new BAPFlowCEnd(m_patternId, static_cast<BAPFlowCEnd::EndType>(0), 0)));

        BAPConOr* const approachCondition = checkPointer(new BAPConOr());
        approachCondition->addCondition(checkPointer(new BAPConObjectDistance(target, weaponInfo->getMaxRange() * 0.5f, true, false, true)));
        approachCondition->addCondition(checkPointer(new BAPConObjectDistance(target, weaponInfo->getMinRange(), true, false, false)));
        BAPStatement* approachMove;
        if (targetMobile)
            approachMove = checkPointer(new BAPAMoveToObjectFinding(
                bot, state->m_target, weaponInfo->getMaxRange() * 0.85f, selfMobile->getMaxSpeed(),
                selfMobile->getMaxSpeed() * 0.5f, 0, controlInfo->getDriveControls(), m_patternId,
                checkPointer(new BAPConFalse()), atLeast(weaponInfo->getMinRange() * 1.1f, 15.0f), 2));
        else
            approachMove = checkPointer(new BAPAMoveToFinding(
                bot, movePosition, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
                controlInfo->getDriveControls(), m_patternId, weaponInfo->getMaxRange() * 0.85f,
                atLeast(weaponInfo->getMinRange() * 1.1f, 15.0f)));
        BAPFlowCIf* const approach = checkPointer(new BAPFlowCIf(approachCondition, approachMove, 0, m_patternId, false, false));
        movement = checkPointer(new BAPFlowCIf(noFire, lost, approach, m_patternId, true, false));   // 0x260B3F
    }

    // Reload: empty magazine and no own projectile within 1000 m (0x260B52..0x260DE4).
    BAPConAnd* const reloadCondition = checkPointer(new BAPConAnd());
    reloadCondition->addCondition(checkPointer(new BAPConWeaponMagAmmo(weaponVariable, newVariable<int>(context, 0), false)));
    reloadCondition->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConDistanceToProj(armament, 1000.0f)))));
    BAPATrigger* const reload = checkPointer(new BAPATrigger(
        static_cast<io::PlayerInputMap>(0x20), checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
        0x100000000ULL, m_patternId, 0.0, false));
    engage->addStatement(checkPointer(new BAPFlowCIf(reloadCondition, reload, 0, m_patternId, true, false)));
    if (controlPointCondition)
    {
        BAPAMoveToFinding* const toControlPoint = checkPointer(new BAPAMoveToFinding(
            bot, controlPoint, selfMobile->getMaxSpeed(), selfMobile->getMaxSpeed() * 0.75f, 0,
            controlInfo->getDriveControls(), m_patternId, 0.0f, 0.0f));
        movement = checkPointer(new BAPFlowCIf(controlPointCondition, toControlPoint, movement, m_patternId, false, false));
    }
    engage->addStatement(movement);   // null is ignored
    engage->addStatement(fireOrLook);
    if (selfFlag)
        engage->addStatement(checkPointer(new BAPAMergeSoldierInputs(m_patternId)));
    block->addStatement(checkPointer(new BAPFlowCWhile(newTargetAlive(target), engage, false, m_patternId)));

    int planWeapon = -1;
    if (weaponInfo)
        planWeapon = m_weapons[bot->getBotId()];
    BAPIWFire* const plan = checkPointer(new BAPIWFire(bot, target, m_flag20, state->m_backOff, planWeapon, -1, root, m_patternId));
    plan->setParent(this);
    return plan;   // no SquadManager::shotsFired here (unlike createPlanInternal)
}

// ELF 0x277CF0 (1,941 instructions). The firing part of a plan: aim at the target (or at the
// firing position's point for indirect fire, FiringState::m_bool26), pull the trigger while the
// precision condition (and, for splash weapons in static attack, the grenade checks) hold, and
// zoom first when the weapon has a zoom input.
BAPStatement* BBPFireInfantery::createFirePlan(Bot* bot, BBPFireInfantery::FiringState* state,
                                               BAPVariable<WeaponInfo*>* weaponVariable, BAPVariable<unsigned int>*,
                                               BAPVariable<bool>* aimValidVariable, BAPVariable<Vec3>* aimVariable,
                                               BAPVariable<Vec3>* aimOffsetVariable, BAPVariable<Vec3>*,
                                               WeaponInfo* weaponInfo, Information* selfInfo, IPIControlInfo* controlInfo,
                                               Information* targetInfo, IPIPhysical*, WeaponGuidingSystem guidance)
{
    BAPContext* const context = bot->getScriptContext();
    const unsigned int target = state->m_target;

    // A sniper (spawn kit 1) with a zoomed long-range weapon (-0x179).
    bool sniping = false;
    if (weaponInfo->getWeaponZoom() != 0x40 && bot->getSpawnKit() == 1 && weaponInfo->getMaxRange() > 100.0f)
        sniping = true;
    bool splash = false;   // -0x17A
    if (weaponInfo->getExplosionRadius() != 0.0f && guidance == 0)
        splash = true;
    SensedObject* const sensed = bot->getSensedObjectFromHandle(target);
    const bool selfFlag = (selfInfo->getObjectTypeFlags() & 0x200000) != 0;
    const bool targetFlag = (targetInfo->getObjectTypeFlags() & 0x200000) != 0;
    bool splashAtTarget = false;   // -0x198
    if (!selfFlag && targetFlag && guidance == 0 && weaponInfo->getExplosionRadius() != 0.0f)
        splashAtTarget = true;
    bool targetAbove = false;
    if (bot->getSpawnKit() != 1 && !weaponInfo->getIsThrown())
    {
        const Vec3& targetPosition = targetInfo->getPosition();
        const Vec3& selfPosition = selfInfo->getPosition();
        if (30.0f < targetPosition.y - selfPosition.y)
            targetAbove = true;
    }
    bool grenadeChecks = false;   // -0x1AA
    if (static_cast<BotMain*>(bot)->getUseStaticAttack() && splash && targetFlag
        && ((sensed->m_flags & 8) || splashAtTarget || targetAbove))
        grenadeChecks = true;

    BAPCombinerParallel* const plan = checkPointer(new BAPCombinerParallel(m_patternId, false, 0xE, false));
    BAPAAimAt* aim;
    if (!state->m_bool26)
    {
        const Vec3 noOffset = { 0.0f, 0.0f, 0.0f };
        aim = checkPointer(new BAPAAimAtObject(bot, target, noOffset, checkPointer(new BAPConFalse()),
                                               controlInfo->getAimControls(), m_patternId, false, aimValidVariable,
                                               aimVariable));
    }
    else
    {
        BAPVariable<Vec3>* const point = newVariable<Vec3>(context, state->m_firingPosition);
        const Vec3 noOffset = { 0.0f, 0.0f, 0.0f };
        aim = checkPointer(new BAPAAimAtPoint(bot, point, noOffset, checkPointer(new BAPConFalse()),
                                              controlInfo->getAimControls(), m_patternId, aimValidVariable, aimVariable));
    }
    plan->addStatement(aim);

    float deviation = weaponInfo->getAllowedDeviation();
    if (splash && targetFlag && !selfFlag)
        deviation = 1.0f;   // 0x2780B1 cmove
    BAPCondition* precision;
    if (!aimValidVariable)
        precision = checkPointer(new BAPCConPrecision(target, aim, deviation, !weaponInfo->getBurst(), sniping,
                                                      weaponInfo->getMaxRange() < 5.0f));
    else
        precision = checkPointer(new BAPCConPrecision(target, aimValidVariable, aimVariable, aimOffsetVariable, deviation,
                                                      !weaponInfo->getBurst(), sniping, weaponInfo->getMaxRange() < 5.0f));
    BAPConAnd* const fireCondition = checkPointer(new BAPConAnd());
    if (weaponInfo->usesHeat())
        fireCondition->addCondition(checkPointer(new BAPConSharedCondition(
            checkPointer(new BAPConWeaponHeat(weaponVariable, 0.8f, 0.5f)))));   // 0x278F50
    if (grenadeChecks && splashAtTarget)
    {
        // 0x278E03: the precision condition, then the splash-damage check (3.5 m).
        fireCondition->addCondition(precision);
        fireCondition->addCondition(checkPointer(new BAPConSplashDamage(target, aimOffsetVariable, aimVariable, 3.5f)));
    }
    else
    {
        if (grenadeChecks)
            fireCondition->addCondition(checkPointer(new BAPConOkToThrowGrenade(target, aimOffsetVariable, aimVariable)));
        fireCondition->addCondition(precision);
    }

    // A fresh plan waits (1.5 + U[0,1)) x (1 - skill) s before the first shot (0x278C50).
    float delay = 0.0f;
    if (m_freshPlan && weaponInfo->getMaxRange() > 10.0f)
    {
        const float wait = randomFloat(1.0f, 0.0f) + 1.5f;
        delay = (1.0f - static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill()) * wait;
    }
    BAPStatement* trigger;
    const int fire = weaponInfo->getWeaponFire();
    if (!weaponInfo->getBurst())
        trigger = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(fire), fireCondition, 0, inputMask(fire),
                                               m_patternId, delay, true));
    else
        trigger = checkPointer(new BAPATriggerContinously(static_cast<io::PlayerInputMap>(fire), fireCondition, 0, 0,
                                                          inputMask(fire), m_patternId, delay));
    if (weaponInfo->getWeaponZoom() != 0x40)
    {
        const int zoom = weaponInfo->getWeaponZoom();
        BAPATrigger* const zoomIn = checkPointer(new BAPATrigger(
            static_cast<io::PlayerInputMap>(zoom), checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
            inputMask(zoom), m_patternId, 0.0, false));
        plan->addStatement(checkPointer(new BAPFlowCIf(checkPointer(new BAPConNot(checkPointer(new BAPConWeaponIsZoomed()))),
                                                       zoomIn, 0, m_patternId, true, false)));
    }
    plan->addStatement(trigger);
    return plan;
}

// ELF 0x279B40 (1,735 instructions). The firing part for guided weapons: look at the target (or
// at the init(Vec3) position when the target is unknown) and fire once nothing in the Or holds:
// an empty magazine, no lock (heat-seeking and laser-guided), or a look deviation above 5 deg
// (laser-guided; the function-static `degrees`, 5.0 in .data 0x4BBE64) or 3 deg (otherwise).
BAPStatement* BBPFireInfantery::createFirePlanGuided(Bot* bot, const BBPFireInfantery::FiringState* state,
                                                     BAPVariable<WeaponInfo*>* weaponVariable, BAPVariable<unsigned int>*,
                                                     BAPVariable<Vec3>* lookVariable, WeaponInfo* weaponInfo,
                                                     Information*, IPIControlInfo* controlInfo, Information* targetInfo,
                                                     IPIPhysical*, WeaponGuidingSystem guidance)
{
    static float degrees = 5.0f;
    BAPContext* const context = bot->getScriptContext();
    const unsigned int target = state->m_target;
    BAPCombinerParallel* const plan = checkPointer(new BAPCombinerParallel(m_patternId, false, 0xE, false));
    BAPALook* look;
    if (targetInfo)
        look = checkPointer(new BAPALookAtObject(bot, target, 0.0f, checkPointer(new BAPConFalse()),
                                                 controlInfo->getLookControls(), m_patternId, lookVariable, 0.0f));
    else
        look = checkPointer(new BAPALookAtPoint(m_targetPositions[bot->getBotId()], bot, 0.00872664619f,
                                                checkPointer(new BAPConFalse()), controlInfo->getLookControls(),
                                                m_patternId));   // 0x27B345
    plan->addStatement(look);

    BAPConOr* const holdFire = checkPointer(new BAPConOr());
    holdFire->addCondition(checkPointer(new BAPConWeaponMagAmmo(weaponVariable, newVariable<int>(context, 0), false)));
    if (static_cast<unsigned int>(guidance) - 1u < 2u)
        holdFire->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConWeaponHasLockOnTarget(weaponVariable, target)))));
    if (guidance == WGSLaserGuided)
        holdFire->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPCConLookDeviation(look, degrees * 0.017453292f, false)))));
    else
        holdFire->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPCConLookDeviation(look, 0.052359879f, false)))));

    const int fire = weaponInfo->getWeaponFire();
    BAPATrigger* const trigger = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(fire), checkPointer(new BAPConTrue()),
                                                              holdFire, inputMask(fire), m_patternId, 0.0, true));
    if (weaponInfo->getWeaponZoom() != 0x40)
    {
        const int zoom = weaponInfo->getWeaponZoom();
        BAPATrigger* const zoomIn = checkPointer(new BAPATrigger(
            static_cast<io::PlayerInputMap>(zoom), checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
            inputMask(zoom), m_patternId, 0.0, false));
        plan->addStatement(checkPointer(new BAPFlowCIf(checkPointer(new BAPConNot(checkPointer(new BAPConWeaponIsZoomed()))),
                                                       zoomIn, 0, m_patternId, true, false)));
    }
    plan->addStatement(trigger);
    return plan;
}

// ELF 0x27B6F0. Tries alternating angles on a circle and keeps the furthest valid trace endpoint.
void BBPFireInfantery::calculateAwayPosition(const Vec3& from, const Vec3& centre, int map, Vec3& out, float radius)
{
    const float requiredDistanceSqr = (radius - 1.0f) * (radius - 1.0f);
    Vec3 best = from;
    Vec3 candidate = {0.0f, 0.0f, 0.0f};
    Vec3 direction = {from.x - centre.x, 0.0f, from.z - centre.z};
    const float directionLengthSqr = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (directionLengthSqr > 0.1f) {
        if (std::fabs(directionLengthSqr - 1.0f) >= 1.1920929e-7f) {
            if (std::fabs(directionLengthSqr) >= 1.1920929e-7f) {
                const float inverseLength = 1.0f / std::sqrt(directionLengthSqr);
                direction.x *= inverseLength;
                direction.y *= inverseLength;
                direction.z *= inverseLength;
            } else {
                direction = {0.0f, 0.0f, 0.0f};
            }
        }
    } else {
        direction = {1.0f, 0.0f, 0.0f};
    }

    float bestDistanceSqr = 0.0f;
    float angle = 0.0f;
    while (bestDistanceSqr <= requiredDistanceSqr && angle <= 1.570796371f) {
        const bool valid = TraceUtils::tracePointOnCircle(direction, centre, from, map, angle, radius, candidate);
        angle = -angle;
        if (angle >= 0.0f)
            angle += 0.261799395f;
        if (valid) {
            const Vec3 delta = {centre.x - candidate.x, centre.y - candidate.y, centre.z - candidate.z};
            const float distanceSqr = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            if (distanceSqr > bestDistanceSqr) {
                best = candidate;
                bestDistanceSqr = distanceSqr;
            }
        }
    }
    out = best;
}

} // namespace ai
} // namespace hfe
} // namespace dice
