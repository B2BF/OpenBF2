// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1DD560 (__tcf_0 0x1DD5C0, _GLOBAL__I 0x1DD5F0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMoveToObjectFinding.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIKnowledge/Information.h"
#include "../AIBot/SensedObject.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"

#include <cmath>
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DB1A0 (C1; C2 0x1DB030)
BAPAMoveToObjectFinding::BAPAMoveToObjectFinding(Bot* bot, unsigned int object, float distance, float maxSpeed,
                                                 float maxArriveSpeed, BAPCondition* endCondition,
                                                 unsigned long long controls, int patternId,
                                                 BAPCondition* replanCondition, float minDistance, int index)
    : BAPAMoveToObject(bot, object, distance, maxSpeed, maxArriveSpeed, endCondition, controls, patternId)
{
    m_flagE9 = false;
    m_intEC = 1;
    m_bot = bot;
    for (int i = 0; i < 8; ++i)
        m_zeroC8[i] = 0;
    m_flagF8 = true;
    m_replanCondition = replanCondition;
    m_pathDone = false;
    m_movingAway = false;
    m_pathStarted = false;
    m_minDistance = minDistance;
    m_minDistanceSqr = minDistance * minDistance;
    m_index = index;
    m_lastInitTime = -1.0;
    if (m_distance < minDistance) {
        m_minDistance = m_distance;
        m_minDistanceSqr = m_distanceSqr;
    }
    if (m_replanCondition)
        m_replanCondition->setOwner(this);
    m_flag107 = false;
    m_pathValid2 = true;
}

// ELF 0x1DC0C0 (D1; D0 0x1DC140, thunk 0x1DD630): deletes the replan condition.
BAPAMoveToObjectFinding::~BAPAMoveToObjectFinding()
{
    if (m_replanCondition)
        delete m_replanCondition;
}

// ELF 0x1DD430
void BAPAMoveToObjectFinding::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    out << "MoveToObjectFinding";
    if (m_index != -1)
        out << "[" << m_index << "]";
    out << "(" << static_cast<double>(m_minDistance) << ", " << static_cast<double>(m_distance) << ")" << std::endl;
}

// ELF 0x1DD660
bool BAPAMoveToObjectFinding::getIsMovingAway()
{
    return m_movingAway;
}

namespace {

bool sameVec3(const Vec3& a, const Vec3& b)
{
    return std::fabs(a.x - b.x) <= 1.1920929e-07f && std::fabs(a.y - b.y) <= 1.1920929e-07f
           && std::fabs(a.z - b.z) <= 1.1920929e-07f;
}

} // namespace

