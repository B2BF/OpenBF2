// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F9D20 (__tcf_0 0x1F9D80, _GLOBAL__I 0x1F9DB0): <iostream>'s Init object only.
#include <iostream>

#include "BAPWrapperAiming.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIBot/SensingAgent.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The sensed hit point (SensedObject +0x04) through the target's transformation (Information slot
// +0x3C), spelled out in place three times in execute.
Vec3 sensedWorldPoint(Information* target, const SensedObject* sensed)
{
    const Mat4& t = target->getTransformation();
    const Vec3& l = sensed->m_seenPosition;
    return bapinline::vec3(t.m[8] * l.z + t.m[0] * l.x + t.m[4] * l.y + t.m[12],
                           t.m[1] * l.x + t.m[5] * l.y + t.m[9] * l.z + t.m[13],
                           l.x * t.m[2] + l.y * t.m[6] + l.z * t.m[10] + t.m[14]);
}

} // namespace

// ELF 0x1F8C30 (C1; C2 0x1F8B70). Note the argument order: aim direction, valid flag, aim point.
BAPWrapperAiming::BAPWrapperAiming(BAPVariable<Vec3>* aimDirection, BAPVariable<bool>* aimValid,
                                   BAPVariable<Vec3>* aimPoint, unsigned int shooter, unsigned int target,
                                   bool continuous)
{
    m_aimDirection = aimDirection;
    m_aimPoint = aimPoint;
    m_aimValid = aimValid;
    m_shooter = shooter;
    m_target = target;
    m_continuous = continuous;
    m_groundQuery.x = m_groundQuery.y = m_groundQuery.z = 0.0f;
    m_groundLevel = 0.0f;
}

// ELF 0x1F9DD0 (D1; D0 0x1F9E30): the Aimer and the base only.
BAPWrapperAiming::~BAPWrapperAiming()
{
}

// ELF 0x1F8CF0. The ground under `pos`: a line from pos down 2 m against the world, ignoring the
// target's object; the answer is cached for the last position (FLT_EPSILON per component). No
// target information: false; no hit: false (cache untouched).
bool BAPWrapperAiming::getGroundLevelAtPos(Bot*, const Vec3& pos, float& level)
{
    Information* target = bapinline::info(m_target);
    if (!target)
        return false;
    if (std::fabs(m_groundQuery.x - pos.x) > 1.1920929e-07f || std::fabs(m_groundQuery.y - pos.y) > 1.1920929e-07f
        || std::fabs(m_groundQuery.z - pos.z) > 1.1920929e-07f) {
        // pos - (0, 2, 0) (the image subtracts 0.0 from x and z).
        const Vec3 below = bapinline::vec3(pos.x - 0.0f, pos.y - 2.0f, pos.z - 0.0f);
        Vec3 hit = bapinline::vec3(0.0f, 0.0f, 0.0f);
        if (!bapinline::collideLineIgnoring(pos, below, target->getAIObject(), hit))
            return false;
        level = hit.y;
        m_groundQuery = pos;
        m_groundLevel = level;
        return true;
    }
    level = m_groundLevel;
    return true;
}

