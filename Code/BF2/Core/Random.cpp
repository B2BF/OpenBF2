// Random state and distribution serialization. [INFERRED] Core placement; AI FILE symbol: Random.cpp.

// static init (each image's own Random.cpp): iostream Init, then Rand.
//   bf2 ELF 0x83CB610 (seed store 0x83CB642); libaidll ELF 0x48A850 (__tcf_0 0x48A8D0, _GLOBAL__I 0x48A900).
#include <iostream>

#include "Random.h"

#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace dice {
namespace hfe {

// Each image owns one instance under the same symbol, dice::hfe::Rand: bf2 ds:0x8ABBCB0 and
// libaidll BSS 0x4DD454. The initializers store Random's default seed (0x759DC150).
Random Rand;

#if !defined(BF2_IMAGE_AIDLL)
// [CHOSEN, not recovered] Image partition: these distribution codecs belong to the engine;
// the original AI Random.cpp contributes its independent state and initialization only.

// ELF 0x83CFC50 (87 B), PE 0x5C10D0 (91 B).  The default arm covers CRD_NONE and every invalid
// selector; do not print an unknown integer numerically.
std::ostream& operator<<(std::ostream& stream, ContRandDist distribution)
{
    switch (distribution)
    {
    case CRD_UNIFORM:
        stream << "CRD_UNIFORM";
        break;
    case CRD_EXPONENTIAL:
        stream << "CRD_EXPONENTIAL";
        break;
    case CRD_NORMAL:
        stream << "CRD_NORMAL";
        break;
    default:
        stream << "CRD_NONE";
        break;
    }
    return stream;
}

// ELF 0x83CFDA0 / PE 0x5C1140.  Default ostream formatting is part of the console grammar:
// enum-token/value/variation/relative, with relative emitted by ostream's bool overload as 0/1.
std::ostream& operator<<(std::ostream& stream, const RandomDistribution& distribution)
{
    stream << distribution.distribution << '/'
           << distribution.value << '/'
           << distribution.variation << '/'
           << distribution.relative;
    return stream;
}

// ELF 0x83CFE40 / PE 0x5C11B0 is a second, independently emitted overload with the same body.
std::ostream& operator<<(std::ostream& stream, RandomDistribution& distribution)
{
    stream << distribution.distribution << '/'
           << distribution.value << '/'
           << distribution.variation << '/'
           << distribution.relative;
    return stream;
}

// ELF 0x83CFCB0 (242 B), PE 0x5C1220 (180 B).  Unknown tokens deliberately leave the value at
// CRD_NONE rather than putting the stream into a failed state.
std::istream& operator>>(std::istream& stream, ContRandDist& distribution)
{
    std::string value;
    stream >> value;

    distribution = CRD_NONE;
    if (value == "CRD_NONE")
        distribution = CRD_NONE;
    else if (value == "CRD_UNIFORM")
        distribution = CRD_UNIFORM;
    else if (value == "CRD_EXPONENTIAL")
        distribution = CRD_EXPONENTIAL;
    else if (value == "CRD_NORMAL")
        distribution = CRD_NORMAL;

    return stream;
}

// Reads the distribution name through the first slash, then its parameters.
// Remaining separator characters are consumed without validation.
std::istream& operator>>(std::istream& stream, RandomDistribution& distribution)
{
    char distributionText[64];
    stream.getline(distributionText, 64, '/');

    const std::string text(distributionText);
    std::istringstream distributionStream(text);
    distributionStream >> distribution.distribution;

    char separator;
    stream >> distribution.value
           >> separator
           >> distribution.variation
           >> separator
           >> distribution.relative;
    return stream;
}

#endif

} // namespace hfe
} // namespace dice
