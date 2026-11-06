// Boat movement entry for BAPAMoveTo.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETO_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETO_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryBoatMoveTo - statement type 7.
class EntryBoatMoveTo : public BAPEntry
{
public:
    EntryBoatMoveTo();
    virtual ~EntryBoatMoveTo();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYBOATMOVETO_H
