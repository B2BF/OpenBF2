// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EB990 (__tcf_0 0x1EB9F0, _GLOBAL__I 0x1EBA20): <iostream>'s Init object only.
#include <iostream>

#include "BAPCConLookDeviation.h"

#include "BAPALook.h"
#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../../../../Math/Mat4.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EB5D0 (C1)
BAPCConLookDeviation::BAPCConLookDeviation(BAPALook* look, float angle, bool flat)
{
    m_look = look;
    m_flat = flat;
    float a = 0.0f;
    if (0.0f <= angle) {
        a = angle;
        if (3.1415927f < angle)
            a = 3.1415927f;
    }
    m_cosLimit = std::cos(a);
}

// ELF 0x1EB670 (D0 0x1EB690)
BAPCConLookDeviation::~BAPCConLookDeviation()
{
}

// ELF 0x1EB6D0: dot of the normalised look direction with camera row 2 (forward, +0x20) of
// IAIPlayer::getCameraTransformation (+0x8C) against the cosine limit; too short a direction or
// no player/information/look: false.
bool BAPCConLookDeviation::evaluate(Bot* bot)
{
    if (!bapinline::info(bot->getCurrentEquipment()))
        return false;
    bool valid;
    Vec3 d = m_look->getLookDirection(valid);
    if (!valid)
        return false;
    if (!m_flat) {
        const float len = std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);
        if (0.001f <= len) {
            const float inv = 1.0f / len;
            d.x *= inv;
            d.y *= inv;
            d.z = inv * d.z;
            if (!bot->getPlayer())
                return false;
            const Mat4& cam = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
            return m_cosLimit <= d.z * cam.m[10] + d.y * cam.m[9] + d.x * cam.m[8];
        }
        return false;
    }
    d.y = 0.0f;
    const float len = std::sqrt(d.z * d.z + d.x * d.x + 0.0f);
    if (0.0001f <= len) {
        const float inv = 1.0f / len;
        d.x *= inv;
        d.y *= inv;
        d.z = inv * d.z;
        if (!bot->getPlayer())
            return false;
        const Mat4& cam = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
        const float fx = cam.m[8];
        const float fz = cam.m[10];
        const float flen = std::sqrt(fx * fx + 0.0f + fz * fz);
        if (0.0001f <= flen) {
            const float finv = 1.0f / flen;
            return m_cosLimit <= fx * finv * d.x + 0.0f * finv * d.y + finv * fz * d.z;
        }
    }
    return false;
}

// ELF 0x1EB980
BAPConditionType BAPCConLookDeviation::getConditionType() const
{
    return BAPCTLookDeviation;   // 0
}

} // namespace ai
} // namespace hfe
} // namespace dice
