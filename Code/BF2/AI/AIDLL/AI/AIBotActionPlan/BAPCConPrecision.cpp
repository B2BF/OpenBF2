// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EC520 (__tcf_0 0x1EC580, _GLOBAL__I 0x1EC5B0): <iostream>'s Init object only.
#include <iostream>

#include "BAPCConPrecision.h"

#include "BAPAAimAt.h"
#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

#include <cfloat>
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The function-static `negativeYLimitFactor` of BAPCConPrecision::evaluate (ELF .data 0x4B95A4).
const float negativeYLimitFactor = 0.1f;

} // namespace

// ELF 0x1EBAF0 (C1; C2 0x1EBA40): against an aim action.
BAPCConPrecision::BAPCConPrecision(unsigned int target, BAPAAimAt* aim, float precision, bool waitForBest,
                                   bool limitPositiveY, bool noAngleCheck)
{
    m_target = target;
    m_precisionSqr = 0.01f <= precision * precision ? precision * precision : 0.01f;
    m_precision = 0.1f <= precision ? precision : 0.1f;
    m_aim = aim;
    m_bestDistanceSqr = FLT_MAX;
    m_aimValid = 0;
    m_aimDirection = 0;
    m_waitForBest = waitForBest;
    m_noAngleCheck = noAngleCheck;
    m_limitPositiveY = limitPositiveY;
    if (!noAngleCheck)
        m_angleLimit = std::atan(m_precision / 50.0f);
}

// ELF 0x1EBC50 (C1; C2 0x1EBBA0): against BAPWrapperAiming's output variables.
BAPCConPrecision::BAPCConPrecision(unsigned int target, BAPVariable<bool>* aimValid, BAPVariable<Vec3>* aimDirection,
                                   BAPVariable<Vec3>* aimPoint, float precision, bool waitForBest,
                                   bool limitPositiveY, bool noAngleCheck)
{
    m_target = target;
    m_precisionSqr = 0.01f <= precision * precision ? precision * precision : 0.01f;
    m_aim = 0;
    m_bestDistanceSqr = FLT_MAX;
    m_precision = 0.1f <= precision ? precision : 0.1f;
    m_waitForBest = waitForBest;
    m_aimValid = aimValid;
    m_aimDirection = aimDirection;
    m_aimPoint = aimPoint;
    m_noAngleCheck = noAngleCheck;
    m_limitPositiveY = limitPositiveY;
    if (!noAngleCheck)
        m_angleLimit = std::atan(m_precision / 50.0f);
}

// ELF 0x1EBD20 (D1; D0 0x1EBD40): base only.
BAPCConPrecision::~BAPCConPrecision()
{
}

