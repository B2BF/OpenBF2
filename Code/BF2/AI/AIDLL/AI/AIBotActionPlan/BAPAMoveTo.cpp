// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D6700 (__tcf_0 0x1D6760, _GLOBAL__I 0x1D6790): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMoveTo.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIPlanInterpreter/InfanteryControl.h"
#include "../Main/AIRandom.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/SensingAgent.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The constructor's fire threshold: `rnd * 2^-32 * 14 + 1` in x87 precision, stored as float.
float randomFireThreshold()
{
    const unsigned int r = AIRandom::m_instance->next();
    return static_cast<float>(static_cast<double>(r) * static_cast<double>(2.3283064e-10f) * 14.0 + 1.0);
}

inline float length2(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

} // namespace

// ELF 0x1D5310 (C1; C2 0x1D5150). Pose chooser: stand, lie under incoming fire above a random
// threshold in [1, 15), crouch in danger, no control threshold, no object, 10 s between changes.
// Ends with BotMain::setMoveToStatus(0) (Bot vtable +0x244).
BAPAMoveTo::BAPAMoveTo(Bot* bot)
    : BAPAction(UninitializedPatternId()),
      BAPAComponentSoldierPose(SPStanding, SPLying, randomFireThreshold(), SPCrouching, 0.0f, 0xFFFFFFFFu, 10.0f)
{
    m_coordinate = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_isSprinting = false;
    m_previousSpeed = 0.0f;
    m_currentPoint.x = 0.0f;
    m_currentPoint.y = 0.0f;
    m_vec74.x = 0.0f;
    m_vec74.y = 0.0f;
    m_avoidPoint.x = 0.0f;
    m_avoidPoint.y = 0.0f;
    m_obstaclePosition.x = 0.0f;
    m_obstaclePosition.y = 0.0f;
    m_cachedDir.x = 0.0f;
    m_cachedDir.y = 0.0f;
    m_obstacle = 0xFFFFFFFFu;
    m_nextDirTime = -1000.0;
    m_previousHeading.y = 0.0f;
    m_previousHeading.x = 1.0f;
    static_cast<BotMain*>(bot)->setMoveToStatus(0);
}

// ELF 0x1D5550 (D1; D2 0x1D54D0, D0 0x1D55D0, thunk 0x1D67D0): deletes the end condition.
BAPAMoveTo::~BAPAMoveTo()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1D5650
unsigned long long BAPAMoveTo::getControls()
{
    return m_controls;
}

// ELF 0x1D5710
BAPStatementType BAPAMoveTo::getId() const
{
    return BAPSTMoveTo;   // 7
}

// ELF 0x1D5720
bool BAPAMoveTo::hasDefault()
{
    return false;
}

// ELF 0x1D5700
bool BAPAMoveTo::getIsSprinting()
{
    return m_isSprinting;
}

// ELF 0x1D6880
bool BAPAMoveTo::getIsMovingAway()
{
    return false;
}

// ELF 0x1D6890
Vec2& BAPAMoveTo::getCurrentPoint()
{
    return m_currentPoint;
}

// ELF 0x1D68A0
int BAPAMoveTo::update(Vec2&, bool&, bool&, float)
{
    return 0;
}

// ELF 0x1D68B0: only the end condition.
bool BAPAMoveTo::isFinished(Bot* bot)
{
    bool finished = false;
    if (m_endCondition)
        finished = m_endCondition->evaluate(bot);
    return finished;
}

// ELF 0x1D68F0
bool BAPAMoveTo::getSecondPoint(Vec2&)
{
    return false;
}

// ELF 0x1D6900
bool BAPAMoveTo::getLargestAngleDiff(float&)
{
    return false;
}

// ELF 0x1D6910
float BAPAMoveTo::getTargetDistance()
{
    return 0.0f;
}

// ELF 0x1D6930
unsigned int BAPAMoveTo::getObject()
{
    return 0xFFFFFFFFu;
}

// ELF 0x1D5760: distance from the bot's current vehicle/soldier to the goal (x87 sum z, x, y).
float BAPAMoveTo::getSimpleTargetDistance(Bot* bot)
{
    return std::sqrt(BAPAMoveTo::getSimpleTargetDistanceSqr(bot));   // inline in the image (no virtual call)
}

