// ControlFunctions: the shared regulator and geometry helpers of the plan-interpreter controls
// (attested TU `AI/AIDLL/AI/AIPlanInterpreter/ControlFunctions.cpp`, the path of changeReg's Debug).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); every body carries its ELF address. x87 code
// was read in Intel syntax (llvm-objdump), which has no fsub(r)p/fdiv(r)p operand swap, and checked
// against the Ghidra decompilation. Parameter names are [INFERRED] from their use.
//
// The *Debug variants are the same computation as their non-debug twins in this build (the debug
// output was compiled out); they stay separate functions because the image has both symbols.

// static init: ELF 0x2B2080 (__tcf_0 0x2B20E0, _GLOBAL__I 0x2B2110): <iostream>'s Init object only.
#include <iostream>

#include "ControlFunctions.h"

#include <cmath>
#include <string>

#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Core/Debug.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// [CHOSEN, not recovered] helpers for this TU's readability. The image indexes Vec2 as float[2]
// (`[v + 4*axis]`), and every regulator reads the frame time through IBFTimer slot +0x0C and
// narrows it to float (`fstp dword` / `fld dword` right after the call).
inline float& axisOf(Vec2& v, int axis) { return (&v.x)[axis]; }
inline float axisOf(const Vec2& v, int axis) { return (&v.x)[axis]; }

inline float frameTime()
{
    return static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
}

// The three-way sign the regulators compute inline: 1 above zero, -1 below, 0 at zero. NaN fails
// the `<= 0` test and gives 1, as in the image.
inline float signOf(float v)
{
    if (v <= 0.0f)
        return (0.0f <= v) ? 0.0f : -1.0f;
    return 1.0f;
}

// The inline clamp at the end of every regulator (`fld -1; fucomi; ... fcmov`): NaN gives -1.
inline float clampUnit(float v)
{
    if (!(-1.0f <= v))
        return -1.0f;
    if (1.0f < v)
        return 1.0f;
    return v;
}

// One axis of getBoxIntersections (the image repeats the same code for x and then for y with the
// axes swapped). Box A is [aMin, aMax], box B is [bMin, bMax]. Returns false when the intervals are
// disjoint; lo/hi receive the overlap on `axis`. When one interval contains the other the image
// takes the overlap as a segment at a fixed coordinate of the other axis (`spans`, `other`).
bool boxAxisIntersection(const Vec2& aMin, const Vec2& aMax, const Vec2& bMin, const Vec2& bMax,
                         int axis, float& lo, float& hi, bool& spans, float& other)
{
    const int o = 1 - axis;
    const float aMaxV = axisOf(aMax, axis);
    const float bMaxV = axisOf(bMax, axis);
    spans = false;
    other = 0.0f;
    if (bMaxV <= aMaxV)
    {
        const float aMinV = axisOf(aMin, axis);
        if (aMaxV != bMaxV)
        {
            if (bMaxV < aMinV)
                return false;
            if (aMinV != bMaxV)
            {
                const float bMinV = axisOf(bMin, axis);
                if (aMinV < bMinV)
                {
                    // B lies strictly inside A on this axis.
                    spans = true;
                    other = axisOf(aMax, o);
                    if (axisOf(bMax, o) <= other)
                        other = axisOf(aMin, o);
                    lo = bMinV;
                    hi = bMaxV;
                    return true;
                }
            }
            lo = aMinV;
            hi = bMaxV;
            return true;
        }
        // aMax == bMax
        lo = axisOf(bMin, axis);
        if (lo <= aMinV)
            lo = aMinV;
        hi = bMaxV;
        return true;
    }
    const float bMinV = axisOf(bMin, axis);
    if (!(bMinV <= aMaxV))
        return false;
    if (aMaxV != bMinV)
    {
        const float aMinV = axisOf(aMin, axis);
        if (!(aMinV < bMinV))
        {
            // A lies inside B on this axis.
            spans = true;
            other = axisOf(bMax, o);
            if (axisOf(aMax, o) <= other)
                other = axisOf(bMin, o);
            lo = aMinV;
            hi = aMaxV;
            return true;
        }
    }
    lo = bMinV;
    hi = aMaxV;
    return true;
}

} // namespace

