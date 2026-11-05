// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\Main\IBotManager.cpp" (constructor 0x100CE370).
// IBotManagerModifier lives in IBotManagerModifier.cpp (AIDLL_r __FILE__).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining IBotManager operations.

#include "IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

IBotManager* IBotManager::instance = 0;

// Each interface publishes its singleton unconditionally and clears it on destruction.
// Destroying a displaced manager can clear the pointer to a newer instance.

IBotManager::IBotManager()
{
    instance = this;
}

IBotManager::~IBotManager()
{
    instance = 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
