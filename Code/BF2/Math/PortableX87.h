// [CHOSEN, not recovered] Native C++ arms for the MSVC x86 expression blocks.
#ifndef BF2_MATH_PORTABLEX87_H
#define BF2_MATH_PORTABLEX87_H

#include "../Platform/X87Compat.h"
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace bf2x87 {

// x87 leaves an out-of-range finite trig operand unchanged (C2=1).
// Native libm supplies the transcendental approximation inside that range.
inline Real sine(Real a) { return isFinite(a) && abs(a) >= std::ldexp(1.0L,63) ? a : sinValue(a); }
inline Real cosine(Real a) { return isFinite(a) && abs(a) >= std::ldexp(1.0L,63) ? a : cosValue(a); }
inline void trig(float angle, float& sineOut, float& cosineOut)
{
    if (isFinite(angle) && abs(wide(angle)) >= std::ldexp(1.0L,63)) {
        // FSINCOS did not push a cosine. The second FSTP underflows the stack.
        cosineOut = angle;
        const std::uint32_t indefinite = 0xffc00000U;
        std::memcpy(&sineOut, &indefinite, 4);
        std::feraiseexcept(FE_INVALID);
        return;
    }
    cosineOut = store(cosine(wide(angle)));
    sineOut = store(sine(wide(angle)));
}

inline float multiply(float a, float b)
{
    // FLD quiets a signaling source before FMUL chooses the larger NaN payload.
    std::uint32_t aa, bb;
    std::memcpy(&aa, &a, 4); std::memcpy(&bb, &b, 4);
    const bool an = (aa & 0x7fffffffU) > 0x7f800000U;
    const bool bn = (bb & 0x7fffffffU) > 0x7f800000U;
    if ((an && !(aa & 0x400000U)) || (bn && !(bb & 0x400000U)))
        std::feraiseexcept(FE_INVALID);
    if (an) aa |= 0x400000U;
    if (an && bn) {
        if (bb & 0x400000U) {
            if ((bb & 0x7fffffffU) > (aa & 0x7fffffffU) ||
                ((bb & 0x7fffffffU) == (aa & 0x7fffffffU) && bb < aa)) aa = bb;
        }
        std::memcpy(&a, &aa, 4);
        return a;
    }
    return store(mul(wide(a), wide(b)));
}
inline float offset(float value, float start, float origin)
{ return store(add(sub(wide(value), wide(start)), wide(origin))); }
inline float center(float origin, float dimension)
{ return store(sub(wide(origin), mul(wide(dimension), 0.5L))); }
inline float multiplyAdd(float value, float factor, float translation)
{ return store(add(mul(wide(value), wide(factor)), wide(translation))); }
inline Real decoded(float value)
{ const Real loaded = wide(value); return less(loaded,0) ? -add(loaded, 1.0L) : loaded; }
inline bool sumNotLess(float a, float b, float limit)
{ return notLess(add(wide(a), wide(b)), wide(limit)); }
inline void fadeInPlace(float& current, float delta, float duration, bool increase)
{
    const Real step = div(wide(delta), wide(duration));
    const Real next = increase ? add(step, wide(current)) : sub(wide(current), step);
    current = store(next);
    if (increase) { if (greater(next,1)) current = 1.0f; }
    else if (less(next,0)) current = 0.0f;
}
inline float fade(float current, float delta, float duration, bool increase)
{ fadeInPlace(current,delta,duration,increase); return current; }
inline bool chooseValue(float current, float target, float delta, float speed,
                        float braking, bool eased, float& value)
{
    const Real loadedCurrent=wide(current), loadedTarget=wide(target);
    if (!less(loadedCurrent,loadedTarget) && !greater(loadedCurrent,loadedTarget)) return false;
    const bool increasing = less(loadedCurrent,loadedTarget);
    const Real distance = increasing ? sub(wide(target), wide(current)) : sub(wide(current), wide(target));
    Real step;
    if (eased && less(distance,wide(braking))) {
        const Real angle = wide(1.57075f);
        step = mul(mul(cosine(sub(angle, mul(div(distance, wide(braking)), angle))), wide(speed)), wide(delta));
    } else step = mul(wide(delta), wide(speed));
    const Real next = increasing ? add(step, wide(current)) : sub(wide(current), step);
    const float rounded = store(next);
    value = (increasing ? greater(next,wide(target)) : less(next,wide(target))) ? target : rounded;
    return true;
}
inline void fractionalDigits(double magnitude, int integral, int digits, char*& cursor)
{
    Real remainder = sub(wide(magnitude), wide(integral));
    // The original is a do/while, including its caller precondition digits > 0.
    do {
        remainder = mul(remainder, 10.0L);
        const int digit = integer<int>(remainder);
        *cursor++ = static_cast<char>(static_cast<unsigned char>(digit) + 0x30U);
        remainder = sub(remainder, wide(digit));
    } while (--digits != 0);
}
inline bool waitRemaining(double last, double cooldown, double now, double& remaining)
{
    const Real next = sub(wide(cooldown), mul(sub(wide(now), wide(last)), wide(1.0 / 60.0)));
    if (!greaterOrUnordered(next, 0)) return false;
    remaining = storeDouble(next);
    return true;
}
inline bool abilityPosition(float x, float y)
{ return greaterOrUnordered(add(abs(sub(wide(y), -4000.0L)), abs(sub(wide(x), -4000.0L))), wide(std::numeric_limits<float>::epsilon())); }
inline void beginCooldown(double now, const float& duration, float& remaining,
                          bool& reloading, float& deadline, bool supply)
{
    const Real next = add(wide(now), wide(duration));
    remaining = duration;
    if (supply) reloading = true;
    deadline = store(next);
    if (!supply) reloading = true;
}
inline void movePointer(float* position, float x, float y, int sensitivity,
                        bool absolute, float width, float height)
{
    if (absolute) {
        position[0] = store(mul(mul(wide(x), wide(width)), 0.5L));
        position[1] = store(mul(mul(wide(y), wide(height)), 0.5L));
    } else {
        position[0] = store(add(mul(wide(x), wide(sensitivity)), wide(position[0])));
        position[1] = store(add(mul(wide(sensitivity), wide(y)), wide(position[1])));
    }
}
inline void clampCoordinate(float& value, float origin, float extent)
{
    if (less(wide(value),wide(origin))) value = origin;
    else {
        const Real upper = add(wide(extent), wide(origin));
        if (less(upper,wide(value))) value = store(upper);
    }
}
inline float affine(float a, float ma, float b, float mb, float c, float mc, float translation)
{ return store(add(add(add(mul(wide(a), wide(ma)), mul(wide(b), wide(mb))), mul(wide(c), wide(mc))), wide(translation))); }
inline void project(float x, float y, float z, float height, float halfWidth,
                    float& outputX, float& outputY, float& outputZ)
{
    const Real halfHeight = mul(wide(height), 0.5L);
    const Real inverse = div(1.0L, wide(z));
    outputX = store(mul(mul(wide(x), inverse), wide(halfWidth)));
    outputY = store(mul(mul(-halfHeight, wide(y)), inverse));
    outputZ = store(inverse);
}
inline float distanceWithX(Real x, float fromY, float fromZ, float toY,
                           float toZ, float scale)
{
    const Real z = sub(wide(toZ), wide(fromZ));
    const Real y = sub(wide(toY), wide(fromY));
    return store(mul(squareRoot(add(add(mul(z,z),mul(x,x)),mul(y,y))),wide(scale)));
}
inline float distance(float fromX, float fromY, float fromZ, float toX, float toY,
                      float toZ, float scale)
{
    const Real z = sub(wide(toZ), wide(fromZ));
    const Real x = sub(wide(toX), wide(fromX));
    const Real y = sub(wide(toY), wide(fromY));
    return store(mul(squareRoot(add(add(mul(z,z),mul(x,x)),mul(y,y))),wide(scale)));
}
inline void pullPoint(float hx, float hy, float hz, float ox, float oy, float oz, float* output)
{
    const Real z = sub(wide(hz), wide(oz)); const float dz = store(z);
    const Real y = sub(wide(hy), wide(oy)); const float dy = store(y);
    const Real x = sub(wide(hx), wide(ox));
    const Real length = squareRoot(add(add(mul(z,z),mul(y,y)),mul(x,x)));
    const float magnitude = store(length);
    if (!greater(length,wide(0.2f))) return;
    const float scale = store(div(wide(0.2f), wide(magnitude)));
    const float offsetX = store(mul(x,wide(scale)));
    output[0] = store(sub(wide(hx),wide(offsetX)));
    output[1] = store(sub(wide(hy),mul(wide(dy),wide(scale))));
    output[2] = store(sub(wide(hz),mul(wide(dz),wide(scale))));
}
inline bool subtractTimer(float& timer, float delta, bool inclusive)
{
    const Real next = sub(wide(timer), wide(delta)); timer = store(next);
    return inclusive ? lessEqual(next,0) : less(next,0);
}
inline bool addDemoTimer(float& timer, float delta)
{
    const Real next = add(wide(delta),wide(timer)); timer = store(next);
    return lessEqual(10,next);
}
inline float mapCoordinate(float world, float camera, float scale, bool alive)
{
    Real next = mul(wide(world), 0.5L);
    if (alive) next = add(next,wide(camera));
    return store(mul(next,wide(scale)));
}
inline float heading(float x, float z)
{ return store(atanValue(wide(x),wide(z))); }
inline float armorRatio(float points, float maximum, float previous, bool blend)
{
    Real ratio = div(wide(points),wide(maximum));
    if (blend) ratio = mul(add(ratio,wide(previous)),0.5L);
    return store(ratio);
}
inline float ammoRatio(int status, float previous, bool blend)
{
    Real ratio = mul(wide(status),wide(0.06666667014360428f));
    if (blend) ratio = mul(add(ratio,wide(previous)),0.5L);
    return store(ratio);
}
inline void cooldown(float deadline, double now, float duration, float& remaining, float& fraction)
{
    const Real next = sub(wide(deadline),wide(now));
    remaining = store(next); fraction = store(div(next,wide(duration)));
}
inline float glyphWidth(float third, float second, float first, float width)
{ return store(add(add(add(wide(third),wide(second)),wide(first)),wide(width))); }
inline void cofactor(float& output, double a0, double b0, double a1, double b1,
                     double a2, double b2, double a3, double b3, double a4, double b4,
                     double a5, double b5)
{
    output = store(add(add(mul(wide(a0),wide(b0)),mul(wide(a1),wide(b1))),mul(wide(a2),wide(b2))));
    output = store(sub(wide(output), add(add(mul(wide(a3),wide(b3)),mul(wide(a4),wide(b4))),mul(wide(a5),wide(b5)))));
}
inline void divideByDeterminant(const float* input, float* output)
{
    Real determinant = add(mul(wide(input[4]),wide(output[1])),mul(wide(input[0]),wide(output[0])));
    determinant = add(determinant,mul(wide(input[8]),wide(output[2])));
    determinant = add(determinant,mul(wide(input[12]),wide(output[3])));
    const Real inverse = div(1.0L,determinant);
    for (int i=0;i<16;++i) output[i] = store(mul(inverse,wide(output[i])));
}
inline void lod(float lodScale, float view, float nearRatio, float farRatio,
                float& base, float& nearDistance, float& inverseNear, float& farDistance, float& inverseFar)
{
    const Real product = mul(wide(lodScale),wide(view)); base = store(product);
    const Real near = mul(product,wide(nearRatio)); nearDistance = store(near);
    inverseNear = store(div(1.0L,sub(wide(base),near)));
    const Real far = mul(wide(farRatio),wide(base)); farDistance = store(far);
    inverseFar = store(div(1.0L,sub(wide(base),far)));
}
inline float effectiveDistance(float renderer, float global, float factor)
{
    const Real next = mul(mul(wide(renderer),wide(global)),wide(factor));
    return greater(next,200) ? store(next) : 200.0f;
}
inline void packedRotation(float angle, short& sineOut, short& cosineOut)
{
    sineOut = lowShort(integer<std::int64_t>(mul(sine(wide(angle)),32767.0L)));
    cosineOut = lowShort(integer<std::int64_t>(mul(cosine(wide(angle)),32767.0L)));
}
inline void tableRotation(unsigned index, short& sineOut, short& cosineOut)
{
    // FILD dword is signed even though the C++ parameter is unsigned.
    const Real angle = mul(wide(lowWord(index)),wide(0.0015339808305725455284f));
    sineOut = integer<short>(mul(sine(angle),32767.0L));
    cosineOut = integer<short>(mul(cosine(angle),32767.0L));
}
inline void zoom(float radians, float halfScale, float reference, float& zoomOut, float& inverseOut)
{
    const Real angle = mul(wide(radians),wide(halfScale));
    const float ref = store(mul(wide(reference),wide(halfScale)));
    const Real ratio = div(div(cosine(angle),sine(angle)),div(cosine(wide(ref)),sine(wide(ref))));
    zoomOut = store(ratio);
    inverseOut = ratio == 0 ? 0.0f : store(div(1.0L,ratio));
}
inline float consoleFloat(const float* input, bool* returnFlag)
{
    const Real loaded = wide(*input);
    *returnFlag = true;
    return store(loaded);
}
} // namespace bf2x87
#endif