// ELF 0x2B09C0. Where the line through `point` (direction perpendicular to `normal`) leaves the box
// boxMin..boxMax; `point` must lie in the box. A box that is flat on either axis returns the point.
int getIntersection(const Vec2& boxMin, const Vec2& boxMax, const Vec2& normal, const Vec2& point, Vec2& out)
{
    const float px = point.x;
    const float x0 = boxMin.x;
    if (px < x0)
        return 0;
    const float x1 = boxMax.x;
    if (x1 < px)
        return 0;
    const float py = point.y;
    const float y0 = boxMin.y;
    if (py < y0)
        return 0;
    const float y1 = boxMax.y;
    if (y1 < py)
        return 0;

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    if (dx == 0.0f || dy == 0.0f)
    {
        out.x = px;
        out.y = py;
        return 1;
    }

    const float ny = normal.y;
    const float nx = normal.x;
    const float mnx = -nx;
    const float c = -(ny * px + py * mnx);
    const float at0 = ny * x0 + mnx * y0 + c;
    const float at1 = ny * x1 + mnx * y1 + c;
    if (normal.y <= 0.0f)
    {
        if (normal.y < 0.0f)
        {
            const float t = -at0 / (dx * ny);
            if (0.0f <= t && t <= 1.0f)
            {
                out.y = y0;
                out.x = x0 + dx * t;
                return 1;
            }
        }
    }
    else
    {
        const float t = at1 / (dx * ny);
        if (0.0f <= t && t <= 1.0f)
        {
            out.y = y1;
            out.x = x1 - dx * t;
            return 1;
        }
    }

    if (0.0f <= nx)
    {
        if (nx <= 0.0f)
            return 0;
        const float t = at1 / (dy * mnx);
        if (t < 0.0f)
            return 0;
        if (1.0f < t)
            return 0;
        out.x = x1;
        out.y = y1 - t * dy;
    }
    else
    {
        const float t = -at0 / (dy * mnx);
        if (t < 0.0f || 1.0f < t)
            return 0;
        out.x = x0;
        out.y = t * dy + y0;
    }
    return 1;
}

// ELF 0x2B0C30. Stepped proportional-derivative regulator for a linear quantity: `rate` is the
// change of `current` per second, the error is taken at the look-ahead point, and the step factor
// depends on the error relative to |target| in units of `scale`. `control` is clamped to [-1, 1].
void updateLinear(float target, float current, float previous, float& control, float gain, float lookAhead, float scale)
{
    const float dt = frameTime();
    const float rate = (current - previous) / dt;
    float error = target - (lookAhead * rate + current);
    const float sign = signOf(error);
    if (error < 0.0f)
        error = -error;
    float relative = error;
    if (target != 0.0f)
        relative = error / std::fabs(target);

    float step;
    if (0.0f < rate * sign)
    {
        // already moving towards the target
        if (relative < scale * 0.01f)
            step = 0.02f;
        else if (relative < scale * 0.05f)
            step = 0.27f;
        else
            step = (relative < scale * 0.1f) ? 0.55f : 1.0f;
    }
    else
    {
        if (relative < scale * 0.01f)
            return;
        if (relative < scale * 0.05f)
            step = 0.02f;
        else if (relative < scale * 0.1f)
            step = 0.27f;
        else
            step = (relative < scale * 0.4f) ? 0.55f : 1.0f;
    }
    control = control - step * dt * sign * gain * error;
    control = clampUnit(control);
}

// ELF 0x2B0DF0. Same computation as updateLinear (see the file comment).
void updateLinearDebug(float target, float current, float previous, float& control, float gain, float lookAhead, float scale)
{
    const float dt = frameTime();
    const float rate = (current - previous) / dt;
    float error = target - (lookAhead * rate + current);
    const float sign = signOf(error);
    if (error < 0.0f)
        error = -error;
    float relative = error;
    if (target != 0.0f)
        relative = error / std::fabs(target);

    float step;
    if (0.0f < rate * sign)
    {
        if (relative < scale * 0.01f)
            step = 0.02f;
        else if (relative < scale * 0.05f)
            step = 0.27f;
        else
            step = (relative < scale * 0.1f) ? 0.55f : 1.0f;
    }
    else
    {
        if (relative < scale * 0.01f)
            return;
        if (relative < scale * 0.05f)
            step = 0.02f;
        else if (relative < scale * 0.1f)
            step = 0.27f;
        else
            step = (relative < scale * 0.4f) ? 0.55f : 1.0f;
    }
    control = control - step * dt * sign * gain * error;
    control = clampUnit(control);
}

