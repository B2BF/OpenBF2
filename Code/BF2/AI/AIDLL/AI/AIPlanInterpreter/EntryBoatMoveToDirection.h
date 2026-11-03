// EntryBoatMoveToDirection: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETODIRECTION_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETODIRECTION_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryBoatMoveToDirection - statement type 8.
class EntryBoatMoveToDirection : public BAPEntry
{
public:
    EntryBoatMoveToDirection();
    virtual ~EntryBoatMoveToDirection();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETODIRECTION_H