// ELF 0x1DC1C0. Path to the object's last valid position. Fails (m_pathValid2 = false, 0) without
// the bot's or the object's information, the object's primary mobile, a valid position for the
// object, or (soldier, close in height) an active weapon; a bot without a valid position is
// marked stuck. A bot waiting for repairs or airborne out of water: reached = false, done, 1.
// Re-plans (initObjectFinding) when: the object is sensed only indirectly and its last valid
// position moved (once, m_flag107, and not while flanking with the path unfinished); the
// navigation goal is not m_coordinate; every second, when closer than m_minDistance and not
// flanking, or flanking with the target within m_minDistance of the goal and the path done, or
// with the path done, not within distance and the target moved; the search is not running
// (a fresh start: zero direction/origin, flag true); or every second when the re-plan condition
// holds (after it was false, or with the path done). Then updatePath (the float is passed on);
// a target point inside an artillery zone the bot is not in stops the move (done). A soldier
// chasing (not flanking) within 2 m of height sprints at the object while the path is shorter
// than its physical's radius + 25 (unless firing a weapon with range > 5 that is not type 4),
// otherwise stops sprinting; in an artillery zone with an avoiding squad member it sprints at
// the point. isFinished then completes the move.
int BAPAMoveToObjectFinding::update(Vec2& point, bool& reached, bool& done, float distance)
{
    BotMain* bot = static_cast<BotMain*>(m_bot);
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (!self || !m_pathValid2) {
        m_pathValid2 = false;
        return 0;
    }
    if (bot->getShouldWaitForRepairs() || (self->getPhysical()->isInAir() && !self->getPhysical()->isInWater())) {
        reached = false;
        done = true;
        return 1;
    }
    Information* target = bapinline::info(m_object);
    if (!target) {
        m_pathValid2 = false;
        return 0;
    }
    IPIMobile* mobile = self->getMobile();
    const IPIMobile::ValidPos& own = mobile->getLastValidPosition(mobile->getVehicleNumber());
    Information* primary = bapinline::info(target->getPrimaryObject(self->getSide()));
    if (!primary || !primary->getMobile()) {
        m_pathValid2 = false;
        return 0;
    }
    const IPIMobile::ValidPos& goal = primary->getMobile()->getLastValidPosition(mobile->getVehicleNumber());
    if (own.polygon == 0xFFFFFFFFu) {
        bot->getBotNavigation()->setIsStuck(true);
        m_pathValid2 = false;
        return 0;
    }
    if (goal.polygon == 0xFFFFFFFFu) {
        m_pathValid2 = false;
        return 0;
    }
    Vec3 dir = bapinline::sub(goal.position, own.position);
    float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (0.0001f < dist) {
        const float inv = 1.0f / dist;
        dir.x *= inv;
        dir.y *= inv;
        dir.z = inv * dir.z;
    }
    SensedObject* sensed = bot->getSensedObjectFromHandle(m_object);
    const bool indirect = sensed ? (sensed->m_flags & 8u) != 0 : true;
    if (!indirect)
        m_flag107 = false;
    const bool within = withinDistance() != 0;
    const bool pathDone = m_pathDone;
    const bool replanDue = 1.0 < bapinline::gameTime() - m_lastInitTime;

    // 0: no re-plan; 1: towards the target (dist, dir, own position, false); 2: fresh start.
    int replan = 0;
    bool checkNavigation = true;
    if (indirect) {
        if (!sameVec3(m_coordinate, goal.position) && !m_flag107 && !(m_movingAway && !pathDone)) {
            m_flag107 = true;
            replan = 1;
            checkNavigation = false;
        }
    }
    if (checkNavigation) {
        bool searchCheck = true;
        if (!sameVec3(m_coordinate, bot->getBotNavigation()->getGoal())) {
            replan = 1;
            searchCheck = false;
        } else if (replanDue) {
            if (m_movingAway || !(m_minDistance > dist)) {     // 0x1DCCAB ja
                if (m_movingAway) {
                    const Vec3 d = bapinline::sub(goal.position, m_coordinate);
                    if (d.x * d.x + d.y * d.y + d.z * d.z < m_minDistance * m_minDistance && pathDone) {
                        replan = 1;
                        searchCheck = false;
                    }
                }
                if (searchCheck && pathDone && !within && !sameVec3(m_coordinate, goal.position)) {
                    replan = 1;
                    searchCheck = false;
                }
            } else {
                replan = 1;
                searchCheck = false;
            }
        }
        if (searchCheck) {
            if (!m_pathStarted || !bot->getBotNavigation()->isSearchActive()) {
                replan = 2;
            } else if (replanDue && m_replanCondition && m_replanCondition->evaluate(m_bot)) {
                if (!m_flagF8 || m_pathDone) {
                    m_flagF8 = true;
                    replan = 1;
                }
            } else {
                m_flagF8 = false;
            }
        }
    }
    if (replan == 1) {
        initObjectFinding(dist, dir, own.position, goal.quadTree, goal.polygon, goal.position, false);
    } else if (replan == 2) {
        const Vec3 zero = bapinline::vec3(0.0f, 0.0f, 0.0f);
        initObjectFinding(0.0f, zero, zero, goal.quadTree, goal.polygon, goal.position, true);
    }

    m_pathStarted = true;
    if (!m_pathValid2)
        return 0;
    if (!m_movingAway)
        bot->getBotNavigation()->updateLastPoint(primary);
    const bool got = bot->getBotNavigation()->updatePath(m_currentPoint, m_pathDone, done, distance);
    bool stopped = false;
    const Vec3 target3 = bapinline::vec3(m_currentPoint.x, self->getPosition().y, m_currentPoint.y);
    double sinceStart;
    int artillerySide;
    if (got) {
        point = m_currentPoint;
        reached = m_pathDone;
        if (!(self->getObjectTypeFlags() & 0x10u)
            && SquadManager::m_instance->isInArtilleryDangerZone(target3, bot->getSide(), sinceStart, artillerySide)
            && !SquadManager::m_instance->isInArtilleryDangerZone(self->getPosition(), bot->getSide(), sinceStart,
                                                                  artillerySide)) {
            done = true;
            stopped = true;
        }
    }
    if (!done) {
        const bool soldier = (self->getObjectTypeFlags() & 0x200000u) != 0;
        bool artilleryCheck = true;
        if (!stopped && !m_movingAway && soldier
            && std::fabs(target->getPosition().y - self->getPosition().y) < 2.0f) {
            artilleryCheck = false;
            const float pathLeft = bot->getBotNavigation()->getTargetDistance();
            IPIPhysical* physical = target->getPhysical();
            IPIArmament* armament = self->getArmament();
            if (!armament || armament->getActiveWeapon() == -1) {
                m_pathValid2 = false;
                return 0;
            }
            WeaponInfo* weapon =
                WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
            if (!weapon) {
                m_pathValid2 = false;
                return 0;
            }
            const Vec3 zero = bapinline::vec3(0.0f, 0.0f, 0.0f);
            if ((!bot->isFiring() || !(weapon->getMaxRange() > 5.0f) ||     /* 0x1DCAB4 jbe */ weapon->getWeaponType() == 4)
                && pathLeft < physical->getRadius() + 25.0f)
                setIsSprinting(m_bot, true, zero, m_object);
            else
                setIsSprinting(m_bot, false, zero, 0xFFFFFFFFu);
        }
        if (artilleryCheck && soldier
            && SquadManager::m_instance->isInArtilleryDangerZone(self->getPosition(), bot->getSide(), sinceStart,
                                                                 artillerySide)) {
            SquadMember* member = bot->getSquadMember();
            if (member && member->isAvoidingArtillery())
                setIsSprinting(m_bot, true, target3, 0xFFFFFFFFu);
        }
        if (!done && isFinished(m_bot)) {
            m_pathValid2 = true;
            reached = true;
            m_pathDone = true;
            point = m_currentPoint;
            return 1;
        }
    }
    if (!got) {
        m_pathStarted = false;
        return 0;
    }
    return 1;
}

// ELF 0x1DD340: end condition, or (when moving away) path done and within distance, or
// (otherwise) within distance; then BotNavigation::clearProgress.
bool BAPAMoveToObjectFinding::isFinished(Bot* bot)
{
    bool ended = false;
    if (m_endCondition && m_endCondition->evaluate(bot))
        ended = true;
    bool finished;
    if (!m_movingAway)
        finished = ended || withinDistance();
    else
        finished = ended || (m_pathDone && withinDistance());
    if (finished)
        bot->getBotNavigation()->clearProgress();
    return finished;
}

// ELF 0x1DD200
bool BAPAMoveToObjectFinding::getLargestAngleDiff(float& angle)
{
    return m_bot->getBotNavigation()->getLargestAngleDiff(angle);
}

// ELF 0x1DD1D0: BotNavigation +0xBC.
float BAPAMoveToObjectFinding::getTargetDistance()
{
    return m_bot->getBotNavigation()->getTargetDistance();
}

// ELF 0x1DD020 (same body as BAPAMoveToObject's)
float BAPAMoveToObjectFinding::getSimpleTargetDistance(Bot* bot)
{
    return std::sqrt(BAPAMoveToObjectFinding::getSimpleTargetDistanceSqr(bot));
}

// ELF 0x1DD110: neither information is null-checked.
float BAPAMoveToObjectFinding::getSimpleTargetDistanceSqr(Bot* bot)
{
    const Vec3& a = bapinline::info(bot->getCurrentEquipment())->getPosition();
    const Vec3& b = bapinline::info(m_object)->getPosition();
    const double dz = static_cast<double>(a.z) - b.z;
    const double dx = static_cast<double>(a.x) - b.x;
    const double dy = static_cast<double>(a.y) - b.y;
    return static_cast<float>(dz * dz + dx * dx + dy * dy);
}

// ELF 0x1DD650
int BAPAMoveToObjectFinding::usesPathfinding()
{
    return 1;
}