// ELF 0x2B0FE0. Angle regulator: `angle` is the remaining angle (radians), `angleChange` its change
// over the last frame. The thresholds are 0.5, 1, 2 and 4 degrees times `scale`; the gains are
// 0.01, 0.2, 0.5, 0.8, 1 and 1.5 times 180/pi (57.2957764).
void updateAngle(float angle, float angleChange, float& control, float gain, float lookAhead, float scale)
{
    const float dt = frameTime();
    const float rate = angleChange / dt;
    float error = rate * lookAhead + angle;
    const float sign = signOf(error);
    if (error < 0.0f)
        error = -error;

    float k;
    if (0.0f < rate * sign)
    {
        if (error < scale * 0.0174532924f)
            k = 28.6478882f;
        else
            k = (scale * 0.0349065848f <= error) ? 85.9436646f : 57.2957764f;
    }
    else
    {
        if (error < scale * 0.00872664619f)
            return;
        if (error < scale * 0.0174532924f)
            k = 0.572957754f;
        else if (error < scale * 0.0349065848f)
            k = 11.4591551f;
        else
            k = (scale * 0.0698131695f <= error) ? 57.2957764f : 45.8366203f;
    }
    control = control - sign * gain * dt * error * k;
    control = clampUnit(control);
}

// ELF 0x2B1170. Same computation as updateAngle.
void updateAngleDebug(float angle, float angleChange, float& control, float gain, float lookAhead, float scale)
{
    const float dt = frameTime();
    const float rate = angleChange / dt;
    float error = rate * lookAhead + angle;
    const float sign = signOf(error);
    if (error < 0.0f)
        error = -error;

    float k;
    if (0.0f < rate * sign)
    {
        if (error < scale * 0.0174532924f)
            k = 28.6478882f;
        else
            k = (scale * 0.0349065848f <= error) ? 85.9436646f : 57.2957764f;
    }
    else
    {
        if (error < scale * 0.00872664619f)
            return;
        if (error < scale * 0.0174532924f)
            k = 0.572957754f;
        else if (error < scale * 0.0349065848f)
            k = 11.4591551f;
        else
            k = (scale * 0.0698131695f <= error) ? 57.2957764f : 45.8366203f;
    }
    control = control - sign * gain * dt * error * k;
    control = clampUnit(control);
}

// ELF 0x2B1320. Intersection of the axis-aligned segment c..d with the box a..b. When `axis`
// differs from `lineAxis` the result is the single crossing point out1 (isSegment = false);
// otherwise the segment runs along `axis` and the result is the overlap out1..out2 (isSegment = true).
int getLineIntersections(Vec2& a, Vec2& b, Vec2& c, Vec2& d, Vec2& out1, Vec2& out2, bool& isSegment, int axis, int lineAxis)
{
    if (axis != lineAxis)
    {
        const float fixed = axisOf(a, lineAxis);
        if (fixed < axisOf(c, lineAxis))
            return 0;
        if (axisOf(d, lineAxis) < fixed)
            return 0;
        const float v = axisOf(c, axis);
        if (v < axisOf(a, axis))
            return 0;
        if (v <= axisOf(b, axis))
        {
            axisOf(out1, axis) = v;
            axisOf(out1, lineAxis) = axisOf(a, lineAxis);
            isSegment = false;
            return 1;
        }
        return 0;
    }

    const int o = (axis + 1) % 2;
    if (axisOf(d, o) != axisOf(c, o))
        return 0;
    const float dv = axisOf(d, axis);
    if (dv <= axisOf(a, axis))
        return 0;
    const float bv = axisOf(b, axis);
    if (bv < axisOf(c, axis))
        return 0;
    out1 = (axisOf(c, axis) <= axisOf(a, axis)) ? a : c;
    out2 = (dv <= bv) ? d : b;
    isSegment = true;
    return 1;
}

// ELF 0x2B1490. Overlap of the boxes aMin..aMax and bMin..bMax, as the segment outMin..outMax.
int getBoxIntersections(Vec2& aMin, Vec2& aMax, Vec2& bMin, Vec2& bMax, Vec2& outMin, Vec2& outMax)
{
    float lo, hi, other;
    bool spans;
    if (!boxAxisIntersection(aMin, aMax, bMin, bMax, 0, lo, hi, spans, other))
        return 0;
    outMin.x = lo;
    outMax.x = hi;
    if (spans)
    {
        outMin.y = other;
        outMax.y = other;
        return 1;
    }
    if (!boxAxisIntersection(aMin, aMax, bMin, bMax, 1, lo, hi, spans, other))
        return 0;
    outMin.y = lo;
    outMax.y = hi;
    if (spans)
    {
        outMin.x = other;
        outMax.x = other;
    }
    return 1;
}

// ELF 0x2B17E0
bool isInBox(const Vec2& point, const Vec2& boxMin, const Vec2& boxMax)
{
    bool aboveMin = false;
    if (boxMin.x <= point.x && boxMin.y <= point.y)
        aboveMin = true;
    bool insideX = false;
    if (aboveMin && point.x <= boxMax.x)
        insideX = true;
    return insideX && point.y <= boxMax.y;
}

// ELF 0x2B1870. The angle regulator with an outer limit: the level is the larger of the remaining
// angle in units of `limitDegrees` and |limitValue| / limitRange.
void updateAngleOuterLimit(float angle, float angleChange, float& control, float gain, float lookAhead, float limitDegrees, float limitValue, float limitRange)
{
    const float dt = frameTime();
    const float rate = angleChange / dt;
    const float error = rate * lookAhead + angle;
    float sign;
    if (error <= 0.0f)
        sign = (error < 0.0f) ? -1.0f : 0.0f;
    else
        sign = 1.0f;
    const float ratio = error / (limitDegrees * 0.0174532924f);
    float level = std::fabs(limitValue) / limitRange;
    if (std::fabs(limitValue) / limitRange <= ratio)
        level = ratio;
    internalAngle(error, level, sign * gain * dt, rate, control);
}

// ELF 0x2B1930. Same computation as updateAngleOuterLimit.
void updateAngleOuterLimitDebug(float angle, float angleChange, float& control, float gain, float lookAhead, float limitDegrees, float limitValue, float limitRange)
{
    const float dt = frameTime();
    const float rate = angleChange / dt;
    const float error = rate * lookAhead + angle;
    float sign;
    if (error <= 0.0f)
        sign = (error < 0.0f) ? -1.0f : 0.0f;
    else
        sign = 1.0f;
    const float ratio = error / (limitDegrees * 0.0174532924f);
    float level = std::fabs(limitValue) / limitRange;
    if (std::fabs(limitValue) / limitRange <= ratio)
        level = ratio;
    internalAngle(error, level, sign * gain * dt, rate, control);
}

// ELF 0x2B1A00. The angle regulator limited by the angular rate: the level is the larger of the
// remaining angle in units of `limitDegrees` and |rate| / maxRate.
void updateAngleAccelerationLimit(float angle, float angleChange, float& control, float gain, float lookAhead, float limitDegrees, float maxRate)
{
    const float dt = frameTime();
    const float rate = angleChange / dt;
    const float error = rate * lookAhead + angle;
    float sign;
    if (error <= 0.0f)
        sign = (error < 0.0f) ? -1.0f : 0.0f;
    else
        sign = 1.0f;
    const float ratio = error / (limitDegrees * 0.0174532924f);
    float level = std::fabs(rate) / maxRate;
    if (std::fabs(rate) / maxRate <= ratio)
        level = ratio;
    internalAngle(error, level, sign * gain * dt, rate, control);
}

