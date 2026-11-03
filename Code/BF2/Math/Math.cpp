// Math initialization and proportional control.

// static init: ELF 0x49BCF0 (__tcf_0 0x49BD50, _GLOBAL__I 0x49BD80): iostream Init only.
#include <iostream>

#include "Math.h"

#include <cmath>

namespace dice {
namespace hfe {


// Initialization succeeds without allocating resources.
// ELF 0x49BAA0; bf2 0x83B7EF0.
bool initMath()
{
    return true;
}


// No resources require shutdown.
// ELF 0x49BAB0; bf2 0x83B7F00.
void closeMath()
{
}

// ELF 0x49BAC0 / 0x49BB00; bf2 0x83B7F10 / 0x83B7F40.
PController::PController() { init(0, 0.0f); }
// ELF 0x49BB40 / 0x49BB80; bf2 0x83B7F70 / 0x83B7F80.
PController::PController(const float* target, float rate) { init(target, rate); }

// ELF 0x49BBC0; bf2 0x83B7F90 / PE 0x68D2E0. Constructors forward this initialization.
void PController::init(const float* target, float rate)
{
    m_target = target;
    m_rate = rate;
    m_value = target ? *target : 0.0f;
}

// Does nothing without a target. Thresholds are strict; the negative clamp selects error for NaN.
// Negative steps and rates are accepted.
// ELF 0x49BC00; bf2 0x83B7FC0. [PLATFORM] The two x87 builds spill different intermediate
// values around fabs; native float arithmetic follows their shared source-level operations.
void PController::update(int steps)
{
    if (!m_target)
        return;
    const float error = *m_target - m_value;
    if (std::fabs(error) < 0.0000099999997f)
        return;

    float change;
    if (error > 0.0f)
    {
#if defined(_WIN32)
        change = static_cast<float>(static_cast<double>(steps) * m_rate * error);
#else
        change = static_cast<float>(steps) * error * m_rate;
#endif
        if (change > error) change = error;
    }
    else if (error < 0.0f)
    {
#if defined(_WIN32)
        change = static_cast<float>(static_cast<double>(steps) * m_rate * error);
#else
        change = static_cast<float>(steps) * error * m_rate;
#endif
        if (!(error < change)) change = error;
    }
    else
        return;

    m_value += change;
    if (std::fabs(m_value) < 0.000099999997f)
        m_value = 0.0f;
}

// bf2 ELF 0x83B9710; this helper has no symbol in libaidll.
#if !defined(BF2_IMAGE_AIDLL)
float smooth_in_out(float current, float target, float* velocity, float smoothTime, float dt)
{
    const float omega = 2.0f / smoothTime;
    const float x = omega * dt;
    const float exp = 1.0f / (1.0f + x + 0.47999999f * x * x + 0.235f * x * x * x);
    float change = current - target;
    if (std::fabs(change) < 0.000099999997f)
        change = 0.0f;
    const float temp = (*velocity + omega * change) * dt;
    *velocity = (*velocity - omega * temp) * exp;
    if (std::fabs(*velocity) < 0.000099999997f)
        *velocity = 0.0f;
    return target + (change + temp) * exp;
}
#endif

} // namespace hfe
} // namespace dice