// ELF 0x1D5810 (the information is not null-checked)
float BAPAMoveTo::getSimpleTargetDistanceSqr(Bot* bot)
{
    const Vec3& p = bapinline::info(bot->getCurrentEquipment())->getPosition();
    const double dz = static_cast<double>(p.z) - m_coordinate.z;
    const double dx = static_cast<double>(p.x) - m_coordinate.x;
    const double dy = static_cast<double>(p.y) - m_coordinate.y;
    return static_cast<float>(dz * dz + dx * dx + dy * dy);
}

// ELF 0x1D6940
bool BAPAMoveTo::isStuck()
{
    return false;
}

// ELF 0x1D5660: sprinting looks at a point (target -1) or at an object (the point is zeroed).
void BAPAMoveTo::setIsSprinting(Bot* bot, bool sprinting, const Vec3& point, unsigned int target)
{
    m_isSprinting = sprinting;
    if (!sprinting)
        return;
    if (target == 0xFFFFFFFFu) {
        bot->getSensingAgent()->setSprintFocus(point);
    } else {
        bot->getSensingAgent()->setSprintFocusTarget(target);
        bot->getSensingAgent()->setSprintFocus(bapinline::vec3(0.0f, 0.0f, 0.0f));
    }
}

// ELF 0x1D5730
Vec3& BAPAMoveTo::getCoordinate()
{
    return m_coordinate;
}

// ELF 0x1D5740
float BAPAMoveTo::getMaxSpeed()
{
    return m_maxSpeed;
}

// ELF 0x1D5750
float BAPAMoveTo::getMaxArriveSpeed()
{
    return m_maxArriveSpeed;
}

