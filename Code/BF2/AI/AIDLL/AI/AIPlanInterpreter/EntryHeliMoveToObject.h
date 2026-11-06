// EntryHeliMoveToObject: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVETOOBJECT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVETOOBJECT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryHeliMoveToObject - statement type 9.
class EntryHeliMoveToObject : public BAPEntry
{
public:
    EntryHeliMoveToObject();
    virtual ~EntryHeliMoveToObject();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYHELIMOVETOOBJECT_H
