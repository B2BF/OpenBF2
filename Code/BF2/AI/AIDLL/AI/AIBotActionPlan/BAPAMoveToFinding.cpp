// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1DA4D0 (__tcf_0 0x1DA530, _GLOBAL__I 0x1DA560): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMoveToFinding.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/SquadManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include "../AIBot/SquadMember.h"

#include <cmath>
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D9880 (C1; C2 0x1D96E0). The bot's getCurrentEquipment() result is discarded.
BAPAMoveToFinding::BAPAMoveToFinding(Bot* bot, const Vec3& coordinate, float maxSpeed, float maxArriveSpeed,
                                     BAPCondition* endCondition, unsigned long long controls, int patternId,
                                     float maxDistance, float minDistance)
    : BAPAMoveTo(bot)
{
    m_flagD4 = false;
    m_flagD5 = false;
    m_intD8 = 1;
    for (int i = 0; i < 8; ++i)
        m_zeroB4[i] = 0;
    m_bot = bot;
    m_maxDistance = maxDistance;
    m_minDistance = minDistance;
    m_maxDistanceSqr = maxDistance * maxDistance;
    m_minDistanceSqr = minDistance * minDistance;
    m_pathDone = false;
    bot->getCurrentEquipment();
    m_patternId = patternId;
    m_coordinate = coordinate;
    m_maxSpeed = maxSpeed;
    m_maxArriveSpeed = maxArriveSpeed;
    m_endCondition = endCondition;
    if (endCondition)
        endCondition->setOwner(this);
    m_previousSpeed = 0.0f;
    m_pathValid = true;
    m_pathStarted = false;
    m_controls = controls;
    m_alternative.x = 0.0f;
    m_alternative.y = 0.0f;
}

// ELF 0x1D9A60 (D0 0x1D9AA0, thunk 0x1DA5A0): base only.
BAPAMoveToFinding::~BAPAMoveToFinding()
{
}

// ELF 0x1D9AF0: runs only through the move entries.
bool BAPAMoveToFinding::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1DA420
void BAPAMoveToFinding::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    out << "MoveToFinding(" << static_cast<double>(m_minDistance) << ", " << static_cast<double>(m_maxDistance)
        << ")" << std::endl;
}

