// InfanteryControl (attested TU `AI/AIDLL/AI/AIPlanInterpreter/InfanteryControl.cpp`, the path of
// infanteryPose's Debug). Recovered from libaidll.so (Linux 1.5.3153, ia-32); every body carries its
// ELF address. Read in Intel syntax (no x87 operand swap) and checked against the Ghidra
// decompilation. Parameter and local names are [INFERRED].
//
// Input writes: the image calls Input<PlayerInputMap, 64>::setInput(index, float) out of line at
// most sites and inlines it at a few (removeZoom's release); both are the same function here.

// static init: ELF 0x2CCE30 (__tcf_0 0x2CCE90, _GLOBAL__I 0x2CCEC0): <iostream>'s Init object only.
#include <iostream>

#include "../AIBot/SensingAgent.h"
#include "InfanteryControl.h"

#include <cmath>
#include <string>
#include <vector>

#include "../Main/AIDllFrontier.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/SquadMember.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../../../../Core/Debug.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

namespace infantry_detail {

float inputValue(const io::PlayerInput* input, io::PlayerInputMap index)
{
    const int i = static_cast<int>(index);
    if (i > 63)
        return 0.0f;
    if (!((input->mappingFlags >> (static_cast<unsigned int>(i) & 0x3F)) & 1ULL))
        return 0.0f;
    return input->inputs[i];
}

// [CHOSEN, not recovered] the image writes SensingAgent +0x50 (the move direction) and stores 0.0
// at +0x7C (a double) inline (EntryInfanteryMoveTo 0x2B7CF7 `fstpl 0x7c(%eax)`, 0x2B7CFA/0x2B7CFD).
// The +0x7C store is SensingAgent::setOverrideTime (ai-bot).
void setSensingMoveDirection(SensingAgent* agent, const Vec2& direction)
{
    agent->setOverrideTime(0.0);
    agent->setMoveDirection(direction);
}

void normaliseInline(Vec3& v)
{
    const float lengthSqr = v.x * v.x + v.y * v.y + v.z * v.z;
    if (!(1.1920929e-07f <= std::fabs(lengthSqr - 1.0f)))
        return;
    if (!(1.1920929e-07f <= std::fabs(lengthSqr)))
    {
        v.x = v.y = v.z = 0.0f;
        return;
    }
    const float inverse = 1.0f / std::sqrt(lengthSqr);
    v.x = v.x * inverse;
    v.y = v.y * inverse;
    v.z = inverse * v.z;
}

Vec3 crossInline(const Vec3& a, const Vec3& b)
{
    Vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - b.z * a.x;
    r.z = b.y * a.x - b.x * a.y;
    return r;
}

} // namespace infantry_detail

