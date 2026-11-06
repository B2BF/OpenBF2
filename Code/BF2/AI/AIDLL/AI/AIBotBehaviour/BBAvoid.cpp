// Avoid behaviour (infantry "Avoid" slot of aibehaviours.ai): a soldier scans the obstacles its
// navigation collected, predicts collisions with the moving non-soldier objects and hands the most
// urgent one to its BBPAvoidCollision pattern; a vehicle about to be bailed from is fled at urgency
// 100 per object, and a grenade warning or gas forces urgency 1000.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x1FD0B0 (__tcf_0 0x1FD110, _GLOBAL__I 0x1FD140): <iostream>'s Init object only.
#include <iostream>

#include "BBAvoid.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBotBehaviourPattern/BBPAvoidCollision.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Game/Objects/IPlayer.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The sphere centre of an information in world space: the local sphere offset turned by the
// rotation rows of the transformation (row vector times matrix: x uses m[0], m[4], m[8]), plus the
// position. Inlined twice in calculateUrgency (0x1FBE28 for the bot, 0x1FC5DB per obstacle); the
// calls run getTransformation (slot +0x3C), getSphereLocalOffset (IPIPhysical +0x30), getPosition
// (+0x30) in that order.
Vec3 sphereCentre(Information* information, IPIPhysical* physical)
{
    const Mat4& transformation = information->getTransformation();
    const Vec3 offset = physical->getSphereLocalOffset();
    const float* m = transformation.m;
    Vec3 rotated;
    rotated.x = offset.x * m[0] + offset.y * m[4] + offset.z * m[8];
    rotated.y = offset.x * m[1] + offset.y * m[5] + offset.z * m[9];
    rotated.z = offset.x * m[2] + offset.y * m[6] + offset.z * m[10];
    const Vec3& origin = information->getPosition();
    Vec3 centre;
    centre.x = rotated.x + origin.x;
    centre.y = rotated.y + origin.y;
    centre.z = rotated.z + origin.z;
    return centre;
}

inline float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace

// ELF 0x1FB9D0 (C2) / 0x1FBA90 (C1). The base gets getMaxNBots (IAISettings slot 0); a second
// getMaxNBots sizes the +0x08 target array (plain operator new[], no checkPointer block here), and
// every bot starts with urgency 0.0f (rodata 0x49CB34) and target 0xFFFFFFFF (`movl $0xffffffff`).
BBAvoid::BBAvoid(unsigned char index, unsigned char queue, BBPAvoidCollision* pattern, UrgeCurve* curve,
                 float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = new unsigned int[count];
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = 0xFFFFFFFFu;
    }
}

