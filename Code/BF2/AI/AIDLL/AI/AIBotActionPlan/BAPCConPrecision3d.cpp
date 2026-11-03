// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// [PLATFORM] precision: the x87 build compares the squared miss distance in extended precision.

// static init: ELF 0x1ECA30 (__tcf_0 0x1ECA90, _GLOBAL__I 0x1ECAC0): <iostream>'s Init object only.
#include <iostream>

#include "BAPCConPrecision3d.h"

#include <cfloat>
#include <cmath>

#include "../AIBot/BotMain.h"
#include "BAPAAimAt3d.h"
#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EC650 (C1; C2 0x1EC5D0)
BAPCConPrecision3d::BAPCConPrecision3d(unsigned int object, BAPAAimAt3d* aim, float precision, bool waitForClosest)
{
    m_precision = precision;
    m_object = object;
    m_precisionSq = precision * precision;
    m_aim = aim;
    m_waitForClosest = waitForClosest;
    m_bestDistanceSq = FLT_MAX;
    m_maxAngle = std::atan(precision / 80.0f);
}

// ELF 0x1EC6F0 (D1 0x1EC6D0, D0 0x1EC710): base only.
BAPCConPrecision3d::~BAPCConPrecision3d()
{
}

// ELF 0x1EC750. The bot's own information is looked up and not used, as compiled.
bool BAPCConPrecision3d::evaluate(Bot* bot)
{
    if (!m_aim->isAimVecValid())
        return false;
    if (!bapinline::info(m_object))
        return false;
    bot->getCurrentEquipment();
    double impactTime;
    m_aim->getImpactTime(impactTime);
    Vec3 aimVec = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_aim->getLastAimVec(aimVec);
    const Mat4& transform = m_aim->getAimerTransform();
    float cosine = transform.m[8] * aimVec.x + transform.m[9] * aimVec.y + transform.m[10] * aimVec.z;
    if (cosine < -1.0f)
        cosine = -1.0f;
    else if (cosine > 1.0f)
        cosine = 1.0f;
    if (std::acos(cosine) > m_maxAngle)
        return false;

    Vec3 predicted = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_aim->getPredictedPosition(predicted);
    Vec3 impact = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (!m_aim->getImpactPosition(impact, impactTime - bapinline::gameTime()))
        return false;
    const float distanceSq = bapinline::lengthSqr(bapinline::sub(predicted, impact));

    if (!m_waitForClosest)
    {
        const bool hit = distanceSq <= m_precisionSq;
        if (hit)
            static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
        return hit;
    }
    const float best = m_bestDistanceSq;
    if (best > 0.01f && best >= distanceSq)
    {
        m_bestDistanceSq = distanceSq;   // still closing in
        if (distanceSq <= m_precisionSq)
            static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
        return false;
    }
    const bool hit = best <= m_precisionSq;
    if (hit)
        static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
    m_bestDistanceSq = FLT_MAX;
    return hit;
}

// ELF 0x1ECA20
BAPConditionType BAPCConPrecision3d::getConditionType() const
{
    return BAPCTPrecision3d;
}

} // namespace ai
} // namespace hfe
} // namespace dice