// ELF 0x1D9B10. Next path point into `point`; `reached`/`done` as the entries expect (the float
// argument is not read). A bot that waits for repairs, or whose vehicle is in the air but not in
// water, stops (reached = false, done = true). Pathfinding restarts when the goal moved or the
// search is not running; inside the ring [min + 1] round the goal a valid ring point is searched
// in 0.5 rad steps. Finally artillery: a target point in a danger zone the bot is not in stops
// the move; a soldier in a danger zone whose squad member avoids artillery sprints.
int BAPAMoveToFinding::update(Vec2& point, bool& reached, bool& done, float)
{
    Information* self = bapinline::info(m_bot->getCurrentEquipment());
    if (static_cast<BotMain*>(m_bot)->getShouldWaitForRepairs()
        || (self->getPhysical()->isInAir() && !self->getPhysical()->isInWater())) {
        reached = false;
        done = true;
        return 1;
    }

    bool restart = !m_pathStarted;
    if (!restart) {
        BotNavigation* nav = m_bot->getBotNavigation();
        const Vec3& goal = nav->getGoal();
        const bool sameGoal = std::fabs(m_coordinate.x - goal.x) <= 1.1920929e-07f
                              && std::fabs(m_coordinate.y - goal.y) <= 1.1920929e-07f
                              && std::fabs(m_coordinate.z - goal.z) <= 1.1920929e-07f;
        restart = !sameGoal || !m_bot->getBotNavigation()->isSearchActive();
    }
    if (restart) {
        m_pathStarted = true;
        m_pathValid = m_bot->getBotNavigation()->initPathfinding(m_coordinate, 0.0f, m_maxDistance);
    }

    bool result = false;
    if (m_pathValid
        && m_bot->getBotNavigation()->updatePath(m_currentPoint, m_pathDone, done, 0.0f)) {
        result = true;
        point = m_currentPoint;
        reached = m_pathDone;
    }

    const float distance = getSimpleTargetDistance(m_bot);
    if (!(m_minDistance + 1.0f > distance) || !(m_minDistance > 1.0f)) {     // 0x1D9C57 / 0x1D9C61 jbe
        if (done)
            goto finish;
        if (isFinished(m_bot)) {
            m_pathValid = true;
            reached = true;
            m_pathDone = true;
            point = m_currentPoint;
            result = true;
        }
    } else {
        if (1.1920929e-07f < std::fabs(m_alternative.y - 0.0f) + std::fabs(m_alternative.x - 0.0f)) {
            IPIMobile* mobile = self->getMobile();
            const int equipment = self->getUnit()->getEquipmentType();
            float angle = 0.0f;
            bool valid;
            do {
                const float s = std::sin(angle);
                const float c = std::cos(angle);
                const float r = m_minDistance + 1.0f;
                Vec3 p = bapinline::vec3(s * r + m_coordinate.x, r * 0.0f + m_coordinate.y, r * c + m_coordinate.z);
                valid = IAIPathfinding::instance->isValidPosition(mobile->getVehicleNumber(), equipment, p, true);
                if (valid)
                    vec3ToVec2XZ(p, m_alternative);
                angle += 0.5f;
                if (6.2831855f <= angle) {
                    if (!valid)
                        return 0;
                    break;
                }
            } while (!valid);
        }
        point = m_alternative;
    }

    if (!done) {
        const Vec3 target = bapinline::vec3(point.x, self->getPosition().y, point.y);
        double sinceStart;
        int artillerySide;
        if (!self->isOfType(ITAir)) {
            const int side = m_bot->getSide();
            if (SquadManager::m_instance->isInArtilleryDangerZone(target, side, sinceStart, artillerySide)
                && !SquadManager::m_instance->isInArtilleryDangerZone(self->getPosition(), m_bot->getSide(),
                                                                      sinceStart, artillerySide)) {
                done = true;
                goto finish;
            }
        }
        if (self->isOfType(ITSoldier)) {
            SquadMember* member;
            if (SquadManager::m_instance->isInArtilleryDangerZone(self->getPosition(), m_bot->getSide(), sinceStart,
                                                                  artillerySide)
                && (member = m_bot->getSquadMember()) != 0 && member->isAvoidingArtillery())
                setIsSprinting(m_bot, true, target, 0xFFFFFFFFu);
        }
    }

finish:
    if (!result)
        m_pathStarted = false;
    return result;
}

// ELF 0x1DA3B0: end condition or within distance; then BotNavigation::clearProgress.
bool BAPAMoveToFinding::isFinished(Bot* bot)
{
    if ((!m_endCondition || !m_endCondition->evaluate(bot)) && !withinDistance())
        return false;
    bot->getBotNavigation()->clearProgress();
    return true;
}

// ELF 0x1DA260
bool BAPAMoveToFinding::getSecondPoint(Vec2& point)
{
    return m_bot->getBotNavigation()->getSecondPoint(point);
}

// ELF 0x1DA2B0
bool BAPAMoveToFinding::getLargestAngleDiff(float& angle)
{
    return m_bot->getBotNavigation()->getLargestAngleDiff(angle);
}

// ELF 0x1DA300: BotNavigation +0xBC.
float BAPAMoveToFinding::getTargetDistance()
{
    return m_bot->getBotNavigation()->getTargetDistance();
}

// ELF 0x1DA5C0
int BAPAMoveToFinding::usesPathfinding()
{
    return 1;
}

// ELF 0x1DA360: min^2 <= d^2 <= max^2.
int BAPAMoveToFinding::withinDistance()
{
    const float d = getSimpleTargetDistanceSqr(m_bot);
    return (d <= m_maxDistanceSqr && m_minDistanceSqr <= d) ? 1 : 0;
}

// ELF 0x1DA330: BotNavigation +0xC0.
bool BAPAMoveToFinding::isStuck()
{
    return m_bot->getBotNavigation()->getIsStuck();
}

} // namespace ai
} // namespace hfe
} // namespace dice