// ELF 0x2B1AC0. Same computation as updateAngleAccelerationLimit.
void updateAngleAccelerationLimitDebug(float angle, float angleChange, float& control, float gain, float lookAhead, float limitDegrees, float maxRate)
{
    const float dt = frameTime();
    const float rate = angleChange / dt;
    const float error = rate * lookAhead + angle;
    float sign;
    if (error <= 0.0f)
        sign = (error < 0.0f) ? -1.0f : 0.0f;
    else
        sign = 1.0f;
    const float ratio = error / (limitDegrees * 0.0174532924f);
    float level = std::fabs(rate) / maxRate;
    if (std::fabs(rate) / maxRate <= ratio)
        level = ratio;
    internalAngle(error, level, sign * gain * dt, rate, control);
}

// ELF 0x2B1B90. The stepped gain of the limited angle regulators; `level` is in limit units
// (thresholds 0.5, 1, 2, 4).
void internalAngle(float error, float level, float factor, float rate, float& control)
{
    const float sign = signOf(error);
    if (error < 0.0f)
        error = -error;

    float k;
    if (0.0f < sign * rate)
    {
        if (level < 1.0f)
            k = 28.6478882f;
        else
            k = (level < 2.0f) ? 57.2957764f : 85.9436646f;
    }
    else
    {
        if (level < 0.5f)
            return;
        if (level < 1.0f)
            k = 0.572957754f;
        else if (level < 2.0f)
            k = 11.4591551f;
        else
            k = (level < 4.0f) ? 45.8366203f : 57.2957764f;
    }
    control = control - factor * error * k;
    control = clampUnit(control);
}

// ELF 0x2B1CE0. Adaptive bang-bang regulator. It estimates the response `gain` (rate change per
// second per unit of the last control change, low-pass 0.8/0.2, accepted only when it is below
// twice the current estimate or the estimate is still ~0), predicts the travel until the rate is
// zero and pushes, brakes or coasts. `previousValue`/`previousRate` are the caller's state.
void regulator(float target, float current, float& previousValue, float& previousRate, float& controlChange, float& gain, float strength, float& control)
{
    const float dt = frameTime();
    const float rate = (previousValue - current) / dt;
    float g;
    if (controlChange == 0.0f)
    {
        g = gain;
    }
    else
    {
        const float measured = ((rate - previousRate) / dt) / controlChange;
        g = gain;
        if (std::fabs(measured) < std::fabs(g) + std::fabs(g) || g < 1e-06f)
        {
            g = g * 0.8f + measured * 0.2f;
            gain = g;
        }
    }

    const float diff = target - current;
    const float sign = signOf(diff);
    const float time = rate / std::fabs(g);
    const float travel = -sign * g * time * time + rate * time;
    const float remaining = target - (current + travel);
    const float remainingSign = signOf(remaining);
    if (remaining != 0.0f)
    {
        float next;
        if (sign != remainingSign)
        {
            controlChange = strength * sign + control;
            next = -sign * strength;
        }
        else
        {
            if (std::fabs(travel) * 10.0f <= std::fabs(diff))
            {
                controlChange = control - sign * strength;
                control = sign * strength;
                previousValue = current;
                previousRate = rate;
                return;
            }
            next = (((rate * time + std::fabs(diff)) * -2.0f) / (time * time)) * strength * sign;
            controlChange = control - next;
        }
        control = next;
    }
    previousValue = current;
    previousRate = rate;
}

// ELF 0x2B1EF0. A retired regulator: it clamps `value` into `control` and then reports that it
// must not be used (an Error Debug, which aborts in this build).
void changeReg(float value, float, float, float, float&, float& control)
{
    static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease();   // result unused
    control = value;
    control = clampUnit(value);
    Debug(DebugTypeError, "AI/AIDLL/AI/AIPlanInterpreter/ControlFunctions.cpp", 1065, "")
        << "Not using changeReg!";
}

// ELF 0x2B2030. Clamped proportional step: -1 below -1, else 1 above 1, else the difference (NaN
// passes through, unlike clampUnit: the order of the two tests is reversed here).
void simpleReg(float target, float current, float, float& control)
{
    const float diff = target - current;
    if (-1.0f > diff)
        control = -1.0f;
    else
        control = (diff > 1.0f) ? 1.0f : diff;
}

} // namespace ai
} // namespace hfe
} // namespace dice
