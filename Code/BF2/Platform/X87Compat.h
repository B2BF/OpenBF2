// [CHOSEN, not recovered] Shared numeric policy for native x87 expression arms.
#ifndef BF2_PLATFORM_X87COMPAT_H
#define BF2_PLATFORM_X87COMPAT_H
#include <cfenv>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
// The soft 80-bit Real (ExtendedFloat) replaces long double wherever long double is not the x87
// extended format: Apple silicon, BF2_TEST_SOFT_EXTENDED test builds, and GCC/Clang targets
// whose long double has a different significand precision (binary64 or binary128). Linux
// AArch64 uses binary128, which would retain bits the x87 discards. [CHOSEN, not recovered]
// Native numeric selection follows issue #36; retail is x87 throughout.
//
// BF2X87_NATIVE_EXTENDED: the compiler's long double is the x87 80-bit format (<cfloat>
// LDBL_MANT_DIG == 64), x86 and x86-64 GCC/Clang; the bf2x87 arms run on the FPU there.
//
// [CHOSEN, not recovered] Client images without that format use binary32 instead
// (BF2X87_CLIENT_FLOAT): Apple silicon and Linux AArch64 with DEDICATED=0. Retail's main thread
// runs the x87 at 24-bit precision control. BF2.exe's only _controlfp call is the CRT startup
// _controlfp(_PC_53, _MCW_PC) (0x83DBBC), and the client creates its D3D9 device with behaviour
// flags 0x50, without D3DCREATE_FPU_PRESERVE (TerrainDraw.cpp), so Direct3D leaves single
// precision in the control word: from then on every arithmetic result of the thread that runs
// rendering, game logic, collisions, the local host and the menus is rounded to 24 bits. Loads of
// int32, int64, float and double are still exact; only the exponent range of the intermediates
// differs from binary32. The software ExtendedFloat cost hundreds of milliseconds on the first
// bullet impacts on Apple silicon (kr4uzi/OpenBF2 issue #39). The dedicated server
// (DEDICATED=1), the AI library (built as DEDICATED=1) and BF2_TEST_SOFT_EXTENDED test builds keep
// the exact ExtendedFloat; BF2X87_CLIENT_EXACT_EXTENDED selects it for a client build too.
// BF2_TEST_CLIENT_FLOAT selects the binary32 arm on any host (tests).
#if !defined(_MSC_VER) && defined(LDBL_MANT_DIG) && LDBL_MANT_DIG == 64 \
    && !defined(BF2_TEST_SOFT_EXTENDED) && !defined(BF2_TEST_CLIENT_FLOAT)
#define BF2X87_NATIVE_EXTENDED 1
#endif
#if defined(BF2_TEST_CLIENT_FLOAT) \
    || (!defined(BF2X87_NATIVE_EXTENDED) && !defined(_MSC_VER) && !defined(BF2_TEST_SOFT_EXTENDED) \
        && !defined(BF2X87_CLIENT_EXACT_EXTENDED) && defined(DEDICATED) && DEDICATED == 0)
#define BF2X87_CLIENT_FLOAT 1
#elif (defined(__APPLE__) && defined(__aarch64__)) || defined(BF2_TEST_SOFT_EXTENDED) \
    || (defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ != 64)