// ELF 0x1EBD80. True when the shot would hit within the precision. Without the target
// information: false.
// Aim action: needs a valid aim; an "aimer only" weapon pointing exactly along the last aim vector
// (dot >= 1 with the aimer transform's row 2) fires at once. Otherwise the miss is the distance
// between the predicted target position and the Aimer's impact position at the impact time; with
// m_waitForBest the condition waits while the miss still shrinks and fires on the first shot that
// is no better than the best so far (when that best was within the precision). Every positive
// answer stamps BotMain::setHadPrecisionTimeStamp(now); the "still improving" path stamps when
// within the precision but answers false (0x1EC41A..0x1EC43D).
// Variables: needs the valid flag; the miss is the angle between the weapon's projectile-origin
// row 2 and the aim direction (unless m_noAngleCheck), then the offset of that row from the aim
// direction scaled by the distance to the aim point: horizontally within the precision and
// vertically (signed square) within +-20% of it (the lower bound times negativeYLimitFactor, the
// upper one too when m_limitPositiveY). Vehicles close to a non-soldier target (|dy| < 5, < 15 m)
// and vehicles using static attacks skip the angle and vertical checks.
bool BAPCConPrecision::evaluate(Bot* bot)
{
    Information* target = bapinline::info(m_target);
    if (!target)
        return false;
    Information* self = bapinline::info(bot->getCurrentEquipment());

    if (!m_aimValid) {
        if (!m_aim->isAimingValid())
            return false;
        double impactTime;
        m_aim->getImpactTime(impactTime);
        Vec3 lastAim = bapinline::vec3(0.0f, 0.0f, 0.0f);
        m_aim->getLastAimVec(lastAim);
        IPIArmament* armament = self->getArmament();
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
        if (weapon && weapon->useAimerOnly()) {
            const Mat4* t = m_aim->getAimerTransform();
            if (1.0f <= t->m[10] * lastAim.z + t->m[8] * lastAim.x + t->m[9] * lastAim.y) {
                static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
                return true;
            }
        }
        Vec3 predicted = bapinline::vec3(0.0f, 0.0f, 0.0f);
        m_aim->getPredictedPosition(predicted);
        Vec3 impact = bapinline::vec3(0.0f, 0.0f, 0.0f);
        if (!m_aim->getImpactPosition(impact, impactTime - bapinline::gameTime()))
            return false;
        const Vec3 miss = bapinline::sub(predicted, impact);
        const float d = miss.z * miss.z + miss.x * miss.x + miss.y * miss.y;
        if (!m_waitForBest) {
            if (m_precisionSqr <= d)
                return false;
            static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
            return true;
        }
        const float best = m_bestDistanceSqr;
        if (best <= 0.01f || best <= d) {
            m_bestDistanceSqr = FLT_MAX;
            if (m_precisionSqr < best)
                return false;
            static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
            return true;
        }
        m_bestDistanceSqr = d;
        if (d <= m_precisionSqr)
            static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
        return false;
    }

    if (!m_aimValid->m_value)
        return false;
    Information* own = bapinline::info(bot->getCurrentEquipment());   // re-read; not null-checked
    IPIArmament* armament = own->getArmament();
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon)
        return false;
    const Vec3& point = m_aimPoint->m_value;
    const Vec3& from = own->getPosition();
    const Vec3 toPoint = bapinline::sub(point, from);
    float distance = std::sqrt(toPoint.x * toPoint.x + toPoint.y * toPoint.y + toPoint.z * toPoint.z);
    bool closeVehicle = false;
    if (5.0f <= std::fabs(point.y - from.y)) {
    } else if (!target->isOfType(ITSoldier)) {
        if (!own->isOfType(ITSoldier) && distance < 15.0f)
            closeVehicle = true;
    }
    bool staticAttack = false;
    if (!own->isOfType(ITSoldier))
        staticAttack = static_cast<BotMain*>(bot)->getUseStaticAttack();

    const Vec3 aim = m_aimDirection->m_value;
    const Mat4& origin = weapon->getProjectileOrigin();
    const Vec3 forward = bapinline::vec3(origin.m[8], origin.m[9], origin.m[10]);
    if (!m_noAngleCheck && !closeVehicle && !staticAttack) {
        float c = forward.x * aim.x + forward.y * aim.y + forward.z * aim.z;
        if (c < -1.0f)
            c = -1.0f;
        else if (1.0f < c)
            c = 1.0f;
        if (m_angleLimit <= std::acos(c))
            return false;
    }
    const float ox = (forward.x - aim.x) * distance;
    const float oz = (forward.z - aim.z) * distance;
    const float oy = (forward.y - aim.y) * distance;
    const float sign = oy <= 0.0f ? (0.0f <= oy ? 0.0f : -1.0f) : 1.0f;
    const float verticalSqr = oy * oy * sign;
    float upper = m_precisionSqr * 0.2f;
    const float lower = -upper;
    if (m_limitPositiveY)
        upper = upper * negativeYLimitFactor;
    if (m_precisionSqr < ox * ox + oz * oz)
        return false;
    if (!closeVehicle && !staticAttack) {
        if (upper < verticalSqr)
            return false;
        if (verticalSqr < lower * negativeYLimitFactor)
            return false;
    }
    return true;
}

// ELF 0x1EC510
BAPConditionType BAPCConPrecision::getConditionType() const
{
    return BAPCTPrecision;   // 1
}

} // namespace ai
} // namespace hfe
} // namespace dice
