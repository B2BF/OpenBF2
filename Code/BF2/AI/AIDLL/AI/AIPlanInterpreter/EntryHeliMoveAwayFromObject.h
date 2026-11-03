// EntryHeliMoveAwayFromObject: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVEAWAYFROMOBJECT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVEAWAYFROMOBJECT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryHeliMoveAwayFromObject - statement type 10.
class EntryHeliMoveAwayFromObject : public BAPEntry
{
public:
    EntryHeliMoveAwayFromObject();
    virtual ~EntryHeliMoveAwayFromObject();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVEAWAYFROMOBJECT_H
