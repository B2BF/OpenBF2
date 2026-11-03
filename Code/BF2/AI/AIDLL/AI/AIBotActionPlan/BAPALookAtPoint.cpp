// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D39C0 (__tcf_0 0x1D3A20, _GLOBAL__I 0x1D3A50): <iostream>'s Init object only.
#include <iostream>

#include "BAPALookAtPoint.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D36C0 (C1; C2 0x1D3630). Unlike the other looks it neither sets the condition's owner
// nor deletes the condition.
BAPALookAtPoint::BAPALookAtPoint(const Vec3& point, Bot* bot, float lookSpeed, BAPCondition* endCondition,
                                 unsigned long long controls, int tag)
    : BAPALook(lookSpeed, endCondition, controls, tag)
{
    m_point = point;
    m_bot = bot;
}

// ELF 0x1D3940 (D1; D2 0x1D38F0 region, D0 0x1D3990): base only.
BAPALookAtPoint::~BAPALookAtPoint()
{
}

// ELF 0x1D3750: from the bot's current vehicle/soldier position to the point, normalised; no
// information: ok = false and a zero vector.
Vec3 BAPALookAtPoint::getLookDirection(bool& ok)
{
    Information* self = bapinline::info(m_bot->getCurrentEquipment());
    if (!self) {
        ok = false;
        return bapinline::vec3(0.0f, 0.0f, 0.0f);
    }
    ok = true;
    Vec3 dir = bapinline::sub(m_point, self->getPosition());
    bapinline::normalize(dir);
    return dir;
}

} // namespace ai
} // namespace hfe
} // namespace dice
