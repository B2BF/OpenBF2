// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// [PLATFORM] precision: the x87 build compares the squared miss distance in extended precision.

// static init: ELF 0x1EB440 (__tcf_0 0x1EB4A0, _GLOBAL__I 0x1EB4D0): <iostream>'s Init object only.
#include <iostream>

#include "BAPCConBombPrecision3d.h"

#include <algorithm>
#include <cfloat>

#include "../AIBot/BotMain.h"
#include "BAPAAimAt3d.h"
#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EB110 (C1; C2 0x1EB0B0)
BAPCConBombPrecision3d::BAPCConBombPrecision3d(BAPAAimAt3d* aim, float precision, bool waitForClosest)
{
    m_precisionSq = std::max(0.01f, precision * precision);
    m_aim = aim;
    m_bestDistanceSq = FLT_MAX;
    m_float14 = FLT_MAX;
    m_waitForClosest = waitForClosest;
}

// ELF 0x1EB4F0 (D0 0x1EB510): base only.
BAPCConBombPrecision3d::~BAPCConBombPrecision3d()
{
}

// ELF 0x1EB170. The aimer transform is fetched and not used; a failed closest approach keeps its
// best distance (unlike BAPCConPrecision3d), as compiled.
bool BAPCConBombPrecision3d::evaluate(Bot* bot)
{
    if (!m_aim->isAimVecValid())
        return false;
    if (!bapinline::info(bot->getCurrentEquipment()))
        return false;
    double impactTime;
    m_aim->getImpactTime(impactTime);
    Vec3 aimVec = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_aim->getLastAimVec(aimVec);
    m_aim->getAimerTransform();
    Vec3 predicted = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_aim->getPredictedPosition(predicted);
    Vec3 impact = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (!m_aim->getImpactPosition(impact, impactTime - bapinline::gameTime()))
        return false;
    const float distanceSq = bapinline::lengthSqr(bapinline::sub(predicted, impact));

    if (m_waitForClosest)
    {
        const float precisionSq = m_precisionSq;
        if (!(precisionSq * 0.25f > distanceSq))
        {
            const float best = m_bestDistanceSq;
            if (best > 0.01 && best >= distanceSq)
            {
                m_bestDistanceSq = distanceSq;   // still closing in
                if (precisionSq >= distanceSq)
                    static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
                return false;
            }
            if (!(precisionSq >= best))
                return false;
            static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
            m_bestDistanceSq = FLT_MAX;
            return true;
        }
    }
    else if (!(m_precisionSq >= distanceSq))
    {
        return false;
    }
    static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(bapinline::gameTime());
    return true;
}

// ELF 0x1EB430
BAPConditionType BAPCConBombPrecision3d::getConditionType() const
{
    return BAPCTBombPrecision3d;
}

} // namespace ai
} // namespace hfe
} // namespace dice
