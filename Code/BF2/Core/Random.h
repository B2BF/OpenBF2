// Shared random state and distribution parameters. [INFERRED] Source placement.
// Random uses one word of state; RandomDistribution occupies 20 bytes on x86.

#ifndef CORE_RANDOM_H
#define CORE_RANDOM_H

#include <iosfwd>

namespace dice {
namespace hfe {

// Values and spellings are attested by the ContRandDist stream writer at ELF 0x83CFC50 / PE
// 0x5C10D0 and its matching reader at ELF 0x83CFCB0.  Every other integer prints as CRD_NONE.
enum ContRandDist : int
{
    CRD_NONE        = 0,
    CRD_UNIFORM     = 1,
    CRD_EXPONENTIAL = 2,
    CRD_NORMAL      = 3
};

class Random
{
public:
    Random() : m_seed(0x759DC150u) {}

    unsigned int getRaw();
    float getNormal();

    unsigned int m_seed;
};

struct RandomDistribution
{
    RandomDistribution()
        : distribution(static_cast<ContRandDist>(0))
        , value(1.0f)
        , variation(0.0f)
        , relative(false)
        , random()
    {
    }

    ContRandDist distribution;                 // +0x00
    float value;                               // +0x04
    float variation;                           // +0x08
    bool relative;                             // +0x0C; +0x0D..+0x0F natural padding
    Random random;                             // +0x10
};

// Three distinct shipped symbols.  RandomDistribution has both const and non-const writer
// overloads; their bodies deliberately serialize the same four slash-separated fields.
std::ostream& operator<<(std::ostream& stream, ContRandDist distribution);
std::ostream& operator<<(std::ostream& stream, const RandomDistribution& distribution);
std::ostream& operator<<(std::ostream& stream, RandomDistribution& distribution);
std::istream& operator>>(std::istream& stream, ContRandDist& distribution);
std::istream& operator>>(std::istream& stream, RandomDistribution& distribution);

static_assert(sizeof(Random) == 4, "Random must retain its shipped x86 layout");
static_assert(sizeof(RandomDistribution) == 0x14,
              "RandomDistribution must retain its shipped x86 layout");

// The image's random instance, defined in Random.cpp: dice::hfe::Rand in both retail images
// (bf2 ds:0x8ABBCB0, libaidll BSS 0x4DD454); the engine and the AI DLL each have their own.
extern Random Rand;

} // namespace hfe
} // namespace dice

#endif // CORE_RANDOM_H
