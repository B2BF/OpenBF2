// [INFERRED] Source placement.

#include "UrgeCurve.h"
#include <iostream>
// Original UCLinear.cpp and UCXInverse.cpp are combined here; both only initialized iostreams.
// static init: ELF 0x226350, 0x2263B0, 0x2263E0
// static init: ELF 0x226530, 0x226590, 0x2265C0

#include <algorithm>            // std::max - see the argument-order note in the header

namespace dice {
namespace hfe {
namespace ai {

// The four destructors. Every one of them is thirty bytes that do exactly one thing: restore the
// vptr to the base's vtable. There is no member to release - the curves hold only floats - so the
// source bodies are empty and these are the compiler's own epilogues.

UrgeCurve::~UrgeCurve()
{
}

UCConstant::~UCConstant()
{
}

UCLinear::~UCLinear()
{
}

UCXInverse::~UCXInverse()
{
}

// UCConstant - , , and , .

UCConstant::UCConstant(float value)
    : m_value(value)
{
}

// `mov eax, [ebp+this]; fld dword ptr [eax+4]; retn` - the stored value, unclamped.
float UCConstant::calculate(float)
{
    return m_value;
}

// UCLinear - , , and , .

UCLinear::UCLinear(float slope, float offset)
    : m_slope(slope)
    , m_offset(offset)
{
}

// Evaluates the linear curve and clamps the result at zero.
float UCLinear::calculate(float x)
{
    return std::max(m_slope * x + m_offset, 0.0f);
}

// UCXInverse - , , and , .

UCXInverse::UCXInverse(float numerator, float offset, float slope, float bias)
    : m_numerator(numerator)
    , m_offset(offset)
    , m_slope(slope)
    , m_bias(bias)
{
}

// Evaluates the inverse curve and clamps the result at zero.
float UCXInverse::calculate(float x)
{
    return std::max(m_numerator / (m_slope * x + m_offset) + m_bias, 0.0f);
}

} // namespace ai
} // namespace hfe
} // namespace dice
