// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\Main\IBotManagerModifier.cpp" (the
// constructor 0x100DB450 asserts "instance == 0", "IBotManagerModifier is singleton"; asserts are
// compiled out of retail 1.50). Split out of IBotManager.cpp; the functions are unchanged.

#include "IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

IBotManagerModifier* IBotManagerModifier::instance = 0;

// Publishes its singleton unconditionally and clears it on destruction, like IBotManager.
IBotManagerModifier::IBotManagerModifier()
{
    instance = this;
}

IBotManagerModifier::~IBotManagerModifier()
{
    instance = 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
