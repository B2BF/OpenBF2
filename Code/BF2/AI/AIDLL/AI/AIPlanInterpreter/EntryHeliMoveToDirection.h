// EntryHeliMoveToDirection: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVETODIRECTION_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVETODIRECTION_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryHeliMoveToDirection - statement type 8.
class EntryHeliMoveToDirection : public BAPEntry
{
public:
    EntryHeliMoveToDirection();
    virtual ~EntryHeliMoveToDirection();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVETODIRECTION_H
