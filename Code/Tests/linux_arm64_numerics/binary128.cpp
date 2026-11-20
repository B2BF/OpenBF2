// [TEMP] [CHOSEN, not recovered] Linux binary128-to-native-x87 conversion contracts, issue #36.
// Runs with the actual AArch64 long-double ABI; no engine, graphics, content or networking.
// The exact ExtendedFloat arm (the dedicated server's), also in this client test image.
#if !defined(BF2X87_CLIENT_EXACT_EXTENDED)
#define BF2X87_CLIENT_EXACT_EXTENDED 1
#endif
#include "../../BF2/Platform/X87Compat.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <type_traits>

#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 113
namespace {
using U = unsigned __int128;
using S = bf2x87::ExtendedFloat;
unsigned checks = 0;
void check(bool condition, const char* label)
{
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
long double value(bool negative, unsigned exponent, U fraction)
{
    const U bits = (U(negative) << 127) | (U(exponent) << 112) | fraction;
    long double result;
    static_assert(sizeof(result) == sizeof(bits), "binary128 storage required");
    std::memcpy(&result, &bits, sizeof result);
    return result;
}
unsigned exceptions()
{
    const int bits = std::fetestexcept(FE_ALL_EXCEPT);
    return ((bits & FE_INEXACT) ? 1 : 0) | ((bits & FE_UNDERFLOW) ? 2 : 0) |
           ((bits & FE_OVERFLOW) ? 4 : 0) | ((bits & FE_INVALID) ? 8 : 0);
}
S convert(bool negative, unsigned exponent, U fraction)
{
    std::feclearexcept(FE_ALL_EXCEPT);
    return S(value(negative, exponent, fraction));
}
S scale(int exponent)
{
    S result(1), power(exponent < 0 ? 0.5 : 2.0);
    unsigned remaining = exponent < 0 ? -exponent : exponent;
    while (remaining) {
        if (remaining & 1) result = result * power;
        remaining >>= 1;
        if (remaining) power = power * power;
    }
    return result;
}
void reference(const char* path)
{
    std::ifstream input(path);
    std::string magic;
    check(bool(input >> magic) && magic == "BF2_BINARY128_X87_V1", "reference corpus header");
    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    unsigned mode, signExponent, flags;
    unsigned long long high, low, significand;
    unsigned records = 0;
    while (input >> std::dec >> mode >> std::hex >> high >> low >> signExponent >> significand >> flags) {
        check(mode < 4 && std::fesetround(modes[mode]) == 0, "reference rounding mode");
        const U bits = (U(high) << 64) | low;
        const bool negative = (bits >> 127) != 0;
        const S actual = convert(negative, static_cast<unsigned>((bits >> 112) & 0x7fff), bits & ((U(1) << 112) - 1));
        const unsigned actualFlags = exceptions();
        const unsigned exponent = signExponent & 0x7fff;
        bool matches;
        if (exponent == 0x7fff) {
            matches = (significand & 0x7fffffffffffffffULL) ? actual.isNaN() : actual.isInf();
        } else {
            S expected = S(significand) * scale((exponent ? static_cast<int>(exponent) - 16383 : -16382) - 63);
            if (signExponent & 0x8000) expected = -expected;
            matches = actual == expected;
        }
        matches = matches && std::signbit(static_cast<double>(actual)) == bool(signExponent & 0x8000);
        if (!matches || actualFlags != flags) {
            std::fprintf(stderr, "record %u mode %u input %016llx%016llx expected x87 %04x/%016llx flags %x got flags %x value-match %d\n",
                         records, mode, high, low, signExponent, significand, flags, actualFlags, int(matches));
        }
        check(matches, "GCC binary128-to-x87 reference value");
        check(actualFlags == flags, "GCC binary128-to-x87 reference exceptions");
        ++records;
    }
    check(input.eof() && records > 1000, "complete reference corpus");
    std::printf("binary128 reference: %u records passed\n", records);
}
}
#endif

int main(int argc, char** argv)
{
#if !defined(__LDBL_MANT_DIG__) || __LDBL_MANT_DIG__ != 113
    std::puts("SKIP: this test requires actual IEEE binary128 long double");
    return 77;
#else
    static_assert(std::is_same<bf2x87::Real, S>::value, "Linux binary128 must select software x87");
    const int saved = std::fegetround();
    check(std::fesetround(FE_TONEAREST) == 0, "nearest rounding");
    const S one(1), ulp = one / S(1ULL << 63);
    const S beyondDouble = convert(false, 16383, U(1) << 52);
    check(exceptions() == 0 && beyondDouble > one && beyondDouble == one + one / S(1ULL << 60), "preserve precision beyond binary64");
    check(convert(false, 16383, U(1) << 48) == one && exceptions() == 1, "halfway rounds to even");
    check(convert(false, 16383, (U(1) << 48) + 1) == one + ulp && exceptions() == 1, "above halfway rounds up");
    check(convert(false, 16383, (U(1) << 49) + (U(1) << 48)) == one + ulp + ulp && exceptions() == 1, "odd halfway rounds to even");
    const int modes[] = {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    for (int mode : modes) {
        check(std::fesetround(mode) == 0, "directed rounding");
        check(convert(false, 16383, 1) == (mode == FE_UPWARD ? one + ulp : one), "positive directed precision");
        check(convert(true, 16383, 1) == (mode == FE_DOWNWARD ? -one - ulp : -one), "negative directed precision");
    }
    check(std::fesetround(FE_TONEAREST) == 0, "restore nearest");
    const S large = convert(false, 16383 + 2000, 0), small = convert(false, 16383 - 2000, 0);
    check(large.isFinite() && small > S(0) && large * small == one, "preserve exponent range beyond binary64");
    const S tiny = convert(false, 0, U(1) << 49);
    check(exceptions() == 0 && tiny > S(0), "exact minimum x87 subnormal");
    check(tiny * scale(8192) * scale(8192) == scale(-61), "minimum subnormal magnitude");
    check(convert(false, 0, 1) == S(0) && exceptions() == 3, "minimum binary128 rounds to zero with underflow");
    check(std::fesetround(FE_UPWARD) == 0, "upward tiny rounding");
    check(convert(false, 0, 1) == tiny && exceptions() == 3, "tiny upward conversion retains minimum x87 value");
    check(std::fesetround(FE_TONEAREST) == 0, "nearest tiny rounding");
    const S normalMinimum = convert(false, 1, 0);
    check(exceptions() == 0 && normalMinimum > tiny, "exact minimum normal");
    check(convert(false, 0, (U(1) << 112) - 1) == normalMinimum && exceptions() == 1, "tininess checked after rounding");
    check(convert(false, 0x7ffe, (U(1) << 112) - 1).isInf() && exceptions() == 5, "overflow follows nearest rounding");
    const S negativeZero = convert(true, 0, 0);
    check(exceptions() == 0 && std::signbit(static_cast<double>(negativeZero)), "negative zero sign");
    const S infinity = convert(true, 0x7fff, 0);
    check(exceptions() == 0 && infinity.isInf() && std::signbit(static_cast<double>(infinity)), "negative infinity");
    const S quiet = convert(true, 0x7fff, (U(1) << 111) | 1);
    check(exceptions() == 0 && quiet.isNaN() && std::signbit(static_cast<double>(quiet)), "quiet NaN remains NaN");
    const S signaling = convert(false, 0x7fff, 1);
    check(exceptions() == 8 && signaling.isNaN(), "low-payload signaling NaN raises invalid without becoming infinity");
    if (argc > 1) reference(argv[1]);
    std::fesetround(saved);
    std::printf("binary128 conversion: %u checks passed\n", checks);
    return 0;
#endif
}