// ELF 0x1D58B0. The XZ direction to walk towards `target`, normalised into `dir`, steering round
// one obstacle (m_obstacle) that updateInfantryMoveToPosWithObstacles picked. The answer is
// cached until m_nextDirTime (always "now", so every call recomputes). Returns true when the
// obstacle code took over (dir untouched) or the bot keeps walking straight past the obstacle.
bool BAPAMoveTo::getTargetDir(Bot* bot, Vec2 target, Vec2& dir, Information* targetInfo, bool flag)
{
    const double now = bapinline::gameTime();
    if (now < m_nextDirTime) {
        dir = m_cachedDir;
        return m_cachedResult;
    }
    m_cachedResult = true;
    m_nextDirTime = now + 0.0;

    Vec2 pos;
    pos.x = 0.0f;
    pos.y = 0.0f;
    vec3ToVec2XZ(bapinline::info(bot->getCurrentEquipment())->getPosition(), pos);

    if (usesPathfinding()
        && (m_obstacle == 0xFFFFFFFFu || needsUpdate(m_avoidPoint, m_obstacle, m_obstacleRadius))) {
        Vec2 ahead;
        ahead.x = 0.0f;
        ahead.y = 0.0f;
        bot->getBotNavigation()->getFirstPointBeyondDist(pos, 10.0f, ahead);
        if (updateInfantryMoveToPosWithObstacles(bot, ahead, m_coordinate, getObject(), m_avoidPoint, m_obstacle,
                                                 m_avoidDistance, m_obstaclePosition, m_obstacleRadius, m_avoiding,
                                                 targetInfo, flag))
            return true;
    }

    dir.x = target.x - pos.x;
    dir.y = target.y - pos.y;
    const float distSqr = dir.y * dir.y + dir.x * dir.x;
    float dist = std::sqrt(distSqr);
    bool passing = false;

    Information* obstacle = bapinline::info(m_obstacle);
    if (!obstacle) {
        m_obstacle = 0xFFFFFFFFu;
    } else {
        const float toObsX = m_obstaclePosition.x - pos.x;
        const float toObsY = m_obstaclePosition.y - pos.y;
        const float fromObsX = pos.x - m_obstaclePosition.x;
        const float fromObsY = pos.y - m_obstaclePosition.y;
        unsigned int current = m_obstacle;
        // -0xE8(%ebp): also the left side of the "(r + 1)^2" test below (0x1D6535), not the target
        // distance (audit 2026-11-01).
        const float obsDistSqr = fromObsY * fromObsY + fromObsX * fromObsX;
        if ((m_obstacleRadius + 1.0f) * (m_obstacleRadius + 1.0f) < obsDistSqr) {
            current = 0xFFFFFFFFu;
            m_obstacle = 0xFFFFFFFFu;
        }
        bool isOwnTarget = false;
        if (current != 0xFFFFFFFFu) {
            unsigned int change = bot->getChangeTarget();
            Information* ci = bapinline::info(change);
            if (ci && !ci->isPrimary())
                change = ci->getPrimaryObject(bot->getSide());
            unsigned int repair = bot->getRepairTarget();
            Information* ri = bapinline::info(repair);
            if (ri && !ri->isPrimary())
                repair = ri->getPrimaryObject(bot->getSide());
            current = m_obstacle;
            isOwnTarget = current == repair || current == change;
        }
        passing = isOwnTarget;
        if (current != 0xFFFFFFFFu) {
            if (!isOwnTarget && !m_avoiding) {
                passing = toObsY * dir.y + toObsX * dir.x < 0.0f;
                if (!passing && targetInfo && dist * dist <= toObsX * toObsX + toObsY * toObsY)
                    passing = true;
            }
            Vec2 obsPos;
            obsPos.x = 0.0f;
            obsPos.y = 0.0f;
            vec3ToVec2XZ(obstacle->getPosition(), obsPos);
            const float driftX = m_obstaclePosition.x - obsPos.x;
            const float driftY = m_obstaclePosition.y - obsPos.y;
            if (driftY * driftY + driftX * driftX < 0.25f) {
                Vec2 goal;
                goal.x = 0.0f;
                goal.y = 0.0f;
                vec3ToVec2XZ(m_coordinate, goal);
                const float toGoalX = goal.x - pos.x;
                const float toGoalY = goal.y - pos.y;
                const float goalToObs = length2(obsPos.x - goal.x, obsPos.y - goal.y);
                const float r = m_obstacleRadius;
                const float reach = (r - goalToObs) + 0.8f;
                const bool goalClose = toGoalY * toGoalY + toGoalX * toGoalX < reach * reach;
                if ((m_avoiding && goalToObs <= r
                     && (goalClose || (m_avoidDistance < r && obsDistSqr <= (r + 1.0f) * (r + 1.0f))))
                    || ((!passing || isOwnTarget) && goalToObs <= r && goalClose)) {
                    m_cachedDir = dir;
                    return true;
                }
                target = m_avoidPoint;
                dir.x = target.x - pos.x;
                dir.y = target.y - pos.y;
                dist = length2(dir.x, dir.y);
            } else {
                m_obstacle = 0xFFFFFFFFu;
            }
        }
    }

    if (getIsSprinting() && 1.1920929e-07f < std::fabs(target.y - 0.0f) + std::fabs(target.x - 0.0f))
        setIsSprinting(bot, true, bapinline::vec3(target.x, 0.0f, target.y), 0xFFFFFFFFu);

    bool straight = true;
    if (m_obstacle != 0xFFFFFFFFu) {
        if (dist < 0.3f)
            m_obstacle = 0xFFFFFFFFu;
        if (m_obstacle != 0xFFFFFFFFu && !passing && 0.5f < dist) {
            straight = false;
            Vec2 toAvoid;
            toAvoid.x = m_avoidPoint.x - pos.x;
            toAvoid.y = m_avoidPoint.y - pos.y;
            bapinline::normalize(toAvoid);
            Vec2 toObs;
            toObs.x = m_obstaclePosition.x - pos.x;
            toObs.y = m_obstaclePosition.y - pos.y;
            bapinline::normalize(toObs);
            if (toAvoid.y * toObs.y + toAvoid.x * toObs.x <= 0.05f) {
                dir.x *= 1.0f / dist;
                dir.y = (1.0f / dist) * dir.y;
            } else {
                // Step round the obstacle: from its centre through the point one unit towards the
                // avoid point, out to m_avoidDistance.
                Vec2 out;
                out.x = (pos.x + toAvoid.x) - m_obstaclePosition.x;
                out.y = (pos.y + toAvoid.y) - m_obstaclePosition.y;
                bapinline::normalize(out);
                dir.x = (out.x * m_avoidDistance + m_obstaclePosition.x) - pos.x;
                dir.y = (m_avoidDistance * out.y + m_obstaclePosition.y) - pos.y;
                bapinline::normalize(dir);
            }
        }
    }
    if (straight) {
        if (0.001f < dist) {
            dir.x *= 1.0f / dist;
            dir.y = (1.0f / dist) * dir.y;
        } else {
            dir.x = 1.0f;
            dir.y = 0.0f;
        }
    }
    m_cachedDir = dir;
    m_cachedResult = false;
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