// ELF 0x1FBB50 (D2) / 0x1FBBA0 (D1) / 0x1FBBF0 (D0): delete[] +0x08 when set, then the base.
BBAvoid::~BBAvoid()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x1FBC40 (1,201 insns).
// Not updating: clears the changed flag and keeps the urgency while the pattern runs, else unInits.
// Updating: only a soldier (Information::isOfType(ITSoldier), byte +6 bit 5; AIDLL_r names bit 21
// ITSoldier in its enum switch 0x100D3010 and asserts "!targetPrimaryInfo->isOfType(ITSoldier)") with a mobile plug-in gets past the first
// tests. Retail quirks kept: two `!isSoldier` blocks can never run (the flag was tested earlier; the
// image reloads the same stack byte); the bot's own Information is used without a null check; so is
// the chosen target's Information before insertBehaviourInformation.
float BBAvoid::calculateUrgency(Bot* bot, bool update, float modifier)
{
    BBPAvoidCollision* const pattern = static_cast<BBPAvoidCollision*>(m_pattern);
    unsigned int* const targets = m_targets;

    if (!update)
    {
        // 0x1FBC6A: flags = 0; isInitiated (pattern slot +0x0C) keeps the stored urgency.
        m_targetChanged[bot->getBotId()] = false;
        if (pattern->isInitiated(bot))
            return m_urgency[bot->getBotId()];
        pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    // ELF 0x4BA5A4 (d, 0x3F19999A): calculateUrgency(Bot*, bool, float)::radiusTweak.
    static float radiusTweak = 0.6f;

    // 0x1FBD00: five Vec3 locals (-0x28, -0x38, -0x48, -0x58, -0x68) start at 0.0f.
    Vec3 position = { 0.0f, 0.0f, 0.0f };
    Vec3 speed = { 0.0f, 0.0f, 0.0f };
    Vec3 toObstacle = { 0.0f, 0.0f, 0.0f };
    Vec3 relativeSpeed = { 0.0f, 0.0f, 0.0f };
    Vec3 collisionPoint = { 0.0f, 0.0f, 0.0f };

    const unsigned int previousTarget = targets[bot->getBotId()];
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    unsigned int selfPrimary;
    if (self->isPrimary())
        selfPrimary = bot->getCurrentEquipment();
    else
        selfPrimary = self->getPrimaryObject(bot->getSide());
    self->getName();   // 0x1FBDC1: called, result unused

    float maxUrgency = 0.0f;   // -0x208
    float total = 0.0f;        // -0x200
    IPIMobile* const mobile = self->getMobile();
    if (!mobile)
    {
        m_urgency[bot->getBotId()] = total;
        return total;
    }
    const bool isSoldier = self->isOfType(ITSoldier);   // testb $0x20, 0x6(info)
    if (!isSoldier)
    {
        m_urgency[bot->getBotId()] = total;
        return total;
    }

    IPIPhysical* const physical = self->getPhysical();
    position = sphereCentre(self, physical);
    const float radius = physical->getSmallestRadius();   // IPIPhysical +0x34
    speed = mobile->getSpeed();                          // IPIMobile +0x1C
    const float speedMagnitude = std::sqrt(dot(speed, speed));
    // 0x1FBF7E: unreachable (isSoldier is true here); getUnresolvedCollisionHandle is Bot +0x9C.
    if (!isSoldier && (3.0f > speedMagnitude || bot->getUnresolvedCollisionHandle() != 0xFFFFFFFFu))
    {
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    float lookAhead = mobile->getAvoidCollisionLookAhead();
    if (self->getAIObject())
    {
        if (self->getSoldierPose() == SPLying)
            lookAhead += lookAhead;                       // 0x1FCD75: fadd %st(0), %st
        else if (self->getSoldierPose() == SPCrouching)
            lookAhead *= 1.5f;                            // rodata 0x49CDC0
    }
    // 0x1FC013: (flags & 0x10) != 0, plus 2; collisionPredicted never reads it.
    const int predictMode = (self->isOfType(ITAir)) + 2;

    unsigned int changeTarget = bot->getChangeTarget();
    Information* const changeInformation = informationManager->getInformation(changeTarget);
    if (changeInformation && !changeInformation->isPrimary())
        changeTarget = changeInformation->getPrimaryObject(bot->getSide());
    unsigned int repairTarget = bot->getRepairTarget();
    Information* const repairInformation = informationManager->getInformation(repairTarget);
    if (repairInformation && !repairInformation->isPrimary())
        repairTarget = repairInformation->getPrimaryObject(bot->getSide());

    // 0x1FC0DF: the obstacle handles are BotNavigation +0x58 (std::vector<unsigned int>, filled by
    // updateObstacles); the count is taken once before the loop, the data pointer on every pass.
    // ELF 0x1FC0DF: getBotNavigation (slot +0xFC), then `lea 0x58(%eax)`.
    const std::vector<unsigned int>& obstacles = bot->getBotNavigation()->getObstacles();

    const unsigned int obstacleCount = static_cast<unsigned int>(obstacles.size());
    for (unsigned int i = 0; i < obstacleCount; ++i)
    {
        const unsigned int handle = obstacles[i];
        Information* const obstacle = informationManager->getInformation(handle);
        if (!obstacle)
            continue;
        IPIMobile* const obstacleMobile = obstacle->getMobile();
        if (obstacle->getObjectSpawnerHolding()
            && obstacle->getObjectSpawner(bot->getSide()) == bot->getCurrentEquipment())
            continue;
        unsigned int obstaclePrimary = handle;
        if (!obstacle->isPrimary())
            obstaclePrimary = obstacle->getPrimaryObject(bot->getSide());
        IPIPhysical* const obstaclePhysical = obstacle->getPhysical();
        if (!obstaclePhysical || selfPrimary == obstaclePrimary || handle == repairTarget || handle == changeTarget
            || handle == bot->getWantedRepairTarget() || obstacle->isOfType(ITSoldier))
            continue;

        // 0x1FC5DB: obstacle sphere centre minus own centre.
        const Vec3 obstacleCentre = sphereCentre(obstacle, obstaclePhysical);
        toObstacle.x = obstacleCentre.x - position.x;
        toObstacle.y = obstacleCentre.y - position.y;
        toObstacle.z = obstacleCentre.z - position.z;
        // 0x1FC737: unreachable (isSoldier): an obstacle behind a moving non-soldier is ignored.
        if (!isSoldier && 0.0f > dot(toObstacle, speed))
            continue;

        Vec3 obstacleSpeed = { 0.0f, 0.0f, 0.0f };   // -0x178
        if (obstacleMobile)
            obstacleSpeed = obstacleMobile->getSpeed();
        relativeSpeed.x = obstacleSpeed.x - speed.x;
        relativeSpeed.y = obstacleSpeed.y - speed.y;
        relativeSpeed.z = obstacleSpeed.z - speed.z;
        const float relativeSpeedSq = dot(relativeSpeed, relativeSpeed);   // -0x254
        float radii = radius + obstaclePhysical->getSmallestRadius();
        if (isSoldier)
            radii *= radiusTweak;                                           // fmuls 0x74E0(%ebx)
        const float obstacleHealth = obstaclePhysical->getHealth();        // IPIPhysical +0x14
        const float bailValue = IBotManager::instance->getVehicleBailValue();   // +0x68

        IPIArmament* const armament = self->getArmament();
        WeaponInfo* activeWeapon = 0;
        if (armament && armament->getActiveWeapon() != -1)
            activeWeapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));

        // 0x1FC8B1: flee a vehicle whose health is under the bail value, or one with type flag
        // 0x02000000 (byte +7 bit 1), unless the active weapon is of type 4. [INFERRED] name `flee`.
        bool flee = false;
        if (isSoldier)
        {
            if (bailValue > obstacleHealth || obstacle->isOfType(ITObstacle))
            {
                if (!activeWeapon || activeWeapon->getWeaponType() != 4)
                    flee = true;
            }
        }
        if (isSoldier && !flee && 4.0f > dot(obstacleSpeed, obstacleSpeed))
            continue;

        const float distance = std::sqrt(dot(toObstacle, toObstacle));      // -0x268
        // 0x1FC978: max(r, min(r + 4.0f, 10.0f)) with two getSmallestRadius calls (fcmovnbe, fcmovbe).
        const float smallestRadius = obstaclePhysical->getSmallestRadius();
        float fleeDistance = obstaclePhysical->getSmallestRadius() + 4.0f;
        if (fleeDistance > 10.0f)
            fleeDistance = 10.0f;
        if (fleeDistance <= smallestRadius)
            fleeDistance = smallestRadius;

        if (flee)
        {
            if (fleeDistance > distance)
            {
                maxUrgency = 100.0f;                                       // rodata 0x49CB5C
                total += maxUrgency;
                float strength = (fleeDistance - distance) / 5.0f + 0.5f;
                if (strength <= 1.0f)
                    strength = 1.0f;
                targets[bot->getBotId()] = handle;
                Vec3 zeroPoint = { 0.0f, 0.0f, 0.0f };      // -0x168
                Vec3 zeroSpeed = { 0.0f, 0.0f, 0.0f };      // -0x158
                pattern->init(bot, zeroPoint, handle, zeroSpeed, strength);
            }
            continue;
        }

        float timeToCollision = 0.0f;   // -0x1EC; [CHOSEN] initialised, the image leaves it to collisionPredicted
        const float predicted = collisionPredicted(toObstacle, relativeSpeed, predictMode, lookAhead, radii,
                                                   collisionPoint, timeToCollision);
        if (!(predicted > 0.0f))
            continue;
        // 0x1FCB3C: the image squares 3.0f at run time (flds 3; fmul %st(0), %st).
        const float minRelativeSpeed = 3.0f;
        if (relativeSpeedSq < minRelativeSpeed * minRelativeSpeed)
            continue;
        float divisor;
        if (1.1920929e-07f > timeToCollision)                           // rodata 0x49CC7C (FLT_EPSILON)
            divisor = 1.0f;
        else
            divisor = std::sqrt(dot(toObstacle, toObstacle));
        const float mass = obstacle->getPhysical()->getMass();           // IPIPhysical +0x4C
        const float value = mass * std::sqrt(dot(relativeSpeed, relativeSpeed)) / divisor * predicted;
        total += value;
        if (value > maxUrgency)
        {
            maxUrgency = value;
            targets[bot->getBotId()] = handle;
            pattern->init(bot, collisionPoint, handle, relativeSpeed, timeToCollision);
        }
    }

    Vec3 grenadePosition = { 0.0f, 0.0f, 0.0f };   // -0x1E8
    if (isSoldier && total == 0.0f)
    {
        // 0x1FC577: BotNavigation::getGrenadeWarning (0x195310) answers `seta`: the warning time is
        // still inside the limit at rodata 0x49CCB8 (PLT 0x17E9E0, called at 0x1FC59C, `test al`).
        double grenadeTime = 0.0;
        const bool grenadeWarning = bot->getBotNavigation()->getGrenadeWarning(grenadePosition, grenadeTime);
        if (grenadeWarning)
        {
            Vec3 zero = { 0.0f, 0.0f, 0.0f };   // -0x1D8, passed twice
            pattern->init(bot, zero, 0xFFFFFFFFu, zero, 0.0f);
            m_urgency[bot->getBotId()] = 1000.0f;   // rodata 0x49CF48
            return 1000.0f;
        }
    }
    (void)grenadePosition;

    // 0x1FC26D: IAIPlayer +0x0C getPlayer, then world::IPlayer +0x2B8 getBlurValue, +0x140 getIsGassed.
    world::IPlayer* const player = bot->getPlayer()->getPlayer();
    if (player->getBlurValue() > 0.0f && player->getIsGassed())
    {
        Vec3 zero = { 0.0f, 0.0f, 0.0f };
        pattern->init(bot, zero, 0xFFFFFFFFu, zero, 0.0f);
        m_urgency[bot->getBotId()] = 1000.0f;
        return 1000.0f;
    }

    if (total == 0.0f)
    {
        // 0x1FC2B6: the changed flag takes isInitiated, then unInit and urgency 0.
        const bool initiated = pattern->isInitiated(bot);
        m_targetChanged[bot->getBotId()] = initiated;
        pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders())
        total *= bot->getMemory()->m_float8;
    if (self->isOfType(ITSoldier))
    {
        // 0x1FC456: IBFTimer +0x08 minus the double at CommunicationAgent +0x60 (fsubl 0x60(%eax)),
        // compared with 5.0f (rodata 0x49CDAC); a recent time doubles the total.
        const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        CommunicationAgent* const communication = bot->getCommunicationAgent();
        const double since = communication->getDouble96();
        if (now - since < 5.0f)
            total += total;
    }

    m_urgency[bot->getBotId()] = modifier * total;
    m_targetChanged[bot->getBotId()] = previousTarget != targets[bot->getBotId()];
    // 0x1FC3DF: no null check on the target's Information (retail).
    Information* const target = informationManager->getInformation(targets[bot->getBotId()]);
    bot->insertBehaviourInformation(0.5f, target->getPosition(), m_index);   // Bot +0x8C, rodata 0x49CB44
    return m_urgency[bot->getBotId()];
}

