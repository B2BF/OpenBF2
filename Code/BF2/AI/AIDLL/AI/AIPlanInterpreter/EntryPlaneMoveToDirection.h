// EntryPlaneMoveToDirection: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEMOVETODIRECTION_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEMOVETODIRECTION_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryPlaneMoveToDirection - statement type 8.
class EntryPlaneMoveToDirection : public BAPEntry
{
public:
    EntryPlaneMoveToDirection();
    virtual ~EntryPlaneMoveToDirection();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEMOVETODIRECTION_H
