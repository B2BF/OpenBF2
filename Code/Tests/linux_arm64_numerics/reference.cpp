// [TEMP] [CHOSEN, not recovered] Independent compiler oracle for the binary128 fixture.
// Run on x86-64: GCC converts IEEE binary128 to the host's actual x87 long double. The text
// corpus can then be checked by the AArch64 executable. No production arithmetic is linked.
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

namespace {
using U = unsigned __int128;
__attribute__((noinline)) long double convert(__float128 value) { return static_cast<long double>(value); }
std::uint64_t randomBits()
{
    static std::uint64_t state = 0x43ea21d915c762b9ULL;
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return state;
}
void record(FILE* out, unsigned mode, U bits)
{
    __float128 value;
    std::memcpy(&value, &bits, sizeof value);
    std::feclearexcept(FE_ALL_EXCEPT);
    const long double rounded = convert(value);
    const int raised = std::fetestexcept(FE_ALL_EXCEPT);
    const unsigned flags = ((raised & FE_INEXACT) ? 1 : 0) | ((raised & FE_UNDERFLOW) ? 2 : 0) |
                           ((raised & FE_OVERFLOW) ? 4 : 0) | ((raised & FE_INVALID) ? 8 : 0);
    unsigned long long sig;
    unsigned short se;
    std::memcpy(&sig, &rounded, 8);
    std::memcpy(&se, reinterpret_cast<const char*>(&rounded) + 8, 2);
    std::fprintf(out, "%u %016llx %016llx %04x %016llx %x\n", mode,
                 static_cast<unsigned long long>(bits >> 64), static_cast<unsigned long long>(bits), se, sig, flags);
}
}
int main(int argc, char** argv)
{
    static_assert(std::numeric_limits<long double>::digits == 64, "reference requires actual x87 long double");
    if (argc != 2) return 1;
    FILE* out = std::fopen(argv[1], "w");
    if (!out) return 2;
    std::fputs("BF2_BINARY128_X87_V1\n", out);
    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const U fractions[] = {0, 1, (U(1) << 48) - 1, U(1) << 48, (U(1) << 48) + 1,
        U(1) << 49, (U(1) << 49) + (U(1) << 48), U(1) << 111, (U(1) << 112) - 1};
    unsigned records = 0;
    for (unsigned mode = 0; mode < 4; ++mode) {
        if (std::fesetround(modes[mode]) != 0) return 3;
        for (unsigned exponent : {0u, 1u, 2u, 0x3ffeu, 0x3fffu, 0x4000u, 0x7ffeu, 0x7fffu})
            for (U fraction : fractions)
                for (unsigned sign = 0; sign < 2; ++sign) {
                    record(out, mode, (U(sign) << 127) | (U(exponent) << 112) | fraction);
                    ++records;
                }
        for (unsigned i = 0; i < 8192; ++i) {
            record(out, mode, (U(randomBits()) << 64) | randomBits());
            ++records;
        }
    }
    const bool ok = std::fclose(out) == 0;
    std::printf("wrote %u binary128-to-x87 reference records\n", records);
    return ok ? 0 : 4;
}