#define BF2X87_SOFT_EXTENDED 1
#include "ExtendedFloat.h"
#endif
namespace bf2x87 {
#if defined(BF2X87_CLIENT_FLOAT)
// Real is binary32; the operations take binary64 operands (Operand), which hold every float,
// int32 and double load exactly (int64 up to 2^53), and round their result once to binary32. For binary32 operands
// that is the binary32 operation itself: binary64 has more than 2 * 24 + 2 bits, so rounding the
// binary64 sum, difference, product, quotient or root again gives the correctly rounded binary32
// result (and the compiler emits the binary32 instruction). Comparisons and integer conversions
// of an Operand are exact, as FCOM and FIST of a loaded value are.
using Real = float;
using Operand = double;
inline bool isNaN(Operand value) { return std::isnan(value); }
inline bool isFinite(Operand value) { return (std::isfinite)(value); }
inline Real abs(Real value) { return std::fabs(value); }
// FSQRT obeys precision control. FSIN, FCOS and FPATAN do not; binary64 libm rounded once to
// binary32 stands in for their extended results.
inline Real squareRoot(Operand value) { return static_cast<Real>(std::sqrt(value)); }
inline Real sinValue(Operand value) { return static_cast<Real>(std::sin(value)); }
inline Real cosValue(Operand value) { return static_cast<Real>(std::cos(value)); }
inline Real atanValue(Operand a, Operand b) { return static_cast<Real>(std::atan2(a, b)); }
#elif defined(BF2X87_SOFT_EXTENDED)
using Real = ExtendedFloat;
inline bool isNaN(Real value) { return value.isNaN(); }
inline bool isFinite(Real value) { return value.isFinite(); }
// Presentation-only transcendentals use binary64 libm on Apple silicon.
inline Real squareRoot(Real value) { return Real(std::sqrt(static_cast<double>(value))); }
inline Real sinValue(Real value) { return Real(std::sin(static_cast<double>(value))); }
inline Real cosValue(Real value) { return Real(std::cos(static_cast<double>(value))); }
inline Real atanValue(Real a, Real b) { return Real(std::atan2(static_cast<double>(a), static_cast<double>(b))); }
#else
using Real = long double;
inline bool isNaN(Real value) { return std::isnan(value); }
inline bool isFinite(Real value) { return (std::isfinite)(value); } // (): tu_math.h defines an isfinite macro
inline Real abs(Real value) { return std::fabs(value); }
inline Real squareRoot(Real value) { return std::sqrt(value); }
inline Real sinValue(Real value) { return std::sin(value); }
inline Real cosValue(Real value) { return std::cos(value); }
inline Real atanValue(Real a, Real b) { return std::atan2(a,b); }
#endif
#if !defined(BF2X87_CLIENT_FLOAT)
using Operand = Real;
#endif
// Load exactly, round after each arithmetic operation, and narrow only at stores.
template<class T> inline Real wide(T value)
{
    return Real(value);
}
// A load that must stay exact where Real is binary32: a double or a large integer that reaches a
// comparison, an integer conversion or a time difference without a binary32 rounding in between.
// The same as wide() elsewhere.
template<class T> inline Operand wideExact(T value)
{
    return Operand(value);
}
// A load whose value reaches a store, a select or a returned Real without an arithmetic operation.
// FLD of a signaling NaN raises invalid and loads it quiet. Where Real is binary32, an arithmetic
// operation does the same, and compare() and integer() raise invalid for every NaN; but a value
// that only passes through goes through no instruction at all: the optimizer folds float -> double -> float (GCC at -O2, Clang
// without -frounding-math) and storeDouble() is the identity, so the signaling NaN would be stored
// unchanged and invalid never raised. The binary32 arm performs the FLD step explicitly here; it
// is a range check and a branch that is not taken, on these loads only. Elsewhere the same as
// wideExact(): ExtendedFloat quiets the value itself, and the native arm leaves FLD to the compiler.
template<class T> inline Operand wideQuiet(T value)
{
    return Operand(value);
}
#if defined(BF2X87_CLIENT_FLOAT)
inline Real wideQuiet(float value)
{
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    // Signaling NaNs lie between the infinity and the first quiet NaN.
    if ((bits & 0x7fffffffU) - 0x7f800001U < 0x003fffffU) {
        std::feraiseexcept(FE_INVALID);
        bits |= 0x00400000U;
        std::memcpy(&value, &bits, sizeof(bits));
    }
    return value;
}
inline Operand wideQuiet(double value)
{
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if ((bits & 0x7fffffffffffffffULL) - 0x7ff0000000000001ULL < 0x0007ffffffffffffULL) {
        std::feraiseexcept(FE_INVALID);
        bits |= 0x0008000000000000ULL;
        std::memcpy(&value, &bits, sizeof(bits));
    }
    return value;
}
#endif
// MSVC is exempt: its long double is binary64, which is retail BF2.exe's own compiler semantics, and
// the Windows arms never enter bf2x87 (inline x87 on x86, SSE2 on x64). MSVC defines no
// __LDBL_MANT_DIG__, so it never selects the soft Real either - as before this check existed.
#if !defined(BF2X87_SOFT_EXTENDED) && !defined(BF2X87_CLIENT_FLOAT) && !defined(_MSC_VER)
static_assert(std::numeric_limits<long double>::digits == 64,
              "bf2x87: a non-x87 long double must select BF2X87_SOFT_EXTENDED");
#endif
#if defined(BF2X87_CLIENT_FLOAT)
inline Real add(Operand a, Operand b) { return static_cast<Real>(a + b); }
inline Real sub(Operand a, Operand b) { return static_cast<Real>(a - b); }
inline Real mul(Operand a, Operand b) { return static_cast<Real>(a * b); }
inline Real div(Operand a, Operand b) { return static_cast<Real>(a / b); }
inline float store(Operand value) { return static_cast<float>(value); }
inline double storeDouble(Operand value) { return value; }
#else
#if defined(BF2X87_SOFT_EXTENDED)
inline Real add(Real a, Real b) { return a + b; }
inline Real sub(Real a, Real b) { return a - b; }
inline Real mul(Real a, Real b) { return a * b; }
inline Real div(Real a, Real b) { return a / b; }
#else
inline Real add(Real a, Real b) { volatile Real r = a + b; return r; }
inline Real sub(Real a, Real b) { volatile Real r = a - b; return r; }
inline Real mul(Real a, Real b) { volatile Real r = a * b; return r; }
inline Real div(Real a, Real b) { volatile Real r = a / b; return r; }
#endif
inline float store(Real value) { volatile float r = static_cast<float>(value); return r; }
inline double storeDouble(Real value) { volatile double r = static_cast<double>(value); return r; }
#endif

// FCOM/FTST signal invalid for quiet NaNs too; the status is unordered.
inline bool unordered(Operand a, Operand b) { return isNaN(a) || isNaN(b); }
inline bool ordered(Operand a, Operand b) { return !unordered(a,b); }
inline void compare(Operand a, Operand b) { if (unordered(a,b)) std::feraiseexcept(FE_INVALID); }
inline bool less(Operand a, Operand b) { compare(a,b); return a < b; }
inline bool lessEqual(Operand a, Operand b) { compare(a,b); return a <= b; }
inline bool greater(Operand a, Operand b) { compare(a,b); return a > b; }
inline bool greaterEqual(Operand a, Operand b) { compare(a,b); return a >= b; }
inline bool notLess(Operand a, Operand b) { return !less(a,b); }
inline bool greaterOrUnordered(Operand a, Operand b) { return !lessEqual(a,b); }

// FISTP integer indefinite on masked invalid. Check the rounded result before
// converting; a C++ out-of-range float-to-int cast has undefined behaviour.
// toIntRound follows the active rounding direction (nearest uses ties to even).
// Explicit truncation leaves the caller's rounding mode unchanged.
template<class T> inline T integer(Operand value, bool truncate = true)
{
    static_assert(std::numeric_limits<T>::is_signed && std::numeric_limits<T>::is_integer,
                  "signed FIST destination required");
#if defined(BF2X87_SOFT_EXTENDED)
    return value.integer<T>(truncate);
#else
    const Operand rounded = truncate ? std::trunc(value) : std::rint(value);
    const Operand bound = std::ldexp(1.0L, std::numeric_limits<T>::digits);
    if (!(rounded >= -bound && rounded < bound)) {
        std::feraiseexcept(FE_INVALID);
        return (std::numeric_limits<T>::min)(); // parenthesised: <windows.h> defines min
    }
    return static_cast<T>(rounded);
#endif
}
inline std::int32_t toIntTruncate(Operand value) { return integer<std::int32_t>(value); }
inline std::int32_t toIntRound(Operand value) { return integer<std::int32_t>(value, false); }
inline std::int64_t toInt64Truncate(Operand value) { return integer<std::int64_t>(value); }
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
inline std::uint32_t lowUInt32(Operand value)
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
