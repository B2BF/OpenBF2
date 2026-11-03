// EntryPlaneMoveTo: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEMOVETO_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEMOVETO_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryPlaneMoveTo - statement type 7.
class EntryPlaneMoveTo : public BAPEntry
{
public:
    EntryPlaneMoveTo();
    virtual ~EntryPlaneMoveTo();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEMOVETO_H
