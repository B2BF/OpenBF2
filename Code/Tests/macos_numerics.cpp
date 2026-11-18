// [TEMP] [CHOSEN, not recovered] Numeric scaffolding; no engine or game content.
// The exact ExtendedFloat arm (the dedicated server's), also in client test images.
#if !defined(BF2X87_CLIENT_EXACT_EXTENDED)
#define BF2X87_CLIENT_EXACT_EXTENDED 1
#endif
#include "../BF2/Platform/X87Compat.h"
#include "../BF2/Platform/ExtendedFloat.h"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#if defined(__APPLE__)
#include "../BF2/Platform/macOS/CpuInfo.h"
#endif

#pragma STDC FENV_ACCESS ON
namespace {
unsigned checks;
void check(bool ok, int line)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "macos_numerics.cpp:%d failed\n", line); std::exit(1); }
}
#define CHECK(x) check((x), __LINE__)

#if defined(__x86_64__)
// [TEMP] Processor oracle, not retail binary evidence. PC=64, exceptions masked.
long double oracle(long double a, long double b, int operation)
{
    long double result;
    switch (operation) {
    case 0: asm volatile("fldt %1; fldt %2; faddp; fstpt %0" : "=m"(result) : "m"(a), "m"(b) : "st"); break;
    case 1: asm volatile("fldt %2; fldt %1; fsubp; fstpt %0" : "=m"(result) : "m"(a), "m"(b) : "st"); break;
    case 2: asm volatile("fldt %1; fldt %2; fmulp; fstpt %0" : "=m"(result) : "m"(a), "m"(b) : "st"); break;
    default: asm volatile("fldt %2; fldt %1; fdivp; fstpt %0" : "=m"(result) : "m"(a), "m"(b) : "st"); break;
    }
    return result;
}
int oracleInt(long double value, bool truncate)
{
    unsigned short saved, cw;
    asm volatile("fnstcw %0" : "=m"(saved));
    cw = truncate ? saved | 0xc00 : saved;
    int result;
    asm volatile("fldcw %2; fldt %1; fistpl %0; fldcw %3" : "=m"(result) : "m"(value), "m"(cw), "m"(saved) : "st");
    return result;
}
std::uint64_t randomBits()
{
    static std::uint64_t state = 0x5298187ULL;
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return state;
}
long double randomExtended()
{
    unsigned char bytes[sizeof(long double)] = {};
    auto sig = randomBits() | (1ULL << 63);
    auto se = static_cast<std::uint16_t>(randomBits());
    if ((se & 0x7fff) == 0x7fff) --se;
    if (!(se & 0x7fff)) sig >>= 1; // true subnormal
    std::memcpy(bytes, &sig, 8); std::memcpy(bytes + 8, &se, 2);
    long double result; std::memcpy(&result, bytes, sizeof result); return result;
}
void differential()
{
    using S = bf2x87::ExtendedFloat;
    unsigned short saved;
    asm volatile("fnstcw %0" : "=m"(saved));
    unsigned short cw = (saved | 0x33f) & ~0xc00;
    asm volatile("fldcw %0" :: "m"(cw));
    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    for (int mode : modes) {
        CHECK(std::fesetround(mode) == 0);
        for (int i = 0; i < 20000; ++i) {
            const long double a = randomExtended(), b = randomExtended();
            const S sa(a), sb(b);
            for (int op = 0; op < 4; ++op) {
                const long double expected = oracle(a, b, op);
                const S actual = op == 0 ? sa + sb : op == 1 ? sa - sb : op == 2 ? sa * sb : sa / sb;
                CHECK(std::isnan(expected) ? actual.isNaN() : actual == S(expected));
                const float ef = static_cast<float>(expected), af = static_cast<float>(actual);
                CHECK((std::isnan(ef) && std::isnan(af)) || (ef == af && std::signbit(ef) == std::signbit(af)));
                const double ed = static_cast<double>(expected), ad = static_cast<double>(actual);
                CHECK((std::isnan(ed) && std::isnan(ad)) || (ed == ad && std::signbit(ed) == std::signbit(ad)));
            }
            CHECK(sa.integer<int>(true) == oracleInt(a, true));
            CHECK(sa.integer<int>(false) == oracleInt(a, false));
        }
    }
    asm volatile("fldcw %0" :: "m"(saved));
}
#endif
}
int main()
{
    using namespace bf2x87;
    const int saved = std::fegetround();
    CHECK(std::fesetround(FE_TONEAREST) == 0);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double value : {nan, inf, -inf, 2147483648.0, -2147483649.0, 1e100, -1e100}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        CHECK(toIntTruncate(wide(value)) == INT_MIN);
        CHECK(std::fetestexcept(FE_INVALID));
        std::feclearexcept(FE_ALL_EXCEPT);
        CHECK(toIntRound(wide(value)) == INT_MIN);
        CHECK(std::fetestexcept(FE_INVALID));
    }
    for (double value : {0.0, -0.0, 0.5, -0.5}) CHECK(toIntTruncate(wide(value)) == 0);
    CHECK(toIntTruncate(wide(2147483647.99)) == INT_MAX);
    CHECK(toIntTruncate(wide(-2147483648.99)) == INT_MIN);
    CHECK(toIntRound(wide(2147483647.5)) == INT_MIN);
    CHECK(toIntRound(wide(-2147483648.5)) == INT_MIN);
    CHECK(toIntRound(wide(0.5)) == 0 && toIntRound(wide(-0.5)) == 0);
    CHECK(toIntRound(wide(1.5)) == 2 && toIntRound(wide(2.5)) == 2);
    CHECK(toIntRound(wide(-1.5)) == -2 && toIntRound(wide(-2.5)) == -2);
    CHECK(toInt64Truncate(wide(LLONG_MAX)) == LLONG_MAX);
    CHECK(toInt64Truncate(wide(LLONG_MIN)) == LLONG_MIN);
    CHECK(toInt64Truncate(wide(nan)) == LLONG_MIN);
    CHECK(toInt64Truncate(wide(inf)) == LLONG_MIN);
    CHECK(toInt64Truncate(wide(-inf)) == LLONG_MIN);
    CHECK(toIntTruncate(wide(std::nextafter(2147483648.0, 0.0))) == INT_MAX);
    CHECK(toIntTruncate(wide(std::nextafter(-2147483649.0, 0.0))) == INT_MIN);
    CHECK(toIntRound(wide(std::nextafter(2147483647.5, 0.0))) == INT_MAX);
    CHECK(integer<short>(wide(32768)) == SHRT_MIN);
    CHECK(integer<short>(wide(32767.9)) == SHRT_MAX);
    for (int mode : {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        CHECK(std::fesetround(mode) == 0);
        CHECK(toIntTruncate(wide(-1.9)) == -1 && std::fegetround() == mode);
        CHECK(toIntRound(wide(1.5)) == (mode == FE_UPWARD ? 2 : 1));
        CHECK(toIntRound(wide(-1.5)) == (mode == FE_DOWNWARD ? -2 : -1));
    }
    CHECK(std::fesetround(FE_TONEAREST) == 0);
    CHECK(ordered(wide(0.0), wide(-0.0)));
    CHECK(unordered(wide(nan), wide(1)) && unordered(wide(1), wide(nan)));
    CHECK(!less(wide(nan), 0) && !lessEqual(wide(nan), 0) && !greater(wide(nan), 0));
    CHECK(notLess(wide(nan), 0) && greaterOrUnordered(wide(nan), 0));
    CHECK(!greaterEqual(wide(nan), 0) && !ordered(wide(nan), wide(nan)));
    CHECK(less(wide(-inf), wide(inf)) && !unordered(wide(-inf), wide(inf)));
    CHECK(std::signbit(store(wide(-0.0))) && !std::signbit(store(bf2x87::abs(wide(-0.0)))));

    // Observable cancellation/tie boundaries in AI time and squared distances.
    CHECK(wide(9007199254741002.0) > wide(9007199254740992.0) + wide(9));
    CHECK(wide(9007199254741000.0) < wide(9007199254740992.0) + wide(9));
    CHECK(wide(1) + wide(std::ldexp(1.0, -63)) > wide(1));
    CHECK(wide(1) + wide(std::ldexp(1.0, -64)) == wide(1));
    CHECK((wide(1) + wide(std::ldexp(1.0, -63))) - wide(1) == wide(std::ldexp(1.0, -63)));
    CHECK(store((wide(1) + wide(std::ldexp(1.0, -24))) + wide(std::ldexp(1.0, -60))) == std::nextafter(1.0f, 2.0f));
    const Real maximum = wide(std::numeric_limits<double>::max());
    CHECK(isFinite(maximum * maximum));
    CHECK(storeDouble((maximum * maximum) / maximum) == std::numeric_limits<double>::max());
    const Real minimum = wide(std::numeric_limits<double>::denorm_min());
    CHECK(minimum / wide(2) > wide(0) && storeDouble(minimum / wide(2)) == 0);
    CHECK(storeDouble((minimum / wide(2)) * wide(2)) == std::numeric_limits<double>::denorm_min());

#if defined(__x86_64__)
    differential();
    CHECK(std::fesetround(FE_TONEAREST) == 0);
    // Rule 2: demonstrate why a client PC=24 oracle needs tolerances.
    unsigned short cw, single;
    asm volatile("fnstcw %0" : "=m"(cw));
    single = cw & ~0x300;
    asm volatile("fldcw %0" :: "m"(single));
    CHECK(oracle(16777216.0L, 1.0L, 0) == 16777216.0L);
    asm volatile("fldcw %0" :: "m"(cw));
    CHECK(oracle(16777216.0L, 1.0L, 0) == 16777217.0L);
#endif
    CHECK(std::fesetround(saved) == 0);
#if defined(__APPLE__)
    bf2macos::CpuInfo cpu;
    CHECK(bf2macos::readCpuInfo(cpu));
    CHECK(cpu.name[0] != '\0');
    std::printf("sysctl CPU: %s; optional frequency: %llu Hz\n", cpu.name,
                static_cast<unsigned long long>(cpu.frequencyHz));
#endif
    std::printf("numerics: %u checks passed (%s)\n", checks,
#if defined(__aarch64__)
        "arm64 software extended"
#else
        "x86_64 with hardware x87 differential"
#endif
    );
}