// ELF 0x1FCDF0. Closest approach of the relative motion: position `relative` (obstacle minus self),
// velocity `velocity`. Below a relative speed of 0.001 (double, rodata 0x49F4A0, squared at run
// time) there is no collision. `closestTime` is the time of closest approach; a closest distance
// inside sqrt(1.5) * radius gives the entry time t - sqrt(|d^2 - 1.5 r^2|) / |v|, written to `time`;
// inside [0, lookAhead] the entry point goes to `point` and the answer is 1.0f, else 0.0f.
float BBAvoid::collisionPredicted(Vec3& relative, Vec3& velocity, int, float lookAhead, float radius, Vec3& point,
                                  float& time)
{
    const float radiusSq = radius * radius * 1.5f;   // 0x1FCE13: stored back into the argument slot
    const float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    const double minSpeed = 0.001;
    if (speedSq < minSpeed * minSpeed)
        return 0.0f;
    const float speed = std::sqrt(speedSq);
    const float inverse = 1.0f / speed;
    Vec3 direction;
    direction.x = velocity.x * inverse;
    direction.y = velocity.y * inverse;
    direction.z = velocity.z * inverse;
    Vec3 back;
    back.x = -relative.x;
    back.y = -relative.y;
    back.z = -relative.z;
    const float closestTime = (direction.x * back.x + direction.y * back.y + direction.z * back.z) / speed;
    if (0.0f > closestTime)
        return 0.0f;
    Vec3 closest;
    closest.x = relative.x + velocity.x * closestTime;
    closest.y = relative.y + velocity.y * closestTime;
    closest.z = relative.z + velocity.z * closestTime;
    const float closestSq = closest.x * closest.x + closest.y * closest.y + closest.z * closest.z;
    if (closestSq >= radiusSq)
        return 0.0f;
    const float entryTime = closestTime - std::sqrt(std::fabs(closestSq - radiusSq)) / speed;
    time = entryTime;
    if (entryTime > lookAhead || 0.0f > entryTime)
        return 0.0f;
    point.x = velocity.x * entryTime + relative.x;
    point.y = velocity.y * entryTime + relative.y;
    point.z = velocity.z * entryTime + relative.z;
    return 1.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