namespace {

using infantry_detail::inputValue;

inline Information* lookupInformation(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

// The inline zero test the image applies to trace results (|c - 0| <= FLT_EPSILON for x, y, z).
inline bool isZeroVec(const Vec3& v)
{
    const float eps = 1.1920929e-07f;
    return std::fabs(v.x - 0.0f) <= eps && std::fabs(v.y - 0.0f) <= eps && std::fabs(v.z - 0.0f) <= eps;
}

inline float distance(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

// ELF 0x2CB200
void removeZoom(Bot* bot)
{
    Information* info = lookupInformation(bot->getCurrentEquipment());
    IPIArmament* armament = info->getArmament();
    if (armament == 0)
        return;
    // [INFERRED] the image queries the active weapon twice and drops the first result.
    const unsigned int unusedWeapon = armament->getWeapon(armament->getActiveWeapon());
    (void)unusedWeapon;
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (weapon == 0)
        return;
    if (weapon->getWeaponZoom() == io::PINone)
        return;

    if (!weapon->isZoomed())
    {
        bot->getInput()->setInput(static_cast<io::PlayerInputMap>(weapon->getWeaponZoom()), 0.0f);
        return;
    }
    io::PlayerInput* input = bot->getInput();
    if (inputValue(input, static_cast<io::PlayerInputMap>(weapon->getWeaponZoom())) != 0.0f)
    {
        // the zoom key is held: release it (inlined setInput in the image, ELF 0x2CB3xx)
        input->setInput(static_cast<io::PlayerInputMap>(weapon->getWeaponZoom()), 0.0f);
        return;
    }
    // press it once to leave zoom
    bot->getInput()->setInput(static_cast<io::PlayerInputMap>(weapon->getWeaponZoom()), 1.0f);
}

// ELF 0x2CB550
void infanteryControlThrottle(Bot* bot, io::PlayerInput* input, IPIControlInfo*, float maxSpeed, bool sprint)
{
    if (static_cast<BotMain*>(bot)->getBotIsSimulatingArtillery())   // BotMain +0x234
    {
        bot->getBotNavigation()->clearProgress();
        bot->requestSoldierPose(SPCrouching, true);
        return;
    }
    const float squadSpeed = bot->getSquadMember()->getMaxSpeed();   // 0x2CB5D1
    if (sprint)
    {
        float throttle;
        if (maxSpeed <= 0.0f)
            throttle = (maxSpeed < 0.0f) ? -1.0f : 0.0f;
        else
            throttle = 1.0f;
        input->setInput(io::PIThrottle, throttle);
        input->setInput(io::PISprint, 1.0f);
    }
    else
    {
        input->setInput(io::PIThrottle, squadSpeed / maxSpeed);
        input->setInput(io::PISprint, 0.0f);
    }
}

// ELF 0x2CB660
bool infanteryResetControls(Bot* bot, io::PlayerInput* input, bool movement, bool look, bool weapons)
{
    if (movement)
    {
        input->setInput(io::PIAction, 0.0f);
        input->setInput(io::PISprint, 0.0f);
        input->setInput(io::PIYaw, 0.0f);
        input->setInput(io::PIRoll, 0.0f);
        input->setInput(io::PIPitch, 0.0f);
        input->setInput(io::PIThrottle, 0.0f);
        input->setInput(io::PIMouseLookX, 0.0f);
        input->setInput(io::PIMouseLookY, 0.0f);
    }
    if (look)
    {
        // level the view: look down by the forward vector's height (transformation row 2, y)
        Information* info = lookupInformation(bot->getCurrentEquipment());
        const Mat4& transformation = info->getTransformation();
        input->setInput(io::PIMouseLookX, 0.0f);
        input->setInput(io::PIMouseLookY, -transformation.m[9]);
    }
    if (weapons)
    {
        input->setInput(io::PIFire, 0.0f);
        input->setInput(io::PIAltFire, 0.0f);
        input->setInput(io::PIWeaponSelect1, 0.0f);
        input->setInput(io::PIWeaponSelect2, 0.0f);
        input->setInput(io::PIWeaponSelect3, 0.0f);
        input->setInput(io::PIWeaponSelect4, 0.0f);
        input->setInput(io::PIWeaponSelect5, 0.0f);
        input->setInput(io::PIWeaponSelect6, 0.0f);
        input->setInput(io::PIWeaponSelect7, 0.0f);
        input->setInput(io::PIWeaponSelect8, 0.0f);
        input->setInput(io::PIPositionSelect1, 0.0f);
        input->setInput(io::PIPositionSelect2, 0.0f);
        input->setInput(io::PIPositionSelect3, 0.0f);
        input->setInput(io::PIPositionSelect4, 0.0f);
        input->setInput(io::PIPositionSelect5, 0.0f);
        input->setInput(io::PIPositionSelect6, 0.0f);
        input->setInput(io::PIPositionSelect7, 0.0f);
        input->setInput(io::PIPositionSelect8, 0.0f);
        input->setInput(io::PIUse, 0.0f);
        input->setInput(io::PIReload, 0.0f);
        input->setInput(io::PIFlareFire, 0.0f);
    }
    return true;
}

// ELF 0x2CB9B0. PICrouch and PILie are toggles: the table presses the one that leads from the
// current to the wanted pose (and nothing when they are equal).
int infanteryPose(io::PlayerInput* input, SoldierPose current, SoldierPose wanted)
{
    int line;
    switch (current)
    {
    case SPCrouching:
        if (wanted == SPCrouching)
            goto none;
        if (wanted == SPStanding)
            goto crouch;
        if (wanted == SPLying)
            goto lie;
        line = 263;
        break;
    case SPStanding:
        if (wanted == SPCrouching)
            goto crouch;
        if (wanted == SPStanding)
            goto none;
        if (wanted == SPLying)
            goto lie;
        line = 243;
        break;
    case SPLying:
        if (wanted == SPCrouching)
            goto crouch;
        if (wanted == SPStanding)
            goto lie;
        if (wanted == SPLying)
            goto none;
        line = 283;
        break;
    default:
        line = 288;
        break;
    }
    Debug(DebugTypeError, "AI/AIDLL/AI/AIPlanInterpreter/InfanteryControl.cpp", line, "")
        << "Internal error, unknown pose.";
    return 0;

none:
    input->setInput(io::PICrouch, 0.0f);
    input->setInput(io::PILie, 0.0f);
    return 1;
crouch:
    input->setInput(io::PICrouch, 1.0f);
    input->setInput(io::PILie, 0.0f);
    return 1;
lie:
    input->setInput(io::PICrouch, 0.0f);
    input->setInput(io::PILie, 1.0f);
    return 1;
}

// ELF 0x2CBC90
bool needsUpdate(const Vec2& point, unsigned int handle, float radius)
{
    Information* info = lookupInformation(handle);
    if (info == 0)
        return false;
    Vec2 position;
    position.x = 0.0f;
    position.y = 0.0f;
    vec3ToVec2XZ(info->getPosition(), position);
    const float distSqr = (point.y - position.y) * (point.y - position.y)
                        + (point.x - position.x) * (point.x - position.x);
    return distSqr < radius * radius || (radius + 1.5f) * (radius + 1.5f) < distSqr;
}

// ELF 0x2CBD70. Picks the nearest obstacle in front of the bot on the way to `target` (from the
// navigation's obstacle list: physical objects that are mobile or flagged 0x2000000, within 2 m of
// the bot's height and inside their radius + 1 of the path) and returns in `avoidPos` a point beside
// it: the obstacle position moved along the path normal by at most its radius, to the side where a
// pathfinding trace (15 m each way from the obstacle) has room. The obstacle's handle, position,
// radius and "damaged" state (flag 0x2000000 or health under the bail value) are returned through
// the other references; repair and change targets are never treated as damaged.
// Returns false in every normal case (also when there is no obstacle); true only when the obstacle
// stands on no valid pathfinding position (a retail quirk the callers rely on).
bool updateInfantryMoveToPosWithObstacles(Bot* bot, const Vec2& target, const Vec3&, unsigned int targetHandle,
                                          Vec2& avoidPos, unsigned int& obstacleHandle, float& avoidOffset,
                                          Vec2& obstaclePos, float& obstacleRadius, bool& obstacleDamaged,
                                          Information* ignored, bool includeIgnored)
{
    Information* botInfo = lookupInformation(bot->getCurrentEquipment());
    BotNavigation* navigation = bot->getBotNavigation();
    float bestAlong = 99999.0f;
    Vec2 closest;
    closest.x = 0.0f;
    closest.y = 0.0f;
    const Vec3& botPos3 = botInfo->getPosition();
    Vec2 botPos;
    botPos.x = 0.0f;
    botPos.y = 0.0f;
    vec3ToVec2XZ(botPos3, botPos);
    Vec2 dir;
    dir.x = target.x - botPos.x;
    dir.y = target.y - botPos.y;
    const float length = std::sqrt(dir.y * dir.y + dir.x * dir.x);
    if (length < 0.0001f)
        return false;
    dir.x = dir.x * (1.0f / length);
    dir.y = (1.0f / length) * dir.y;
    Vec2 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    getNormal(dir, normal);

    IPIMobile* bestMobile = 0;
    Information* targetInfo = lookupInformation(targetHandle);
    Vec3 bestPos3;
    bestPos3.x = 0.0f;
    bestPos3.y = 0.0f;
    bestPos3.z = 0.0f;
    if (targetInfo != 0 && !targetInfo->isPrimary())
        targetInfo->getPrimaryObject(bot->getSide());   // retail: the result is dropped
    avoidOffset = 0.0f;

    unsigned int changeTarget = bot->getChangeTarget();
    Information* changeInfo = lookupInformation(changeTarget);
    if (changeInfo != 0 && !changeInfo->isPrimary())
        changeTarget = changeInfo->getPrimaryObject(bot->getSide());
    unsigned int repairTarget = bot->getRepairTarget();
    Information* repairInfo = lookupInformation(repairTarget);
    if (repairInfo != 0 && !repairInfo->isPrimary())
        repairTarget = repairInfo->getPrimaryObject(bot->getSide());

    const std::vector<unsigned int>& obstacles = navigation->getObstacles();   // 0x2CBE2E: BotNavigation +0x58
    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        Information* info = lookupInformation(obstacles[i]);
        if (info == botInfo)
            continue;
        if (!(info != ignored || includeIgnored) || info == 0)
            continue;
        IPIPhysical* physical = info->getPhysical();
        if (physical == 0 || info->isOfType(ITSoldier))
            continue;
        IPIMobile* mobile = info->getMobile();
        if (mobile == 0 && !info->isOfType(ITObstacle))
            continue;

        bool isTarget = false;
        if (obstacles[i] == repairTarget || obstacles[i] == changeTarget)
            isTarget = true;
        const float health = physical->getHealth();
        bool damaged = false;
        if (info->isOfType(ITObstacle) || health < IBotManager::instance->getVehicleBailValue())
            damaged = true;
        float radius;
        if (!damaged || isTarget)
        {
            radius = physical->getSmallestRadius();
        }
        else
        {
            const float smallest = physical->getSmallestRadius();
            float grown = physical->getSmallestRadius() + 4.0f;
            if (10.0f < grown)
                grown = 10.0f;
            if (grown <= smallest)
                grown = smallest;
            radius = grown * 1.3f;
        }

        if (!(std::fabs(info->getPosition().y - botPos3.y) <= 2.0f))
            continue;
        Vec2 pos;
        pos.x = 0.0f;
        pos.y = 0.0f;
        vec3ToVec2XZ(info->getPosition(), pos);
        const float rx = pos.x - botPos.x;
        const float ry = pos.y - botPos.y;
        const float distSqr = rx * rx + ry * ry;
        const float along = rx * dir.x + ry * dir.y;
        if (!(distSqr <= (radius + 1.0f) * (radius + 1.0f)))
            continue;
        if (!(0.1f <= along / std::sqrt(distSqr)))
            continue;
        if (radius + 1.0f <= std::fabs(rx * normal.x + ry * normal.y))
            continue;
        if (bestAlong <= along)
            continue;
        closest = pos;
        obstacleRadius = radius;
        bestPos3 = info->getPosition();
        obstacleHandle = obstacles[i];
        obstaclePos = pos;
        obstacleDamaged = damaged;
        bestMobile = mobile;
        bestAlong = along;
    }
    if (99999.0f <= bestAlong)
        return false;

    const int vehicleNumber = botInfo->getMobile()->getVehicleNumber();
    bot->getBotNavigation();   // retail: called and dropped
    const int equipmentType = botInfo->getUnit()->getEquipmentType();
    Vec3 hitBack;
    hitBack.x = hitBack.y = hitBack.z = 0.0f;
    Vec3 hitFront;
    hitFront.x = hitFront.y = hitFront.z = 0.0f;
    Vec3 normal3;
    normal3.x = normal3.y = normal3.z = 0.0f;
    vec2ToVec3XZ(normal, normal3);
    IAIPathfinding* pathfinding = IAIPathfinding::instance;
    unsigned int polygon;   // trace output, unused
    float frontDist;
    if (bestMobile == 0)
    {
        if (!pathfinding->isValidPosition(vehicleNumber, equipmentType, bestPos3, true))
            return true;
        Vec3 to;
        to.x = normal3.x * 15.0f + bestPos3.x;
        to.y = normal3.y * 15.0f + bestPos3.y;
        to.z = normal3.z * 15.0f + bestPos3.z;
        pathfinding->trace(vehicleNumber, bestPos3, to, hitFront, polygon, equipmentType, -1, 0.0f, 0, 0);
        frontDist = isZeroVec(hitFront) ? 0.0f : distance(hitFront, bestPos3);
        to.x = bestPos3.x - normal3.x * 15.0f;
        to.y = bestPos3.y - normal3.y * 15.0f;
        to.z = bestPos3.z - normal3.z * 15.0f;
        pathfinding->trace(vehicleNumber, bestPos3, to, hitBack, polygon, equipmentType, -1, 0.0f, 0, 0);
    }
    else
    {
        const IPIMobile::ValidPos& valid = bestMobile->getLastValidPosition(vehicleNumber);
        Vec3 to;
        to.x = normal3.x * 15.0f + valid.position.x;
        to.y = normal3.y * 15.0f + valid.position.y;
        to.z = normal3.z * 15.0f + valid.position.z;
        pathfinding->trace(valid.polygon, valid.island, vehicleNumber, valid.position, to, hitFront, polygon,
                           equipmentType, -1, 0.0f, 0, 0);
        // retail: measured from the obstacle's position, not from the valid position traced from
        frontDist = isZeroVec(hitFront) ? 0.0f : distance(hitFront, bestPos3);
        to.x = valid.position.x - normal3.x * 15.0f;
        to.y = valid.position.y - normal3.y * 15.0f;
        to.z = valid.position.z - normal3.z * 15.0f;
        pathfinding->trace(valid.polygon, valid.island, vehicleNumber, valid.position, to, hitBack, polygon,
                           equipmentType, -1, 0.0f, 0, 0);
    }
    const float backDist = isZeroVec(hitBack) ? 0.0f : distance(hitBack, bestPos3);

    avoidPos = closest;
    bool goBack;
    float room = frontDist;
    float radius;
    if ((closest.y - botPos.y) * normal.y + (closest.x - botPos.x) * normal.x <= 0.0f)
    {
        radius = obstacleRadius;
        goBack = !(radius + 1.0f < frontDist || backDist < frontDist);
    }
    else
    {
        radius = obstacleRadius;
        goBack = !(backDist <= radius + 1.0f && backDist <= frontDist);
    }
    if (goBack)
        room = backDist;
    float offset = radius;
    if (room < offset)
        offset = room;
    const float signedOffset = goBack ? -offset : offset;
    avoidPos.x = normal.x * signedOffset + avoidPos.x;
    avoidPos.y = signedOffset * normal.y + avoidPos.y;
    avoidOffset = offset;
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
