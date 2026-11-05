// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\Main\IAIPathfindingSetup.cpp" (the
// constructor 0x100CE290 asserts "!instance", "Singleton already created"; asserts are compiled out
// of retail 1.50). Split out of IAIPathfinding.cpp; the functions are unchanged.
#include <ios>
#include "IAIPathfinding.h"

namespace {
// Local object name [INFERRED].
// static init: ELF 0x3E6B40 (__tcf_0 0x3E6BA0, _GLOBAL__I 0x3E6BD0), IAIPathfindingSetup.cpp.
std::ios_base::Init pathfindingSetupStreamInit;
}

namespace dice {
namespace hfe {
namespace ai {

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
