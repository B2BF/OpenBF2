// Medium-LOD boat movement entry.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOMEDIUMBOAT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOMEDIUMBOAT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryMoveToMediumBoat - statement type 7.
class EntryMoveToMediumBoat : public BAPEntry
{
public:
    EntryMoveToMediumBoat();
    virtual ~EntryMoveToMediumBoat();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOMEDIUMBOAT_H
