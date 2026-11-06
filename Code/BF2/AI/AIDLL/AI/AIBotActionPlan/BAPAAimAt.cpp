// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1CDA60 (__tcf_0 0x1CDAC0, _GLOBAL__I 0x1CDAF0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAAimAt.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/BotSubObjects.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1CD6E0 (C1; C2 0x1CD600). Vec3 members zeroed, the Aimer default-constructed, the ring
// cleared; +0x80..+0xA0 and +0x140/+0x144 are left alone.
BAPAAimAt::BAPAAimAt()
{
    m_offset.x = m_offset.y = m_offset.z = 0.0f;
    for (int i = 0; i < 4; ++i)
        m_vec20[i].x = m_vec20[i].y = m_vec20[i].z = 0.0f;
    for (int i = 0; i < 4; ++i)
        m_vec50[i].x = m_vec50[i].y = m_vec50[i].z = 0.0f;
    m_pitchIndex = 0;
    m_int148 = 0;
    m_overrideValid = 0;
    m_overrideAim = 0;
    m_lastYawError = 0.0f;
    for (int i = 0; i < 10; ++i)
        m_pitchErrors[i] = 0.0f;
}

// ELF 0x1CD8E0 (C1; C2 0x1CD7E0)
BAPAAimAt::BAPAAimAt(BAPVariable<bool>* overrideValid, BAPVariable<Vec3>* overrideAim)
{
    m_offset.x = m_offset.y = m_offset.z = 0.0f;
    for (int i = 0; i < 4; ++i)
        m_vec20[i].x = m_vec20[i].y = m_vec20[i].z = 0.0f;
    for (int i = 0; i < 4; ++i)
        m_vec50[i].x = m_vec50[i].y = m_vec50[i].z = 0.0f;
    m_pitchIndex = 0;
    m_lastYawError = 0.0f;
    m_int148 = 0;
    m_overrideValid = overrideValid;
    m_overrideAim = overrideAim;
    for (int i = 0; i < 10; ++i)
        m_pitchErrors[i] = 0.0f;
}

// ELF 0x1CDB40 (D1; D0 0x1CDBA0): the Aimer and the base only; the end condition belongs to the
// subclasses.
BAPAAimAt::~BAPAAimAt()
{
}

// ELF 0x1CD5C0: never executed directly (the plan interpreter's entry does the aiming).
bool BAPAAimAt::execute(Bot*, bool& ok, bool& done)
{
    ok = false;
    done = true;
    return false;
}

// ELF 0x1CDB20
BAPStatementType BAPAAimAt::getId() const
{
    return BAPSTAimAt;   // 6
}

// ELF 0x1CDB30
bool BAPAAimAt::hasDefault()
{
    return false;
}

// ELF 0x1CD9E0: mean of the ten ring entries (x87 sum, then * 0.1).
float BAPAAimAt::getAveragePitchAngle()
{
    double sum = 0.0;
    for (int i = 0; i < 10; ++i)
        sum += m_pitchErrors[i];
    return static_cast<float>(sum * static_cast<double>(0.1f));     // fmuls: the float constant
}

// ELF 0x1CDA20: `mov 0xb4(bot)` = BotMain::m_fireCorrection; zeroes its +0x2C Vec3 and returns it
// (`lea 0x2c(%edx), %eax`). The aim actions add it to the relative target position and subtract
// it from the prediction.
Vec3& BAPAAimAt::correctAim(Bot* bot)
{
    Vec3& correction = static_cast<BotMain*>(bot)->getFireCorrection()->m_vec44;
    correction.x = 0.0f;
    correction.y = 0.0f;
    correction.z = 0.0f;
    return correction;
}

} // namespace ai
} // namespace hfe
} // namespace dice