// ELF 0x1F8F40. Aims the weapon of m_shooter at m_target: overrides the sensing agent, sets up
// the Aimer from the active weapon (drag, exit velocity, scaled gravity, thrown), picks the aim
// point from the sensed record and solves for the firing direction. Explosive unguided weapons
// against a target that is out of sight use its last seen position (on the ground) or, for a
// "static" shot (BotMain::setBotWantsToSimulateArtillery(true), Aimer::getStaticFiringDirection),
// its last known position led by its velocity. Out: m_aimDirection, m_aimPoint, m_aimValid; ok;
// done unless m_continuous. Missing information, control info, armament or weapon: ok = false,
// done.
bool BAPWrapperAiming::execute(Bot* bot, bool& ok, bool& done)
{
    bot->getSensingAgent()->setOverride(true);
    Information* self = bapinline::info(m_shooter);
    if (!self || !self->getControlInfo() || !self->getArmament()) {
        ok = false;
        done = true;
        return true;
    }
    IPIArmament* armament = self->getArmament();
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon) {
        ok = false;
        done = true;
        return true;
    }
    Information* target = bapinline::info(m_target);
    if (!target) {
        ok = false;
        done = true;
        return true;
    }
    // Neither primary information is null-checked in the image.
    IPIMobile* targetMobile = bapinline::info(target->getPrimaryObject(bot->getSide()))->getMobile();
    IPIMobile* ownMobile = bapinline::info(self->getPrimaryObject(bot->getSide()))->getMobile();

    const float drag = weapon->getDrag();
    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer.m_speed = weapon->getExitVelocity();
    const float gravityModifier = weapon->getGravityModifier();
    m_aimer.m_gravity = static_cast<float>(
        static_cast<double>(static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity()) * gravityModifier);
    m_aimer.m_grenade = weapon->getIsThrown();
    const double now = bapinline::gameTime();

    // Soldier against soldier: short-range weapons aim 0.2 lower, others 0.3 lower at a standing
    // target.
    float yOffset = 0.0f;
    if ((self->getObjectTypeFlags() & 0x200000u) && (target->getObjectTypeFlags() & 0x200000u)) {
        if (weapon->getMaxRange() < 10.0f)
            yOffset = -0.2f;
        else if (target->getSoldierPose() == SPStanding)
            yOffset = -0.3f;
    }

    SensedObject* sensed = bot->getSensedObjectFromHandle(m_target);
    if (!sensed)
        return true;   // ok/done untouched (0x1F91EA)

    Vec3 point = sensedWorldPoint(target, sensed);
    const double now2 = bapinline::gameTime();
    const Vec3& from = self->getPosition();
    const float dy = point.y - from.y;
    Vec3 toTarget = bapinline::vec3(point.x - from.x, dy, point.z - from.z);
    const float len = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
    if (len != 0.0f) {
        const float inv = 1.0f / len;
        toTarget.x *= inv;
        toTarget.y *= inv;
        toTarget.z = inv * toTarget.z;
    }

    const bool explosive = weapon->getExplosionRadius() != 0.0f && weapon->getWeaponGuidanceType() == 0;
    const bool explosiveShot = explosive && !weapon->getIsThrown();
    const bool vehicleAtSoldier = !(self->getObjectTypeFlags() & 0x200000u)
                                  && (target->getObjectTypeFlags() & 0x200000u) && explosiveShot;
    const bool highAbove = bot->getSpawnKit() != 1 && !weapon->getIsThrown() && 30.0f < dy;
    const bool indirect = (sensed->m_flags & 2u) && ((sensed->m_flags & 8u) || vehicleAtSoldier || highAbove)
                          && explosive;
    const double lastFailed = static_cast<BotMain*>(bot)->getTimeOfLastFailedStaticSkyAttack();
    const bool staticShot = indirect && lastFailed <= sensed->m_lastPositionTime && !vehicleAtSoldier
                            && (!explosiveShot || 10.0f < dy || 0.6f < toTarget.y);
    m_aimer.m_flag69 = indirect && !staticShot;

    if (staticShot) {
        if (targetMobile) {
            double sinceSeen = 999999.0;
            if (sensed->m_flags & 2u)
                sinceSeen = now2 - sensed->m_lastSeenTime;
            point = 5.0 <= sinceSeen ? sensed->m_lastSeenPosition : sensed->m_lastPosition;
            double lead = now - sensed->m_lastPositionTime;
            if (1.0 < lead) {
                const double scaled = (lead - 1.0) * 0.05 + 1.0;   // static `factor` = 0.05 (0x4BA328)
                const float limit =
                    8.0f <= static_cast<float>(now2 - bot->getTimeOfGrenadeThrown(0)) ? 2.0f : 0.5f;
                lead = limit;
                if (scaled < static_cast<double>(limit))
                    lead = scaled;
            }
            const Vec3& v = targetMobile->getSpeed();
            const float s = static_cast<float>(lead);
            point.x = v.x * s + point.x;
            point.y = v.y * s + point.y;
            point.z = s * v.z + point.z;
        } else {
            point = sensedWorldPoint(target, sensed);
        }
    } else if (indirect) {
        point = sensed->m_lastSeenPosition;
        float level;
        if (getGroundLevelAtPos(bot, point, level))
            point.y = level;
    } else if (!(sensed->m_flags & 2u) || (sensed->m_flags & 8u)) {
        point.y = yOffset + point.y;
    } else {
        point = sensedWorldPoint(target, sensed);
        point.y = point.y + yOffset;
    }

    if (explosive && !weapon->getIsThrown() && !indirect && (target->getObjectTypeFlags() & 0x200000u))
        point.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getHeight(point.x, point.z);
    if (m_aimPoint)
        m_aimPoint->m_value = point;

    const Mat4& origin = weapon->getProjectileOrigin();
    const Vec3 rel = bapinline::vec3(point.x - origin.m[12], point.y - origin.m[13], point.z - origin.m[14]);
    Vec3 targetVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
    Vec3 ownVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (ownMobile)
        ownVelocity = ownMobile->getSpeed();
    if (targetMobile && !indirect)
        targetVelocity = targetMobile->getSpeed();
    m_aimer.setTargetData(rel, targetVelocity, ownVelocity, &origin, now);

    Vec3 direction = bapinline::vec3(0.0f, 0.0f, 0.0f);
    Vec3 aimTarget = bapinline::vec3(0.0f, 0.0f, 0.0f);
    bool valid;
    if (staticShot) {
        static_cast<BotMain*>(bot)->setBotWantsToSimulateArtillery(true);
        valid = m_aimer.getStaticFiringDirection(direction, aimTarget);
    } else {
        static_cast<BotMain*>(bot)->setBotWantsToSimulateArtillery(false);
        valid = m_aimer.getFiringDirection(now, 0.5f, weapon->getIndirect(), direction, aimTarget, 0);
    }
    done = !m_continuous;
    ok = true;
    m_aimDirection->m_value = direction;
    m_aimValid->m_value = valid;
    return true;
}

// ELF 0x1F9D00
BAPStatementType BAPWrapperAiming::getId() const
{
    return BAPSTAimingWrapper;   // 16
}

// ELF 0x1F9D10
bool BAPWrapperAiming::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
