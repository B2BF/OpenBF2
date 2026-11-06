// An urgency merger that picks one of its generators at random, weighted by the squares of their
// active urgencies.
// Evidence: libaidll.so 1.5.3153 (ia-32). File RandomSquareMerger.cpp (FILE symbol; the AIBot
// directory is [INFERRED] from its neighbours, as the ledger files it); vtable 0x4B6D40: the
// destructors and updateActiveUrgency (+0x10) are its own, every other slot is UrgencyMerger's.
// Nothing in libaidll.so constructs one.

#ifndef AI_AIDLL_AI_AIBOT_RANDOMSQUAREMERGER_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_RANDOMSQUAREMERGER_H

#include "UrgencyMerger.h"

namespace dice {
namespace hfe {
namespace ai {

class RandomSquareMerger : public UrgencyMerger
{
public:
    // As UrgencyMerger.
    RandomSquareMerger(int count, float* weights, UrgencyGenerator** generators);
    virtual ~RandomSquareMerger();

    virtual float updateActiveUrgency(Bot* bot);                    // +0x10
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_RANDOMSQUAREMERGER_H
