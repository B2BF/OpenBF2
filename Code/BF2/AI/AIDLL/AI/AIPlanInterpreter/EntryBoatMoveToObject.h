// Boat movement entry for an object target.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETOOBJECT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETOOBJECT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryBoatMoveToObject - statement type 9.
class EntryBoatMoveToObject : public BAPEntry
{
public:
    EntryBoatMoveToObject();
    virtual ~EntryBoatMoveToObject();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETOOBJECT_H
