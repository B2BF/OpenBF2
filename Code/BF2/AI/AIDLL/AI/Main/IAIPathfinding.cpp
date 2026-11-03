#include <ios>
#include "IAIPathfinding.h"

namespace {
// The two original translation units are combined here; local object names are [INFERRED].
// static init: ELF 0x3E6990 (__tcf_0 0x3E69F0, _GLOBAL__I 0x3E6A20), IAIPathfinding.cpp.
std::ios_base::Init pathfindingStreamInit;
// static init: ELF 0x3E6B40 (__tcf_0 0x3E6BA0, _GLOBAL__I 0x3E6BD0), IAIPathfindingSetup.cpp.
std::ios_base::Init pathfindingSetupStreamInit;
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

// Setup has its own singleton pointer, replaced on construction and cleared on destruction.
// An AIPathfinding instance publishes both interfaces.
IAIPathfindingSetup* IAIPathfindingSetup::instance = 0;

IAIPathfindingSetup::IAIPathfindingSetup()
{
    instance = this;
}

IAIPathfindingSetup::~IAIPathfindingSetup()
{
    instance = 0;
}

// Stateless map-container interface with no registry entry or custom lifecycle work.
IAIPathfindingSetup::IMapContainer::IMapContainer()
{
}

IAIPathfindingSetup::IMapContainer::~IMapContainer()
{
}

} // namespace ai
} // namespace hfe
} // namespace dice
