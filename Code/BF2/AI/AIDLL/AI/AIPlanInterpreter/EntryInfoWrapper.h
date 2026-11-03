// EntryInfoWrapper: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYINFOWRAPPER_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYINFOWRAPPER_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryInfoWrapper - statement type 15.
class EntryInfoWrapper : public BAPEntry
{
public:
    EntryInfoWrapper();
    virtual ~EntryInfoWrapper();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYINFOWRAPPER_H