// ELF 0x1DD250: min^2 <= d^2 <= m_distance^2.
int BAPAMoveToObjectFinding::withinDistance()
{
    const float d = getSimpleTargetDistanceSqr(m_bot);
    return (d <= m_distanceSqr && m_minDistanceSqr <= d) ? 1 : 0;
}

// ELF 0x1DB310: the horizontal right of a direction, (z, y, -x) - written y, x, z in that order
// (`mov 4(%edx)->4(%ecx); mov 8(%edx)->(%ecx); xor $0x80000000 of (%edx) -> 8(%ecx)`). A global
// function of this TU (called through the PLT).
void getRight(const Vec3& direction, Vec3& right)
{
    right.y = direction.y;
    right.x = direction.z;
    right.z = -direction.x;
}

// ELF 0x1DB360 (0xDE0 bytes). `distance` is the current distance to the object, `ownPosition`
// the bot's, `goal` the object's last valid position with its quad tree / polygon; the direction
// argument is unused. Both path flags cleared; no bot or object information: m_pathValid2 =
// false and nothing else. The arrive distance is 1.1 * the object's physical radius (FLT_MAX
// without one). Then:
// * at or beyond m_minDistance (0x1DB496 `fucompi; jbe`) or `fresh`: no sprint, straight at the
//   goal with the caller's quad tree / polygon.
// * nearer: three flank offsets of length d = min((m_distance - m_minDistance) * 0.2, 10) +
//   m_minDistance round the goal - a soldier moving faster than 1.5 m/s horizontally (the object's
//   mobile, or its primary's; not null-checked): the side of its velocity facing the bot
//   (0x1DBD7E `setae`: flipped when dot < 0), its opposite and the right of the unflipped side;
//   a soldier otherwise: away from the goal towards the bot, then its right and left; anything
//   else (a vehicle): along the object's transformation row 2, then right and left. Sprinting
//   from the bot's position, each offset is traced (IAIPathfinding slot +0x90, 0x1DB82A) until
//   one is clear; the goal is 98 % of the way to the clear trace end, or else to the blocked end
//   farthest from the goal (zero / -1 if none is farther than 0), with the trace's polygon and
//   BotNavigation +0xCC as the island; its height is then the terrain's (slot +0x14).
// Then BotNavigation::initPathfinding(island, polygon, goal, arrive, m_distance) and the time.
void BAPAMoveToObjectFinding::initObjectFinding(float distance, const Vec3&, const Vec3& ownPosition,
                                                unsigned int quadTree, unsigned int polygon, const Vec3& goal,
                                                bool fresh)
{
    m_movingAway = false;
    m_pathDone = false;
    Information* self = bapinline::info(m_bot->getCurrentEquipment());
    if (!self) {
        m_pathValid2 = false;
        return;
    }
    Information* target = bapinline::info(m_object);
    if (!target) {
        m_pathValid2 = false;
        return;
    }
    IPIMobile* mobile = target->getMobile();
    if (!mobile && !target->isPrimary()) {
        Information* primary = bapinline::info(target->getPrimaryObject(m_bot->getSide()));
        if (primary)
            mobile = primary->getMobile();
    }
    float arrive = 3.4028235e+38f;
    if (IPIPhysical* physical = target->getPhysical())
        arrive = physical->getRadius() * 1.1f;     // fmuls by the float constant 0x3F8CCCCD
    const int equipmentType = self->getUnit()->getEquipmentType();

    if (!(m_minDistance > distance) || fresh) {     // 0x1DB496 jbe
        m_movingAway = false;
        setIsSprinting(m_bot, false, bapinline::vec3(0.0f, 0.0f, 0.0f), 0xFFFFFFFFu);
        m_coordinate = goal;
    } else {
        Vec3 offsets[3];
        for (int i = 0; i < 3; ++i)
            offsets[i] = bapinline::vec3(0.0f, 0.0f, 0.0f);
        const Vec3& speed = mobile->getSpeed();
        Vec3 flat = bapinline::vec3(speed.x, 0.0f, speed.z);
        const float flatSpeed = std::sqrt(flat.x * flat.x + 0.0f * 0.0f + flat.z * flat.z);
        const bool soldier = (self->getObjectTypeFlags() & 0x200000u) != 0;
        if (!soldier || !(flatSpeed > 1.5f)) {     // 0x1DB53E ja
            float d = (m_distance - m_minDistance) * 0.2f;
            if (10.0f < d)
                d = 10.0f;
            d = d + m_minDistance;
            if (!soldier) {
                const Mat4& t = self->getTransformation();
                offsets[0] = bapinline::vec3(t.m[8] * d, t.m[9] * d, d * t.m[10]);
            } else {
                Vec3 away = bapinline::sub(ownPosition, goal);
                bapinline::normalize(away);
                offsets[0] = bapinline::vec3(away.x * d, away.y * d, d * away.z);
            }
            getRight(offsets[0], offsets[1]);
            offsets[2] = bapinline::vec3(-offsets[1].x, -offsets[1].y, -offsets[1].z);
        } else {
            const float inv = 1.0f / flatSpeed;
            flat = bapinline::vec3(flat.x * inv, flat.y * inv, inv * flat.z);
            Vec3 side = bapinline::vec3(0.0f, 0.0f, 0.0f);
            getRight(flat, side);
            const bool behind = !(bapinline::dot(bapinline::sub(ownPosition, goal), side) >= 0.0f);     // 0x1DBD7A setae
            if (behind)
                side = bapinline::vec3(-side.x, -side.y, -side.z);
            float d = (m_distance - m_minDistance) * 0.2f;
            if (10.0f < d)
                d = 10.0f;
            d = d + m_minDistance;
            offsets[0] = bapinline::vec3(side.x * d, side.y * d, d * side.z);
            offsets[1] = bapinline::vec3(-offsets[0].x, -offsets[0].y, -offsets[0].z);
            const Vec3 unflipped = behind ? offsets[1] : offsets[0];
            getRight(unflipped, offsets[2]);
        }

        m_movingAway = true;
        setIsSprinting(m_bot, true, self->getPosition(), 0xFFFFFFFFu);
        BotNavigation* navigation = m_bot->getBotNavigation();
        float bestDistSqr = 0.0f;
        unsigned int bestPolygon = 0xFFFFFFFFu;
        Vec3 end = bapinline::vec3(0.0f, 0.0f, 0.0f);
        Vec3 best = bapinline::vec3(0.0f, 0.0f, 0.0f);
        unsigned int endPolygon;
        bool clear;
        int i = 0;
        do {
            const Vec3 candidate = bapinline::add(offsets[i], goal);
            clear = IAIPathfinding::instance->trace(navigation, false, candidate, end, endPolygon, equipmentType, -1,
                                                    0.0f, 0, 0);
            if (!clear) {
                const Vec3 back = bapinline::sub(goal, end);
                const float distSqr = back.x * back.x + back.y * back.y + back.z * back.z;
                if (bestDistSqr < distSqr) {
                    best = bapinline::vec3(goal.x + (end.x - goal.x) * 0.98f, goal.y + (end.y - goal.y) * 0.98f,
                                           goal.z + (end.z - goal.z) * 0.98f);
                    bestPolygon = endPolygon;
                    bestDistSqr = distSqr;
                }
            }
            ++i;
        } while (!clear && i < 3);
        quadTree = navigation->getLastValidIsland();
        if (!clear) {
            m_coordinate = best;
            polygon = bestPolygon;
        } else {
            m_coordinate = bapinline::vec3(goal.x + (end.x - goal.x) * 0.98f, goal.y + (end.y - goal.y) * 0.98f,
                                           goal.z + (end.z - goal.z) * 0.98f);
            polygon = endPolygon;
        }
    }

    m_previousSpeed = 0.0f;
    m_flagEA = false;
    if (m_movingAway)
        m_coordinate.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                             ->getTerrainAltitude(m_coordinate.x, m_coordinate.z);
    m_pathValid2 = m_bot->getBotNavigation()->initPathfinding(quadTree, polygon, m_coordinate, arrive, m_distance);
    m_lastInitTime = bapinline::gameTime();
}

// ELF 0x1DD2A0: the bot's current vehicle/soldier is within 1 m of the goal (not null-checked).
bool BAPAMoveToObjectFinding::isAtCoordinate()
{
    const Vec3& p = bapinline::info(m_bot->getCurrentEquipment())->getPosition();
    return bapinline::lengthSqr(bapinline::sub(p, m_coordinate)) < 1.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
