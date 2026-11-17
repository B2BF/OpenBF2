// [CHOSEN, not recovered] Arithmetic for native ports' retained x87 intermediates.
#ifndef BF2_PLATFORM_EXTENDEDFLOAT_H
#define BF2_PLATFORM_EXTENDEDFLOAT_H

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace bf2x87 {

class ExtendedFloat;
inline ExtendedFloat abs(ExtendedFloat value);

// A binary extended value, with a 64-bit significand and x87 exponent range.
// Used when host long double is not x87 extended, including Apple and Linux ARM64. This is
// value arithmetic, not an x87 stack/control-word emulator. Operations obey the
// current C rounding direction; precision is always 64 bits. No fused operations.
class ExtendedFloat {
    using UInt = unsigned __int128;
    std::uint64_t significand_ = 0;
    int exponent_ = 0; // exponent of the leading bit, including subnormals
    bool negative_ = false;
    static constexpr int special = 32768;

    static int width(UInt value)
    {
        const auto high = static_cast<std::uint64_t>(value >> 64);
        return high ? 128 - __builtin_clzll(high) :
            value ? 64 - __builtin_clzll(static_cast<std::uint64_t>(value)) : 0;
    }
    static UInt jam(UInt value, int shift)
    {
        if (shift <= 0) return value;
        if (shift >= 128) return value != 0;
        return (value >> shift) | ((value << (128 - shift)) != 0);
    }
    static bool increment(bool negative, UInt whole, UInt remainder, int shift)
    {
        if (!remainder) return false;
        switch (std::fegetround()) {
        case FE_UPWARD: return !negative;
        case FE_DOWNWARD: return negative;
        case FE_TOWARDZERO: return false;
        default:
            if (shift > 128) return false;
            const UInt half = UInt(1) << (shift - 1);
            return remainder > half || (remainder == half && (whole & 1));
        }
    }
    static UInt rounded(bool negative, UInt value, int shift)
    {
        if (shift <= 0) return value << -shift;
        const UInt whole = shift >= 128 ? 0 : value >> shift;
        const UInt remainder = shift >= 128 ? value : value & ((UInt(1) << shift) - 1);
        return whole + increment(negative, whole, remainder, shift);
    }
    static ExtendedFloat raw(bool negative, std::uint64_t sig, int exponent)
    {
        ExtendedFloat result;
        result.negative_ = negative; result.significand_ = sig; result.exponent_ = exponent;
        return result;
    }
    static ExtendedFloat pack(bool negative, UInt value, int unit)
    {
        if (!value) return raw(negative, 0, 0);
        const int shift = (width(value) - 64 > -16445 - unit) ?
            width(value) - 64 : -16445 - unit;
        UInt sig = rounded(negative, value, shift);
        unit += shift;
        if (!sig) return raw(negative, 0, 0);
        if (width(sig) > 64) { sig >>= 1; ++unit; }
        const int bits = width(sig);
        const int exponent = unit + bits - 1;
        if (exponent > 16383) {
            std::feraiseexcept(FE_OVERFLOW | FE_INEXACT);
            const int mode = std::fegetround();
            return mode == FE_TOWARDZERO || (mode == FE_DOWNWARD && !negative) ||
                (mode == FE_UPWARD && negative) ? raw(negative, UINT64_MAX, 16383) : infinity(negative);
        }
        return raw(negative, static_cast<std::uint64_t>(sig) << (64 - bits), exponent);
    }
    template<class T, class Bits, int Precision, int Bias> T narrow() const
    {
        constexpr int fraction = Precision - 1;
        constexpr int exponentBits = sizeof(Bits) * 8 - Precision;
        const Bits sign = Bits(negative_) << (sizeof(Bits) * 8 - 1);
        const Bits infinityBits = ((Bits(1) << exponentBits) - 1) << fraction;
        Bits result = sign;
        if (isNaN()) result |= infinityBits | (Bits(1) << (fraction - 1)) |
            (static_cast<Bits>(significand_ >> (63 - fraction)) & ((Bits(1) << fraction) - 1));
        else if (isInf()) result |= infinityBits;
        else if (significand_) {
            const int shift = (64 - Precision > (1 - Bias) - exponent_ + 64 - Precision) ?
                64 - Precision : (1 - Bias) - exponent_ + 64 - Precision;
            UInt sig = rounded(negative_, significand_, shift);
            int exponent = exponent_;
            if (sig >= (UInt(1) << Precision)) { sig >>= 1; ++exponent; }
            if (exponent > Bias) {
                const int mode = std::fegetround();
                result |= mode == FE_TOWARDZERO || (mode == FE_DOWNWARD && !negative_) ||
                    (mode == FE_UPWARD && negative_) ? infinityBits - 1 : infinityBits;
            } else if (sig < (UInt(1) << fraction)) result |= static_cast<Bits>(sig);
            else {
                if (exponent < 1 - Bias) exponent = 1 - Bias;
                result |= (Bits(exponent + Bias) << fraction) |
                    (static_cast<Bits>(sig) & ((Bits(1) << fraction) - 1));
            }
        }
        T output; std::memcpy(&output, &result, sizeof output); return output;
    }
public:
    ExtendedFloat() = default;
    ExtendedFloat(double value)
    {
        std::uint64_t bits; std::memcpy(&bits, &value, 8);
        negative_ = bits >> 63;
        const int exponent = (bits >> 52) & 2047;
        const std::uint64_t fraction = bits & 0xfffffffffffffULL;
        if (exponent == 2047) {
            exponent_ = special; significand_ = fraction << 11;
            if (fraction && !(fraction & (1ULL << 51))) std::feraiseexcept(FE_INVALID);
        }
        else *this = pack(negative_, exponent ? fraction | (1ULL << 52) : fraction,
                          exponent ? exponent - 1023 - 52 : -1074);
    }
    ExtendedFloat(float value) : ExtendedFloat(static_cast<double>(value)) {}
    ExtendedFloat(int value) : ExtendedFloat(static_cast<long long>(value)) {}
    ExtendedFloat(unsigned value) : ExtendedFloat(static_cast<unsigned long long>(value)) {}
    ExtendedFloat(long value) : ExtendedFloat(static_cast<long long>(value)) {}
    ExtendedFloat(unsigned long value) : ExtendedFloat(static_cast<unsigned long long>(value)) {}
    ExtendedFloat(long long value)
    {
        const auto bits = static_cast<unsigned long long>(value);
        *this = pack(value < 0, value < 0 ? 0ULL - bits : bits, 0);
    }
    ExtendedFloat(unsigned long long value) { *this = pack(false, value, 0); }
    ExtendedFloat(long double value)
    {
#if defined(__x86_64__)
        // The test fixture can load the host's exact extended operands.
        unsigned char bytes[sizeof value]; std::memcpy(bytes, &value, sizeof value);
        std::uint64_t sig; std::uint16_t se;
        std::memcpy(&sig, bytes, 8); std::memcpy(&se, bytes + 8, 2);
        const int exp = se & 0x7fff;
        if (exp == 0x7fff) *this = raw(se >> 15, sig & 0x7fffffffffffffffULL, special);
        else *this = pack(se >> 15, sig, (exp ? exp - 16383 : -16382) - 63);
#elif defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 113
        // [CHOSEN, not recovered] Linux ARM64 (issue #36): AAPCS64 long double is IEEE binary128,
        // with sign at 127, exponent at 112..126, and fraction at 0..111. Loading its bits avoids
        // rounding through binary64 before pack applies the native x87 precision policy.
        // Format: https://docs.oracle.com/cd/E77782_01/html/E77791/z4000ac019878.html
        static_assert(sizeof(value) == sizeof(UInt) &&
                      std::numeric_limits<long double>::max_exponent == 16384,
                      "binary128 long double required");
        UInt bits;
        std::memcpy(&bits, &value, sizeof bits);
        const bool negative = (bits >> 127) != 0;
        const int exponent = static_cast<int>((bits >> 112) & 0x7fff);
        const UInt fraction = bits & ((UInt(1) << 112) - 1);
        const UInt discardedMask = (UInt(1) << 49) - 1;
        if (exponent == 0x7fff) {
            if (!fraction) *this = infinity(negative);
            else {
                if (!(fraction & (UInt(1) << 111))) std::feraiseexcept(FE_INVALID);
                // Keep the leading payload bits and a sticky bit, then quiet the converted NaN.
                const std::uint64_t payload = static_cast<std::uint64_t>(fraction >> 49) |
                    ((fraction & discardedMask) != 0) | (1ULL << 62);
                *this = raw(negative, payload, special);
            }
        } else {
            const UInt sig = exponent ? fraction | (UInt(1) << 112) : fraction;
            *this = pack(negative, sig, (exponent ? exponent - 16383 : -16382) - 112);
            if (fraction & discardedMask) {
                // Conversion detects tininess after rounding. Exact subnormals raise no underflow.
                const bool tiny = isFinite() && (!significand_ || exponent_ < -16382);
                std::feraiseexcept(FE_INEXACT | (tiny ? FE_UNDERFLOW : 0));
            }
        }
#else
        *this = ExtendedFloat(static_cast<double>(value));
#endif
    }
    explicit operator float() const { return narrow<float, std::uint32_t, 24, 127>(); }
    explicit operator double() const { return narrow<double, std::uint64_t, 53, 1023>(); }
    bool isNaN() const { return exponent_ == special && significand_ != 0; }
    bool isInf() const { return exponent_ == special && significand_ == 0; }
    bool isFinite() const { return exponent_ != special; }
    static ExtendedFloat infinity(bool negative = false) { return raw(negative, 0, special); }
    static ExtendedFloat invalid()
    { std::feraiseexcept(FE_INVALID); return raw(true, 1ULL << 62, special); }
    ExtendedFloat operator-() const { auto result = *this; result.negative_ = !negative_; return result; }
    friend ExtendedFloat abs(ExtendedFloat value) { value.negative_ = false; return value; }
    friend bool operator==(ExtendedFloat a, ExtendedFloat b)
    {
        if (a.isNaN() || b.isNaN()) return false;
        if (!a.isInf() && !b.isInf() && !a.significand_ && !b.significand_) return true;
        return a.negative_ == b.negative_ && a.exponent_ == b.exponent_ && a.significand_ == b.significand_;
    }
    friend bool operator!=(ExtendedFloat a, ExtendedFloat b) { return !(a == b); }
    friend bool operator<(ExtendedFloat a, ExtendedFloat b)
    {
        if (a.isNaN() || b.isNaN()) { std::feraiseexcept(FE_INVALID); return false; }
        if (a == b) return false;
        if (a.negative_ != b.negative_) return a.negative_;
        const int ae = !a.significand_ && !a.isInf() ? -32768 : a.exponent_;
        const int be = !b.significand_ && !b.isInf() ? -32768 : b.exponent_;
        return a.negative_ ? (ae > be || (ae == be && a.significand_ > b.significand_)) :
            (ae < be || (ae == be && a.significand_ < b.significand_));
    }
    friend bool operator>(ExtendedFloat a, ExtendedFloat b) { return b < a; }
    friend bool operator<=(ExtendedFloat a, ExtendedFloat b) { return a < b || a == b; }
    friend bool operator>=(ExtendedFloat a, ExtendedFloat b) { return a > b || a == b; }
    friend ExtendedFloat operator+(ExtendedFloat a, ExtendedFloat b)
    {
        if (a.isNaN()) return a;
        if (b.isNaN()) return b;
        if (a.isInf() || b.isInf()) return a.isInf() && b.isInf() && a.negative_ != b.negative_ ? invalid() : a.isInf() ? a : b;
        if (!a.significand_ && !b.significand_) return raw(a.negative_ == b.negative_ ? a.negative_ : std::fegetround() == FE_DOWNWARD, 0, 0);
        if (!a.significand_) return b;
        if (!b.significand_) return a;
        if (abs(a) < abs(b)) { auto t = a; a = b; b = t; }
        const UInt first = UInt(a.significand_) << 3;
        const UInt second = jam(UInt(b.significand_) << 3, a.exponent_ - b.exponent_);
        if (a.negative_ != b.negative_ && first == second) return raw(std::fegetround() == FE_DOWNWARD, 0, 0);
        return pack(a.negative_, a.negative_ == b.negative_ ? first + second : first - second, a.exponent_ - 66);
    }
    friend ExtendedFloat operator-(ExtendedFloat a, ExtendedFloat b) { return a + -b; }
    friend ExtendedFloat operator*(ExtendedFloat a, ExtendedFloat b)
    {
        if (a.isNaN()) return a;
        if (b.isNaN()) return b;
        if (a.isInf() || b.isInf()) return (!a.isInf() && !a.significand_) || (!b.isInf() && !b.significand_) ? invalid() : infinity(a.negative_ != b.negative_);
        return pack(a.negative_ != b.negative_, UInt(a.significand_) * b.significand_, a.exponent_ + b.exponent_ - 126);
    }
    friend ExtendedFloat operator/(ExtendedFloat a, ExtendedFloat b)
    {
        if (a.isNaN()) return a;
        if (b.isNaN()) return b;
        const bool sign = a.negative_ != b.negative_;
        if (a.isInf() && b.isInf()) return invalid();
        if (a.isInf()) return infinity(sign);
        if (b.isInf()) return raw(sign, 0, 0);
        if (!b.significand_) {
            if (!a.significand_) return invalid();
            std::feraiseexcept(FE_DIVBYZERO); return infinity(sign);
        }
        UInt numerator = UInt(a.significand_) << 63;
        UInt quotient = numerator / b.significand_, remainder = numerator % b.significand_;
        for (int i = 0; i < 3; ++i) {
            remainder <<= 1; quotient <<= 1;
            if (remainder >= b.significand_) { remainder -= b.significand_; ++quotient; }
        }
        return pack(sign, quotient | (remainder != 0), a.exponent_ - b.exponent_ - 66);
    }
    ExtendedFloat& operator+=(ExtendedFloat value) { return *this = *this + value; }
    template<class T> T integer(bool truncate) const
    {
        static_assert(std::numeric_limits<T>::is_signed && std::numeric_limits<T>::is_integer, "signed FIST destination required");
        if (!isFinite() || exponent_ >= std::numeric_limits<T>::digits + 1) {
            std::feraiseexcept(FE_INVALID); return std::numeric_limits<T>::min();
        }
        const int shift = 63 - exponent_;
        const UInt magnitude = truncate ? (shift >= 128 ? 0 : shift > 0 ? UInt(significand_) >> shift : UInt(significand_) << -shift) :
            rounded(negative_, significand_, shift);
        const UInt bound = UInt(1) << std::numeric_limits<T>::digits;
        if (magnitude > bound || (magnitude == bound && !negative_)) {
            std::feraiseexcept(FE_INVALID); return std::numeric_limits<T>::min();
        }
        if (magnitude == bound) return std::numeric_limits<T>::min();
        const T value = static_cast<T>(magnitude); return negative_ ? -value : value;
    }
};
} // namespace bf2x87
#endif
