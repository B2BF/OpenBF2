#include <ios>
#include "IAIPathfinding.h"

// IAIPathfindingSetup lives in IAIPathfindingSetup.cpp (AIDLL_r __FILE__).
namespace {
// Local object name [INFERRED].
// static init: ELF 0x3E6990 (__tcf_0 0x3E69F0, _GLOBAL__I 0x3E6A20), IAIPathfinding.cpp.
std::ios_base::Init pathfindingStreamInit;
}

namespace dice {
namespace hfe {
namespace ai {

IAIPathfinding* IAIPathfinding::instance = 0;

// Unconditionally replaces the pathfinding singleton. This interface uses no ClassManager registration.
IAIPathfinding::IAIPathfinding()
{
    instance = this;
}

// Clears the singleton unconditionally, even when destroying a displaced instance.
IAIPathfinding::~IAIPathfinding()
{
    instance = 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
