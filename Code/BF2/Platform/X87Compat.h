// [CHOSEN, not recovered] Shared numeric policy for native x87 expression arms.
#ifndef BF2_PLATFORM_X87COMPAT_H
#define BF2_PLATFORM_X87COMPAT_H
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace bf2x87 {
using Real = long double;
inline bool isNaN(Real value) { return std::isnan(value); }
inline bool isFinite(Real value) { return (std::isfinite)(value); } // (): tu_math.h defines an isfinite macro
inline Real abs(Real value) { return std::fabs(value); }
inline Real squareRoot(Real value) { return std::sqrt(value); }
inline Real sinValue(Real value) { return std::sin(value); }
inline Real cosValue(Real value) { return std::cos(value); }
inline Real atanValue(Real a, Real b) { return std::atan2(a,b); }
// Load exactly, round after each arithmetic operation, and narrow only at stores.
template<class T> inline Real wide(T value)
{
    // [RETIRE-WHEN: native-x87-extended] Binary64 long-double ports still need
    // validated extended arithmetic before entering the portable inverse helpers.
    if (std::numeric_limits<long double>::digits < 64) {
        std::fprintf(stderr, "[FRONTIER] bf2x87::wide requires x87 extended precision\n");
        std::fflush(stderr);
        std::abort();
    }
    return Real(value);
}
inline Real add(Real a, Real b) { volatile Real r = a + b; return r; }
inline Real sub(Real a, Real b) { volatile Real r = a - b; return r; }
inline Real mul(Real a, Real b) { volatile Real r = a * b; return r; }
inline Real div(Real a, Real b) { volatile Real r = a / b; return r; }
inline float store(Real value) { volatile float r = static_cast<float>(value); return r; }
inline double storeDouble(Real value) { volatile double r = static_cast<double>(value); return r; }

// FCOM/FTST signal invalid for quiet NaNs too; the status is unordered.
inline bool unordered(Real a, Real b) { return isNaN(a) || isNaN(b); }
inline bool ordered(Real a, Real b) { return !unordered(a,b); }
inline void compare(Real a, Real b) { if (unordered(a,b)) std::feraiseexcept(FE_INVALID); }
inline bool less(Real a, Real b) { compare(a,b); return a < b; }
inline bool lessEqual(Real a, Real b) { compare(a,b); return a <= b; }
inline bool greater(Real a, Real b) { compare(a,b); return a > b; }
inline bool greaterEqual(Real a, Real b) { compare(a,b); return a >= b; }
inline bool notLess(Real a, Real b) { return !less(a,b); }
inline bool greaterOrUnordered(Real a, Real b) { return !lessEqual(a,b); }

// FISTP integer indefinite on masked invalid. Check the rounded result before
// converting; a C++ out-of-range float-to-int cast has undefined behaviour.
// toIntRound follows the active rounding direction (nearest uses ties to even).
// Explicit truncation leaves the caller's rounding mode unchanged.
template<class T> inline T integer(Real value, bool truncate = true)
{
    static_assert(std::numeric_limits<T>::is_signed && std::numeric_limits<T>::is_integer,
                  "signed FIST destination required");
    const Real rounded = truncate ? std::trunc(value) : std::rint(value);
    const Real bound = std::ldexp(1.0L, std::numeric_limits<T>::digits);
    if (!(rounded >= -bound && rounded < bound)) {
        std::feraiseexcept(FE_INVALID);
        return std::numeric_limits<T>::min();
    }
    return static_cast<T>(rounded);
}
inline std::int32_t toIntTruncate(Real value) { return integer<std::int32_t>(value); }
inline std::int32_t toIntRound(Real value) { return integer<std::int32_t>(value, false); }
inline std::int64_t toInt64Truncate(Real value) { return integer<std::int64_t>(value); }
inline std::int32_t lowWord(std::int64_t value)
{
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    std::int32_t result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}
// [CHOSEN, not recovered] An unsigned 32-bit cast of a floating value: the PE (_ftol2, a
// 64-bit FISTP) and x86-64 code (64-bit CVTTSD2SI) keep the low 32 bits, so a negative value
// wraps (-1.0 -> 0xFFFFFFFF). arm64 FCVTZU saturates it to 0 instead; use this where a
// negative or out-of-range value can reach the cast.
inline std::uint32_t lowUInt32(Real value)
{
    return static_cast<std::uint32_t>(toInt64Truncate(value));
}
inline short lowShort(std::int64_t value)
{
    const std::uint16_t bits = static_cast<std::uint16_t>(value);
    short result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

} // namespace bf2x87
#endif
