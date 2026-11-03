// EntryPlaneAimAt: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEAIMAT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEAIMAT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryPlaneAimAt - statement type 6.
class EntryPlaneAimAt : public BAPEntry
{
public:
    EntryPlaneAimAt();
    virtual ~EntryPlaneAimAt();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEAIMAT_H
